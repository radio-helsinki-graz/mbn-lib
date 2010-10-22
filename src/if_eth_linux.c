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
#include <errno.h>

#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_arp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <linux/sockios.h>

#include "mbn.h"

#define ETH_P_DNR  0x8820
#define BUFFERSIZE 512
#define ADDLSTSIZE 1000 /* assume we don't have more than 1000 nodes on ethernet */



struct ethdat {
  int socket;
  int ifindex;
  unsigned char address[6];
  unsigned char macs[ADDLSTSIZE][6];
  pthread_t thread;
};

int ethernet_init(struct mbn_interface *, char *);
void *receive_packets(void *);
void ethernet_stop(struct mbn_interface *itf);
void ethernet_free(struct mbn_interface *);
void ethernet_free_addr(struct mbn_interface *, void *);
int transmit(struct mbn_interface *, unsigned char *, int, void *, char *);


/* fetch a list of ethernet interfaces */
struct mbn_if_ethernet * MBN_EXPORT mbnEthernetIFList(char *err) {
  struct ifaddrs *list, *a;
  struct mbn_if_ethernet *e, *n, *l;
  struct sockaddr_ll *mac;

  e = NULL;
  if(getifaddrs(&list) != 0) {
    sprintf(err, "getifaddrs() failed: %s", strerror(errno));
    return e;
  }

  for(a=list; a != NULL; a=a->ifa_next) {
    if(a->ifa_addr == NULL || !(a->ifa_flags & IFF_UP) || a->ifa_addr->sa_family != AF_PACKET)
      continue;
    mac = (struct sockaddr_ll *)a->ifa_addr;
    if(mac->sll_hatype != ARPHRD_ETHER)
      continue;
    n = calloc(1, sizeof(struct mbn_if_ethernet));
    n->name = malloc(strlen(a->ifa_name)+1);
    memcpy((void *)n->name, (void *)a->ifa_name, strlen(a->ifa_name)+1);
    memcpy((void *)n->addr, (void *)mac->sll_addr, 6);
    if(e == NULL)
      e = n;
    else
      l->next = n;
    l = n;
  }
  if(e == NULL)
    sprintf(err, "No interfaces found");
  if(list != NULL)
    freeifaddrs(list);

  return e;
}


void MBN_EXPORT mbnEthernetIFFree(struct mbn_if_ethernet *list) {
  struct mbn_if_ethernet *n;
  while(list != NULL) {
    n = list->next;
    free(list->name);
    free(list);
    list = n;
  }
}


struct mbn_interface * MBN_EXPORT mbnEthernetOpen(char *interface, char *err) {
  struct ethdat *data;
  struct mbn_interface *itf;
  struct ifreq ethreq;
  int error = 0;
  struct sockaddr_ll sockaddr;

  memset(&sockaddr, 0, sizeof(struct sockaddr_ll));

  if(interface == NULL) {
    sprintf(err, "No interface specified");
    return NULL;
  }

  itf = (struct mbn_interface *) calloc(1, sizeof(struct mbn_interface));
  data = (struct ethdat *) calloc(1, sizeof(struct ethdat));
  itf->data = (void *) data;

  /* create a socket
   * Note we use ETH_P_ALL here, because that's the only way to receive
   *  outgoing packets from other processes as well as incoming packets
   *  from the network. */
  data->socket = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL));
  if(data->socket < 0) {
    sprintf(err, "socket(): %s", strerror(errno));
    error++;
  }

  /* look for interface */
  strncpy(ethreq.ifr_name, interface, IFNAMSIZ);
  if(!error && ioctl(data->socket, SIOCGIFFLAGS, &ethreq) < 0) {
    sprintf(err, "Couldn't find interface: %s", strerror(errno));
    error++;
  }

  /* fetch interface index */
  if(!error && ioctl(data->socket, SIOCGIFINDEX, &ethreq) < 0) {
    sprintf(err, "Couldn't get index: %s", strerror(errno));
    error++;
  } else
    data->ifindex = ethreq.ifr_ifindex;

  /* fetch MAC address */
  if(!error && ioctl(data->socket, SIOCGIFHWADDR, &ethreq) < 0) {
    sprintf(err, "Couldn't MAC address: %s", strerror(errno));
    error++;
  } else
    memcpy(data->address, ethreq.ifr_hwaddr.sa_data, 6);

  /* bind socket with the interface */
  sockaddr.sll_family = AF_PACKET;
  sockaddr.sll_protocol = htons(ETH_P_ALL);
  sockaddr.sll_ifindex = data->ifindex;
  if(!error && bind(data->socket, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_ll)) < 0) {
    sprintf(err, "Couldn't bind socket: %s", strerror(errno));
    error++;
  }

  /* something went wrong in the above statements */
  if(error) {
    close(data->socket);
    free(itf);
    free(data);
    return NULL;
  }

  itf->cb_init = ethernet_init;
  itf->cb_stop = ethernet_stop;
  itf->cb_free = ethernet_free;
  itf->cb_free_addr = ethernet_free_addr;
  itf->cb_transmit = transmit;

  return itf;
}


int ethernet_init(struct mbn_interface *itf, char *err) {
  struct ethdat *dat = (struct ethdat *)itf->data;
  int i;

  /* create thread to wait for packets */
  if((i = pthread_create(&(dat->thread), NULL, receive_packets, (void *) itf)) != 0) {
    sprintf(err, "Can't create thread: %s (%d)", strerror(i), i);
    return 1;
  }
  return 0;
}


void ethernet_stop(struct mbn_interface *itf) {
  struct ethdat *dat = (struct ethdat *)itf->data;
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);
}

void ethernet_free(struct mbn_interface *itf) {
  struct ethdat *dat = (struct ethdat *)itf->data;
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);
  free(dat);
  free(itf);
}


void ethernet_free_addr(struct mbn_interface *itf, void *arg) {
  mbnWriteLogMessage(itf, "Remove Ethernet address %02X:%02X:%02X:%02X:%02X:%02X", ((unsigned char *)arg)[0],
                                                                                   ((unsigned char *)arg)[1],
                                                                                   ((unsigned char *)arg)[2],
                                                                                   ((unsigned char *)arg)[3],
                                                                                   ((unsigned char *)arg)[4],
                                                                                   ((unsigned char *)arg)[5]);
  memset(arg, 0, 6);
}


/* Waits for input from network */
void *receive_packets(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct ethdat *dat = (struct ethdat *) itf->data;
  unsigned char buffer[BUFFERSIZE], msgbuf[MBN_MAX_MESSAGE_SIZE];
  char err[MBN_ERRSIZE];
  int i, j, msgbuflen = 0;
  fd_set rdfd;
  struct timeval tv;
  struct sockaddr_ll from;
  ssize_t rd;
  void *ifaddr, *hwaddr;
  socklen_t addrlength = sizeof(struct sockaddr_ll);

  while(1) {
    /* we can safely cancel here */
    pthread_testcancel();

    /* check for incoming data */
    FD_ZERO(&rdfd);
    FD_SET(dat->socket, &rdfd);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    rd = select(dat->socket+1, &rdfd, NULL, NULL, &tv);
    if(rd == 0 || (rd < 0 && errno == EINTR))
      continue;
    if(rd < 0) {
      sprintf(err, "Couldn't check for new packets: %s", strerror(errno));
      mbnInterfaceReadError(itf, err);
      break;
    }

    /* read incoming data */
    rd = recvfrom(dat->socket, buffer, BUFFERSIZE, 0, (struct sockaddr *)&from, &addrlength);
    if(rd == 0 || (rd < 0 && errno == EINTR))
      continue;
    if(rd < 0) {
      sprintf(err, "Couldn't receive packet: %s", strerror(errno));
      mbnInterfaceReadError(itf, err);
      break;
    }
    if(htons(from.sll_protocol) != ETH_P_DNR)
      continue;

    /* handle the data */
    for(i=0; i<rd; i++) {
      /* ignore non-start bytes if we haven't started yet */
      if(msgbuflen == 0 && !(buffer[i] >= 0x80 && buffer[i] < 0xFF))
        continue;
      msgbuf[msgbuflen++] = buffer[i];
      /* we have a full message, send buffer to mambanet stack for processing */
      if(buffer[i] == 0xFF) {
        if(msgbuflen >= MBN_MIN_MESSAGE_SIZE) {
          /* get HW address pointer from mbn */
          hwaddr = ifaddr = NULL;
          for(j=0; j<ADDLSTSIZE-1; j++) {
            if(hwaddr == NULL && memcmp(dat->macs[j], "\0\0\0\0\0\0", 6) == 0)
              hwaddr = dat->macs[j];
            if(memcmp(dat->macs[j], (void *)from.sll_addr, 6) == 0) {
              ifaddr = dat->macs[j];
              break;
            }
          }
          if(ifaddr == NULL) {
            ifaddr = hwaddr;
            memcpy(ifaddr, (void *)from.sll_addr, 6);

            mbnWriteLogMessage(itf, "Add Ethernet address %02X:%02X:%02X:%02X:%02X:%02X", ((unsigned char *)hwaddr)[0],
                                                                                          ((unsigned char *)hwaddr)[1],
                                                                                          ((unsigned char *)hwaddr)[2],
                                                                                          ((unsigned char *)hwaddr)[3],
                                                                                          ((unsigned char *)hwaddr)[4],
                                                                                          ((unsigned char *)hwaddr)[5]);
          }
          mbnProcessRawMessage(itf, msgbuf, msgbuflen, ifaddr);
        }
        msgbuflen = 0;
      }
      /* message was way too long, ignore it */
      if(msgbuflen >= MBN_MAX_MESSAGE_SIZE)
        msgbuflen = 0;
    }
  }

  return NULL;
}


int transmit(struct mbn_interface *itf, unsigned char *buffer, int length, void *ifaddr, char *err) {
  struct ethdat *dat = (struct ethdat *) itf->data;
  unsigned char *addr = (unsigned char *) ifaddr;
  struct sockaddr_ll saddr;
  int rd, sent;

  /* fill sockaddr struct */
  memset((void *)&saddr, 0, sizeof(struct sockaddr_ll));
  saddr.sll_family   = AF_PACKET;
  saddr.sll_protocol = htons(ETH_P_DNR);
  saddr.sll_ifindex  = dat->ifindex;
  saddr.sll_hatype   = ARPHRD_ETHER;
  saddr.sll_pkttype  = PACKET_OTHERHOST;
  saddr.sll_halen    = ETH_ALEN;
  if(addr != NULL)
    memcpy(saddr.sll_addr, addr, 6);
  else
    memset(saddr.sll_addr, 0xFF, 6);

  /* send data */
  sent = 0;
  while((rd = sendto(dat->socket, &(buffer[sent]), length-sent, 0, (struct sockaddr *)&saddr, sizeof(struct sockaddr_ll))) < length-sent) {
    if(rd < 0) {
      sprintf(err, "Can't send packet: %s", strerror(errno));
      return 1;
    }
    sent += rd;
  }
  return 0;
}

char MBN_EXPORT mbnEthernetMIILinkStatus(struct mbn_interface *itf, char *err) {
  struct ethdat *dat = (struct ethdat *)itf->data;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_ifindex = dat->ifindex;

  if (ioctl(dat->socket, SIOCGIFNAME, &ifr) == -1) {
    sprintf(err, "SIOCGIFNAME failed: %s", strerror(errno));
    return -1;
  }
  if (ioctl(dat->socket, SIOCGMIIPHY, &ifr) == -1) {
    sprintf(err, "SIOCGMIIPHY failed: %s", strerror(errno));
    return -1;
  }

  ((unsigned short*) &ifr.ifr_data)[1] = 1;

  if (ioctl(dat->socket, SIOCGMIIREG, &ifr) == -1) {
    sprintf(err, "SIOCGMIIREG failed: %s", strerror(errno));
    return -1;
  }

  return (((unsigned short*) &ifr.ifr_data)[3] & 0x0004) ? 1 : 0;
}

