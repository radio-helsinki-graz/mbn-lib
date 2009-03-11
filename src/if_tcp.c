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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define _BSD_SOURCE /* inet_aton */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "mbn.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MBN_TCP_PORT 34848
/* the actual number of max. connections is also limited by the
 * number of sockets the select() call accepts.
 * Each connection requires about 128 bytes for buffers */
#define MAX_CONNECTIONS 64
#define BUFFERSIZE     512


struct tcpconn {
  unsigned char buf[MBN_MAX_MESSAGE_SIZE];
  int buflen;
  int sock; /* -1 when unused */
  struct sockaddr_in addr;
};

struct tcpdat {
  pthread_t thread;
  int listensocket;
  int rconn;
  struct tcpconn conn[MAX_CONNECTIONS];
  struct sockaddr_in addr;
};

int setup_client(struct tcpdat *, char *, int);
int setup_server(struct tcpdat *, char *, int);
void init_tcp(struct mbn_interface *);
void free_tcp(struct mbn_interface *);
void *receiver(void *);
void transmit(struct mbn_interface *, unsigned char *, int, void *);


struct mbn_interface * MBN_EXPORT mbnTCPOpen(char *remoteip, int remoteport, char *myip, int myport) {
  struct mbn_interface *itf;
  struct tcpdat *dat;
  int i, error = 0;

  itf = (struct mbn_interface *)calloc(1, sizeof(struct mbn_interface));
  dat = (struct tcpdat *)malloc(sizeof(struct tcpdat));
  itf->data = (void *)dat;

  /* initialize connection table */
  for(i=0; i<MAX_CONNECTIONS; i++)
    dat->conn[i].sock = -1;

  /* TODO: hostname lookups on remoteip and myip? */
  if(remoteip != NULL) {
    if(remoteport == 0)
      remoteport = MBN_TCP_PORT;
    error += setup_client(dat, remoteip, remoteport);
  } else
    dat->rconn = -1;

  if(!error && myip != NULL) {
    if(myport == 0)
      myport = MBN_TCP_PORT;
    error += setup_server(dat, myip, myport);
  } else
    dat->listensocket = -1;

  if(error) {
    free(dat);
    free(itf);
    return NULL;
  }

  itf->cb_init = init_tcp;
  itf->cb_free = free_tcp;
  itf->cb_transmit = transmit;
  return itf;
}


int setup_client(struct tcpdat *dat, char *server, int port) {
  /* create socket */
  if((dat->rconn = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Opening client socket");
    return 1;
  }
  dat->conn[0].sock = dat->rconn;
  dat->conn[0].buflen = 0;

  /* connect */
  dat->conn[0].addr.sin_family = AF_INET;
  dat->conn[0].addr.sin_port = htons(port);
  dat->conn[0].addr.sin_addr.s_addr = inet_addr(server);
  if(connect(dat->rconn, (struct sockaddr *)&(dat->conn[0].addr), sizeof(struct sockaddr_in)) < 0) {
    perror("Connecting to server");
    return 1;
  }

  return 0;
}


int setup_server(struct tcpdat *dat, char *ip, int port) {
  struct sockaddr_in myaddr;
  int n;

  /* create socket */
  if((dat->listensocket = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Opening server socket");
    return 1;
  }

  /* allow the address to be reused */
  if(setsockopt(dat->listensocket, SOL_SOCKET, SO_REUSEADDR, (void *)&n, sizeof(int)) < 0) {
    perror("Setting SO_REUSEADDR");
    return 1;
  }

  /* bind socket to an ip/port */
  memset((void *)&myaddr, 0, sizeof(struct sockaddr_in));
  myaddr.sin_family = AF_INET;
  myaddr.sin_port = htons(port);
  myaddr.sin_addr.s_addr = inet_addr(ip);
  if(bind(dat->listensocket, (struct sockaddr *)&myaddr, sizeof(struct sockaddr_in)) < 0) {
    perror("Bind server socket");
    return 1;
  }

  /* start listening */
  if(listen(dat->listensocket, 5) < 0) {
    perror("Listen()");
    return 1;
  }
  return 0;
}


void init_tcp(struct mbn_interface *itf) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  if(pthread_create(&(dat->thread), NULL, receiver, (void *) itf) != 0)
    perror("Error creating thread");
}


void free_tcp(struct mbn_interface *itf) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  int i;

  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);

  for(i=0; i<MAX_CONNECTIONS; i++)
    if(dat->conn[i].sock >= 0)
      close(dat->conn[i].sock);

  free(dat);
  free(itf);
}


void new_connection(struct tcpdat *dat) {
  int i;
  socklen_t n = sizeof(struct sockaddr_in);

  for(i=0; i<MAX_CONNECTIONS; i++)
    if(dat->conn[i].sock < 0)
      break;

  /* MAX_CONNECTIONS reached, just close the connection */
  if(i >= MAX_CONNECTIONS) {
    if((i = accept(dat->listensocket, NULL, 0)) > 0)
      close(i);
    return;
  }

  /* accept the connection */
  if((dat->conn[i].sock = accept(dat->listensocket, (struct sockaddr *)&(dat->conn[i].addr), &n)) < 0)
    return;
  dat->conn[i].buflen = 0;
}


void read_connection(struct mbn_interface *itf, struct tcpconn *cn) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  unsigned char buf[BUFFERSIZE];
  int n, i;

  n = recv(cn->sock, (char *)buf, BUFFERSIZE, 0);
  if(n < 0 && errno == EINTR)
    return;

  /* error, close connection */
  if(n == 0 || n < 0) {
    close(cn->sock);
    /* oops, this was our remote connection, we shouldn't lose this one! */
    if(dat->rconn == cn->sock)
      MBN_ERROR(itf->mbn, MBN_ERROR_ITF_READ);
    cn->sock = -1;
    return;
  }

  /* handle the data */
  for(i=0; i<n; i++) {
    if(cn->buflen == 0 && !(buf[i] >= 0x80 && buf[i] < 0xFF))
      continue;
    cn->buf[cn->buflen++] = buf[i];
    if(buf[i] == 0xFF) {
      if(cn->buflen >= MBN_MIN_MESSAGE_SIZE) {
        /* broadcast message, forward to the other connections */
        /* TODO: this can block the thread, use a send buffer? */
        if(buf[0] == 0x81) {
          for(n=0; n<MAX_CONNECTIONS; n++)
            if(dat->conn[n].sock >= 0 && &(dat->conn[n]) != cn)
              transmit(itf, cn->buf, cn->buflen, (void *)&(dat->conn[n]));
        }
        /* now send to mbn */
        mbnProcessRawMessage(itf, cn->buf, cn->buflen, (void *)cn);
      }
      cn->buflen = 0;
    }
    if(cn->buflen >= MBN_MAX_MESSAGE_SIZE)
      cn->buflen = 0;
  }
}


void *receiver(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  struct timeval tv;
  fd_set rdfd;
  int n, i;

  while(1) {
    pthread_testcancel();

    /* select file descriptors */
    FD_ZERO(&rdfd);
    n = 0;
    if(dat->listensocket >= 0) {
      FD_SET(dat->listensocket, &rdfd);
      n = MAX(n, dat->listensocket);
    }
    for(i=0; i<MAX_CONNECTIONS; i++)
      if(dat->conn[i].sock >= 0) {
        FD_SET(dat->conn[i].sock, &rdfd);
        n = MAX(n, dat->conn[i].sock);
      }

    /* wait for readable sockets */
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    n = select(n+1, &rdfd, NULL, NULL, &tv);
    if(n == 0 || (n < 0 && errno == EINTR))
      continue;
    if(n < 0) {
      perror("select()");
      break;
    }

    /* check for incoming connections */
    if(dat->listensocket >= 0 && FD_ISSET(dat->listensocket, &rdfd))
      new_connection(dat);

    /* check for data on all connections */
    for(i=0; i<MAX_CONNECTIONS; i++)
      if(dat->conn[i].sock >=0 && FD_ISSET(dat->conn[i].sock, &rdfd))
        read_connection(itf, &(dat->conn[i]));
  }
  return NULL;
}


void transmit(struct mbn_interface *itf, unsigned char *buf, int length, void *ifaddr) {
  struct tcpconn *cn = (struct tcpconn *)ifaddr;
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  int i, sent, n;

  for(i=0; i<MAX_CONNECTIONS; i++) {
    if(dat->conn[i].sock < 0 || (cn != NULL && cn != &(dat->conn[i])))
      continue;

    sent = 0;
    while((n = send(dat->conn[i].sock, &(buf[sent]), length-sent, 0)) < length-sent) {
      if(n < 0 && dat->conn[i].sock == dat->rconn) {
        MBN_ERROR(itf->mbn, MBN_ERROR_ITF_WRITE);
        perror("send()");
        return;
      }
      sent += n;
    }
  }
}


