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

#define _XOPEN_SOURCE 600
#define _XOPEN_SOURCE_EXTENDED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mbn.h"

#ifdef MBNP_linux
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <sys/time.h>
# include <netdb.h>
# include <arpa/inet.h>
#else
# define _WIN32_WINNT 0x0501 /* only works for ws2_32.dll > windows 2000 */
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# define close closesocket
#endif
#include <pthread.h>



#define MBN_UDP_PORT "34848"
#define BUFFERSIZE 512
#define ADDLSTSIZE 1000 /* assume we don't have more than 1000 nodes on UDP connections */

struct udpaddr {
  unsigned long addr;
  unsigned short port;
};

struct udpdat {
  int socket;
  unsigned long defaultaddr;
  unsigned short defaultport;
  struct udpaddr addr[ADDLSTSIZE];
  pthread_t thread;
};

int udp_init(struct mbn_interface *, char *);
void *udp_receive_packets(void *);
void udp_free(struct mbn_interface *);
void udp_free_addr(struct mbn_interface *, void *);
int udp_transmit(struct mbn_interface *, unsigned char *, int, void *, char *);


struct mbn_interface * MBN_EXPORT mbnUDPOpen(char *remotehost, char *remoteport, char *localport, char *err) {
  struct udpdat *data;
  struct mbn_interface *itf;
  int error = 0;
  struct sockaddr_in si_me;
  struct hostent *remoteserver;
  int port;

  /* Why the hell is this call _required_?
   * Why doesn't Microsoft just support plain BSD sockets? */
#ifdef MBNP_mingw
  WSADATA wsadat;
  if(WSAStartup(MAKEWORD(2, 0), &wsadat) != 0) {
    sprintf(err, "Unsupported winsock version");
    return NULL;
  }
#endif

  itf = (struct mbn_interface *) calloc(1, sizeof(struct mbn_interface));
  data = (struct udpdat *) calloc(1, sizeof(struct udpdat));
  itf->data = (void *) data;

  /* lookup hostname/ip address */
  data->defaultaddr = 0;
  if (remotehost != NULL) {
    remoteserver = gethostbyname(remotehost);
    if (remoteserver == NULL) {
      sprintf(err, "gethostbyname error: %s", strerror(errno));
      error++;
    }
    data->defaultaddr = *((unsigned long *)remoteserver->h_addr_list[0]);
  }

  if(remoteport == NULL)
    remoteport = MBN_UDP_PORT;
  if (sscanf(remoteport, "%d", &port) != 1) {
    error++;
  }
  data->defaultport = htons(port);

  /* create a socket */
  data->socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(data->socket < 0) {
    sprintf(err, "socket(): %s", strerror(errno));
    error++;
  }

  /* Client/Server installation of addresses */
  if (localport == NULL)
    localport = MBN_UDP_PORT;
  if (sscanf(localport, "%d", &port) != 1) {
    error++;
  }
  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(port);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if(bind(data->socket, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
    sprintf(err, "bind(): %s", strerror(errno));
    error++;
  }

  /* something went wrong in the above statements */
  if(error) {
    close(data->socket);
    free(itf);
    free(data);
#ifdef MBNP_mingw
    WSACleanup();
#endif
    return NULL;
  }

  itf->cb_init = udp_init;
  itf->cb_free = udp_free;
  itf->cb_free_addr = udp_free_addr;
  itf->cb_transmit = udp_transmit;

  return itf;
}


int udp_init(struct mbn_interface *itf, char *err) {
  struct udpdat *dat = (struct udpdat *)itf->data;
  int i;
  
  /* create thread to wait for packets */
  if((i = pthread_create(&(dat->thread), NULL, udp_receive_packets, (void *) itf)) != 0) {
    sprintf(err, "Can't create thread: %s (%d)", strerror(i), i);
    return 1;
  }
  return 0;
}


void udp_free(struct mbn_interface *itf) {
  struct udpdat *dat = (struct udpdat *)itf->data;

  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);
  free(dat);
  free(itf);
#ifdef MBNP_mingw
  WSACleanup();
#endif
}


void udp_free_addr(struct mbn_interface *itf, void *arg) {
  struct udpaddr *addr = arg;
  struct in_addr in;

  in.s_addr = addr->addr;
  mbnWriteLogMessage(itf, "Remove UDP interface using %s:%d", inet_ntoa(in), ntohs(addr->port));

  addr->addr = 0;
  addr->port = 0;
}


/* Waits for input from network */
void *udp_receive_packets(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct udpdat *dat = (struct udpdat *) itf->data;
  unsigned char buffer[BUFFERSIZE], msgbuf[MBN_MAX_MESSAGE_SIZE];
  char err[MBN_ERRSIZE];
  int i, j, msgbuflen = 0;
  fd_set rdfd;
  struct timeval tv;
  struct sockaddr_in from;
  ssize_t rd;
  void *ifaddr, *ipaddr;
  socklen_t addrlength = sizeof(struct sockaddr_in);

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
      printf("error\n");
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
    
    if(htons(from.sin_port) != 34848)
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
          ipaddr = ifaddr = NULL;
          for(j=0; j<ADDLSTSIZE-1; j++) {
            if((ipaddr == NULL) && (dat->addr[j].addr == 0))
              ipaddr = &dat->addr[j];
            if ((dat->addr[j].addr == from.sin_addr.s_addr) && (dat->addr[j].port == from.sin_port)) {
              ifaddr = &dat->addr[j];
              break;
            }
          }
          if(ifaddr == NULL) {
            ifaddr = ipaddr;
            ((struct udpaddr *)ifaddr)->addr = from.sin_addr.s_addr;
            ((struct udpaddr *)ifaddr)->port = from.sin_port;
            mbnWriteLogMessage(itf, "Add UDP interface using %s:%d", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
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


int udp_transmit(struct mbn_interface *itf, unsigned char *buffer, int length, void *ifaddr, char *err) {
  struct udpdat *dat = (struct udpdat *) itf->data;
  struct udpaddr *dest_udpaddr = (struct udpaddr *) ifaddr;
  struct sockaddr_in daddr;
  int rd, sent, i;
  char def_remote_done = 0;

  memset((void *)&daddr, 0, sizeof(struct sockaddr_in));
  daddr.sin_family   = AF_INET;

  if (dest_udpaddr == NULL) {
    for(i=0; i<ADDLSTSIZE; i++) {
      if (dat->addr[i].addr == 0)
        continue;
      
      daddr.sin_port = dat->addr[i].port;
      daddr.sin_addr.s_addr = dat->addr[i].addr;

      /* send data */
      sent = 0;
      while((rd = sendto(dat->socket, &(buffer[sent]), length-sent, 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_in))) < length-sent) {
        if(rd < 0) {
          sprintf(err, "Can't send packet: %s", strerror(errno));
          return 1;
        }
        sent += rd;
      }

      if ((daddr.sin_port == dat->defaultport) && (daddr.sin_addr.s_addr = dat->defaultaddr)) {
        def_remote_done = 1;
      }
    }
    /* broadcast done, clear transmit address */
    daddr.sin_port = 0;
    daddr.sin_addr.s_addr = 0;

    if (!def_remote_done) {
      daddr.sin_port = dat->defaultport;
      daddr.sin_addr.s_addr = dat->defaultaddr;
    }
  } else {
    daddr.sin_port = dest_udpaddr->port;
    daddr.sin_addr.s_addr = dest_udpaddr->addr;
  }

  if (daddr.sin_addr.s_addr != 0)
  {
    sent = 0;
    while((rd = sendto(dat->socket, &(buffer[sent]), length-sent, 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_in))) < length-sent) {
      if(rd < 0) {
        sprintf(err, "Can't send packet: %s", strerror(errno));
        return 1;
      }
      sent += rd;
    }
  }

  return 0;
}

