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

/* sleep() */
#ifdef MBNP_mingw
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#else
# include <unistd.h>
#endif

#include "mbn.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MBN_TCP_PORT "34848"
/* the actual number of max. connections is also limited by the
 * number of sockets the select() call accepts.
 * Each connection requires about 128 bytes for buffers */
#define MAX_CONNECTIONS 64
#define BUFFERSIZE     512


struct tcpconn {
  unsigned char buf[MBN_MAX_MESSAGE_SIZE];
  int buflen;
  int sock; /* -1 when unused */
  unsigned long remoteip;
  unsigned int remoteport;
};

struct tcpdat {
  pthread_t thread;
  char thread_run;
  int listensocket;
  int rconn;
  struct tcpconn conn[MAX_CONNECTIONS];
};

int setup_client(struct tcpdat *, char *, char *, char *);
int setup_server(struct tcpdat *, char *, char *, char *);
int init_tcp(struct mbn_interface *, char *);
void stop_tcp(struct mbn_interface *);
void free_tcp(struct mbn_interface *);
void *receiver(void *);
int tcptransmit(struct mbn_interface *, unsigned char *, int, void *, char *);


struct mbn_interface * MBN_EXPORT mbnTCPOpen(char *remoteip, char *remoteport, char *myip, char *myport, char *err) {
  struct mbn_interface *itf;
  struct tcpdat *dat;
  int i, error = 0;

  /* Why the hell is this call _required_?
   * Why doesn't Microsoft just support plain BSD sockets? */
#ifdef MBNP_mingw
  WSADATA wsadat;
  if(WSAStartup(MAKEWORD(2, 0), &wsadat) != 0) {
    sprintf(err, "Unsupported winsock version");
    return NULL;
  }
#endif

  itf = (struct mbn_interface *)calloc(1, sizeof(struct mbn_interface));
  dat = (struct tcpdat *)calloc(1, sizeof(struct tcpdat));
  itf->data = (void *)dat;

  /* initialize connection table */
  for(i=0; i<MAX_CONNECTIONS; i++)
    dat->conn[i].sock = -1;

  if(remoteip != NULL) {
    if(remoteport == NULL)
      remoteport = MBN_TCP_PORT;
    error += setup_client(dat, remoteip, remoteport, err);
  } else
    dat->rconn = -1;

  if(!error && myip != NULL) {
    if(myport == NULL)
      myport = MBN_TCP_PORT;
    error += setup_server(dat, myip, myport, err);
  } else
    dat->listensocket = -1;

  if(error) {
    free(dat);
    free(itf);
#ifdef MBNP_mingw
    WSACleanup();
#endif
    return NULL;
  }

  itf->cb_init = init_tcp;
  itf->cb_stop = stop_tcp;
  itf->cb_free = free_tcp;
  itf->cb_transmit = tcptransmit;
  return itf;
}


/* TODO: non-blocking connect()? time-out? */
int setup_client(struct tcpdat *dat, char *server, char *port, char *err) {
  struct addrinfo hint, *res, *rp;
#ifdef MBNP_mingw
  unsigned long NonBlockMode;
#endif

  /* lookup hostname/ip address */
  memset((void *)&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;

  if(getaddrinfo(server, port, &hint, &res) != 0) {
    sprintf(err, "Can't resolve %s port %s: %s", server, port, strerror(errno));
    return 1;
  }

  /* loop through possibilities and try to connect */
  for(rp=res; rp != NULL; rp=rp->ai_next) {
    /* create socket */
    if((dat->rconn = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) < 0)
      continue;
    /* connect */
    if(connect(dat->rconn, rp->ai_addr, rp->ai_addrlen) >= 0)
      break;
    close(dat->rconn);
  }

  freeaddrinfo(res);
  if(rp == NULL) {
    sprintf("Couldn't connect to %s port %s: %s", server, port, strerror(errno));
    return 1;
  }
  dat->conn[0].sock = dat->rconn;
  dat->conn[0].buflen = 0;

#ifdef MBNP_mingw
  NonBlockMode=1;
  ioctlsocket(dat->conn[0].sock, FIONBIO,&NonBlockMode);
#endif

  return 0;
}


int setup_server(struct tcpdat *dat, char *ip, char *port, char *err) {
  struct addrinfo hint, *res, *rp;
  int n;

  /* lookup hostname/ip address */
  memset((void *)&hint, 0, sizeof(struct addrinfo));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  if(getaddrinfo(ip, port, &hint, &res) != 0) {
    sprintf(err, "Can't resolve %s port %s: %s", ip, port, strerror(errno));
    return 1;
  }

  for(rp=res; rp != NULL; rp=rp->ai_next) {
    /* create socket */
    if((dat->listensocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) < 0)
      continue;
    /* bind */
    if(setsockopt(dat->listensocket, SOL_SOCKET, SO_REUSEADDR, (void *)&n, sizeof(int)) >= 0
       && bind(dat->listensocket, rp->ai_addr, rp->ai_addrlen) >= 0
       && listen(dat->listensocket, 5) >= 0)
      break;
    close(dat->listensocket);
  }
  if(rp == NULL) {
    sprintf(err, "Can't bind to %s port %s: %s", ip, port, strerror(errno));
    return 1;
  }

  return 0;
}


int init_tcp(struct mbn_interface *itf, char *err) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  int i;

  if((i = pthread_create(&(dat->thread), NULL, receiver, (void *) itf)) != 0) {
    sprintf(err, "Can't create thread: %s (%d)", strerror(i), i);
    return 1;
  }
  return 0;
}


void stop_tcp(struct mbn_interface *itf) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  int i;

  for(i=0; !dat->thread_run; i++) {
    if(i > 5)
      break;
    sleep(1);
  }
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);
}

void free_tcp(struct mbn_interface *itf) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  int i;

  for(i=0; !dat->thread_run; i++) {
    if(i > 5)
      break;
    sleep(1);
  }
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);

  for(i=0; i<MAX_CONNECTIONS; i++)
    if(dat->conn[i].sock >= 0)
      close(dat->conn[i].sock);

  free(dat);
  free(itf);
#ifdef MBNP_mingw
  WSACleanup();
#endif
}


void new_connection(struct mbn_interface *itf, struct tcpdat *dat) {
  int i;
  struct sockaddr_in remote_addr;
  unsigned int remote_addr_length = sizeof(remote_addr);
#ifdef MBNP_mingw
  unsigned long NonBlockMode;
#endif

  for(i=0; i<MAX_CONNECTIONS; i++)
    if(dat->conn[i].sock < 0)
      break;

  /* MAX_CONNECTIONS reached, just close the connection */
  if(i >= MAX_CONNECTIONS) {
    if((i = accept(dat->listensocket, (struct sockaddr *)&remote_addr, &remote_addr_length)) > 0)
      close(i);
    mbnWriteLogMessage(itf, "Rejected TCP connection from %s:%d", inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port));
    return;
  }

  /* accept the connection */
  if((dat->conn[i].sock = accept(dat->listensocket, (struct sockaddr *)&remote_addr, &remote_addr_length)) < 0)
    return;
  dat->conn[i].buflen = 0;
  dat->conn[i].remoteip = remote_addr.sin_addr.s_addr;
  dat->conn[i].remoteport = remote_addr.sin_port;

#ifdef MBNP_mingw
  NonBlockMode=1;
  ioctlsocket(dat->conn[i].sock, FIONBIO,&NonBlockMode);
#endif
  mbnWriteLogMessage(itf, "Accepted TCP connection from %s:%d", inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port));
}


int read_connection(struct mbn_interface *itf, struct tcpconn *cn, char *err) {
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  unsigned char buf[BUFFERSIZE];
  int n, i;
  struct in_addr remote_addr;

  n = recv(cn->sock, (char *)buf, BUFFERSIZE, 0);
  if(n < 0 && errno == EINTR)
    return 0;

  /* error, close connection */
  if(n <= 0) {
    close(cn->sock);
    /* oops, this was our remote connection, we shouldn't lose this one! */
    if(dat->rconn == cn->sock) {
      mbnWriteLogMessage(itf, "Lost connection to server");
      sprintf(err, "Lost connection to server");
      return 1;
    }
    cn->sock = -1;
    remote_addr.s_addr = cn->remoteip;
    mbnWriteLogMessage(itf, "Closed connection from %s:%d", inet_ntoa(remote_addr), ntohs(cn->remoteport));
    cn->remoteip = 0;
    cn->remoteport = 0;
    return 0;
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
              tcptransmit(itf, cn->buf, cn->buflen, (void *)&(dat->conn[n]), err);
        }
        /* now send to mbn */
        mbnProcessRawMessage(itf, cn->buf, cn->buflen, (void *)cn);
      }
      cn->buflen = 0;
    }
    if(cn->buflen >= MBN_MAX_MESSAGE_SIZE)
      cn->buflen = 0;
  }
  return 0;
}


void *receiver(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  char err[MBN_ERRSIZE];
  struct timeval tv;
  fd_set rdfd;
  int n, i;

  dat->thread_run = 1;

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
      sprintf(err, "Couldn't check for read state: %s", strerror(errno));
      mbnInterfaceReadError(itf, err);
      break;
    }

    /* check for incoming connections */
    if(dat->listensocket >= 0 && FD_ISSET(dat->listensocket, &rdfd))
      new_connection(itf, dat);

    /* check for data on all connections */
    for(i=0; i<MAX_CONNECTIONS; i++)
      if(dat->conn[i].sock >=0 && FD_ISSET(dat->conn[i].sock, &rdfd))
        if(read_connection(itf, &(dat->conn[i]), err))
          mbnInterfaceReadError(itf, err);
  }
  return NULL;
}


int tcptransmit(struct mbn_interface *itf, unsigned char *buf, int length, void *ifaddr, char *err) {
  struct tcpconn *cn = (struct tcpconn *)ifaddr;
  struct tcpdat *dat = (struct tcpdat *)itf->data;
  int i;

  for(i=0; i<MAX_CONNECTIONS; i++) {
    if(dat->conn[i].sock < 0 || (cn != NULL && cn != &(dat->conn[i])))
      continue;


#ifdef MBNP_mingw
    send(dat->conn[i].sock, (char *)buf, length, 0);
#else
    send(dat->conn[i].sock, (char *)buf, length, MSG_DONTWAIT);
#endif
  }
  return 0;
  err = NULL;
}


