/****************************************************************************
**
** Copyright (C) 2009 D&R Electronica Weesp B.V. All rights reserved.
**
** This file is part of the Axum/MambaNet digital mixing system.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_arp.h>

#include "mbn.h"

#define ETH_P_DNR  0x8820
#define BUFFERSIZE 512


struct mbn_ethernet_data {
  int socket;
  int ifindex;
  unsigned char address[6];
  pthread_t thread;
};

void *receive_packets(void *ptr);


/* Creates a new ethernet interface and links it to the mbn handler */
int MBN_EXPORT mbnEthernetInit(struct mbn_handler *mbn, char *interface) {
  struct mbn_ethernet_data *data;
  struct ifreq ethreq;
  int error = 0;
  struct sockaddr_ll sockaddr;

  memset(&(mbn->interface), 0, sizeof(struct mbn_interface));

  data = (struct mbn_ethernet_data *) malloc(sizeof(struct mbn_ethernet_data));
  mbn->interface.data = (void *) data;

  /* create a socket
   * Note we use ETH_P_ALL here, because that's the only way to receive
   *  outgoing packets from other processes as well as incoming packets
   *  from the network. */
  data->socket = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
  if(data->socket < 0) {
    perror("Couldn't create socket");
    error++;
  }

  /* look for interface */
  strncpy(ethreq.ifr_name, interface, IFNAMSIZ);
  if(!error && ioctl(data->socket, SIOCGIFFLAGS, &ethreq) < 0) {
    perror("Couldn't find network interface");
    error++;
  }

  /* fetch interface index */
  if(!error && ioctl(data->socket, SIOCGIFINDEX, &ethreq) < 0) {
    perror("Couldn't get index of network interface");
    error++;
  } else
    data->ifindex = ethreq.ifr_ifindex;

  /* fetch MAC address */
  if(!error && ioctl(data->socket, SIOCGIFHWADDR, &ethreq) < 0) {
    perror("Couldn't get MAC address");
    error++;
  } else
    memcpy(data->address, ethreq.ifr_hwaddr.sa_data, 6);

  /* bind socket with the interface */
  sockaddr.sll_family = AF_PACKET;
  sockaddr.sll_protocol = htons(ETH_P_ALL);
  sockaddr.sll_ifindex = data->ifindex;
  if(!error && bind(data->socket, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_ll)) < 0) {
    perror("Error binding socket");
    error++;
  }

  /* create thread to wait for packets */
  if(!error && pthread_create(&(data->thread), NULL, receive_packets, (void *) mbn) != 0) {
    perror("Error creating thread");
    error++;
  }

  /* something went wrong in the above statements */
  if(error) {
    close(data->socket);
    mbn->interface.data = NULL;
    free(data);
    return 1;
  }

  MBN_TRACE(printf("Listening on ethernet interface %s (index = %d, MAC = %02X:%02X:%02X:%02X:%02X:%02X)",
    interface, data->ifindex, data->address[0], data->address[1], data->address[2], data->address[3],
    data->address[4], data->address[5]));

  return 0;
}


/* Waits for input from network */
/* TODO: thread cancellation? */
void *receive_packets(void *ptr) {
  struct mbn_handler *mbn = (struct mbn_handler *) ptr;
  struct mbn_ethernet_data *eth = (struct mbn_ethernet_data *) mbn->interface.data;
  unsigned char buffer[BUFFERSIZE], msgbuf[MBN_MAX_MESSAGE_SIZE];
  int i, msgbuflen = 0;
  struct sockaddr_ll from;
  ssize_t rd;
  socklen_t addrlength = sizeof(struct sockaddr_ll);

  while((rd = recvfrom(eth->socket, buffer, BUFFERSIZE, 0, (struct sockaddr *)&from, &addrlength)) > 0) {
    if(htons(from.sll_protocol) != ETH_P_DNR)
      continue;

    for(i=0; i<rd; i++) {
      /* ignore non-start bytes if we haven't started yet */
      if(msgbuflen == 0 && !(buffer[i] >= 0x80 && buffer[i] < 0xFF))
        continue;
      msgbuf[msgbuflen++] = buffer[i];
      /* we have a full message, send buffer to mambanet stack for processing */
      if(buffer[i] == 0xFF) {
        if(msgbuflen >= MBN_MIN_MESSAGE_SIZE)
          mbnProcessRawMessage(mbn, msgbuf, msgbuflen);
        msgbuflen = 0;
      }
      /* message was way too long, ignore it */
      if(msgbuflen >= MBN_MAX_MESSAGE_SIZE)
        msgbuflen = 0;
    }
  }

  /* TODO: Notify application */
  if(rd < 0)
    perror("Can't read from socket");

  MBN_TRACE(printf("Closing the receiver thread..."));

  return NULL;
}



