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

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/un.h>
#include <pthread.h>

#include "mbn.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
/* the actual number of max. connections is also limited by the
 * number of sockets the select() call accepts.
 * Each connection requires about 128 bytes for buffers */
#define MAX_CONNECTIONS 10
#define BUFFERSIZE     512


struct unixconn {
  unsigned char buf[MBN_MAX_MESSAGE_SIZE];
  int buflen;
  int socket; /* -1 when unused */
  char remote_path[108];
};

struct unixdat {
  pthread_t thread;
  char thread_run;
  char listen_path[108];
  struct unixconn conn[MAX_CONNECTIONS];
  int client_socket;
  int listen_socket;
};

int setup_unix_client(struct unixdat *, char *, char *);
int setup_unix_server(struct unixdat *, char *, char *);
int init_unix(struct mbn_interface *, char *);
void stop_unix(struct mbn_interface *);
void free_unix(struct mbn_interface *);
void free_addr_unix(struct mbn_interface *, void *);
void *unix_receiver(void *);
int unix_transmit(struct mbn_interface *, unsigned char *, int, void *, char *);


struct mbn_interface * MBN_EXPORT mbnUnixOpen(char *remote_path, char *my_path, char *err) {
  struct mbn_interface *itf;
  struct unixdat *dat;
  int i, error = 0;

  itf = (struct mbn_interface *)calloc(1, sizeof(struct mbn_interface));
  dat = (struct unixdat *)calloc(1, sizeof(struct unixdat));
  itf->data = (void *)dat;

  /* initialize connection table */
  for(i=0; i<MAX_CONNECTIONS; i++)
    dat->conn[i].socket = -1;

  if(remote_path != NULL) {
    error += setup_unix_client(dat, remote_path, err);
  }
  else
    dat->client_socket = -1;

  if(!error && my_path != NULL) {
    error += setup_unix_server(dat, my_path, err);
  } else
    dat->listen_socket = -1;

  if(error) {
    free(dat);
    free(itf);
    return NULL;
  }

  itf->cb_init = init_unix;
  itf->cb_stop = stop_unix;
  itf->cb_free = free_unix;
  itf->cb_free_addr = free_addr_unix;
  itf->cb_transmit = unix_transmit;
  return itf;
}


/* TODO: non-blocking connect()? time-out? */
int setup_unix_client(struct unixdat *dat, char *path, char *err) {
  struct sockaddr_un sockaddr;
  int on = 1;

  memset(&sockaddr, 0, sizeof(struct sockaddr_un));

  dat->client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if(dat->client_socket < 0) {
    sprintf(err, "socket(): %s", strerror(errno));
    return 1;
  }

  setsockopt(dat->client_socket, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

  /* bind socket with the interface 
  sockaddr.sun_family = AF_UNIX;
  strncpy(sockaddr.sun_path, path, 108);
  if(!error && bind(dat->client_socket, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_un)) < 0) {
    sprintf(err, "Couldn't bind socket: %s", strerror(errno));
    error++;
  }*/

  /* connect */
  sockaddr.sun_family = AF_UNIX;
  strncpy(sockaddr.sun_path, path, 108);
  if(connect(dat->client_socket, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_un)) < 0) {
    sprintf(err, "Couldn't connect to unix socket %s", path);
    close(dat->client_socket);
    return 1;
  }

  dat->conn[0].socket = dat->client_socket;
  dat->conn[0].buflen = 0;

  return 0;
}


int setup_unix_server(struct unixdat *dat, char *path, char *err) {
  struct sockaddr_un sockaddr;
  int on = 1;

  memset(&sockaddr, 0, sizeof(struct sockaddr_un));

  unlink(path);

  dat->listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if(dat->listen_socket < 0) {
    sprintf(err, "socket(): %s", strerror(errno));
    return 1;
  }

  setsockopt(dat->listen_socket, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

  /* bind socket with the interface */
  sockaddr.sun_family = AF_UNIX;
  strncpy(sockaddr.sun_path, path, 108);
  if(bind(dat->listen_socket, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_un)) < 0) {
    sprintf(err, "Couldn't bind socket: %s", strerror(errno));
    close(dat->listen_socket);
    return 1;
  }

  /* listen */
  if (listen(dat->listen_socket, 5) < 0) {
    sprintf(err, "Couldn't start listening: %s", strerror(errno));
    close(dat->listen_socket);
    return 1;
  }

  return 0;
}


int init_unix(struct mbn_interface *itf, char *err) {
  struct unixdat *dat = (struct unixdat *)itf->data;
  int i;

  if((i = pthread_create(&(dat->thread), NULL, unix_receiver, (void *) itf)) != 0) {
    sprintf(err, "Can't create thread: %s (%d)", strerror(i), i);
    return 1;
  }
  return 0;
}


void stop_unix(struct mbn_interface *itf) {
  struct unixdat *dat = (struct unixdat *)itf->data;
  int i;

  for(i=0; !dat->thread_run; i++) {
    if(i > 5)
      break;
    sleep(1);
  }
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);
}

void free_unix(struct mbn_interface *itf) {
  struct unixdat *dat = (struct unixdat *)itf->data;
  int i;

  for(i=0; !dat->thread_run; i++) {
    if(i > 5)
      break;
    sleep(1);
  }
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);

  for(i=0; i<MAX_CONNECTIONS; i++)
    if(dat->conn[i].socket >= 0)
      close(dat->conn[i].socket);

  if (dat->listen_socket >= 0)
    close(dat->listen_socket);

  free(dat);
  free(itf);
}

void free_addr_unix(struct mbn_interface *itf, void *ifaddr) {
  /* Nothing to be don because *ifaddr is not dynamically allocated */
  return;
  itf = NULL;
  ifaddr = NULL;
}

void new_unix_connection(struct mbn_interface *itf, struct unixdat *dat) {
  int i;
  struct sockaddr_un remote_addr;
  unsigned int remote_addr_length = sizeof(remote_addr);

  for(i=0; i<MAX_CONNECTIONS; i++)
    if(dat->conn[i].socket < 0)
      break;

  /* MAX_CONNECTIONS reached, just close the connection */
  if(i >= MAX_CONNECTIONS) {
    if((i = accept(dat->listen_socket, (struct sockaddr *)&remote_addr, &remote_addr_length)) > 0)
      close(i);
    mbnWriteLogMessage(itf, "Rejected unix connection from %s", remote_addr.sun_path);
    return;
  }

  /* accept the connection */
  if((dat->conn[i].socket = accept(dat->listen_socket, (struct sockaddr *)&remote_addr, &remote_addr_length)) < 0)
    return;
  dat->conn[i].buflen = 0;
  strncpy(dat->conn[i].remote_path, remote_addr.sun_path, 108);

  mbnWriteLogMessage(itf, "Accepted unix connection as socket %d", dat->conn[i].socket);
}

int read_unix_connection(struct mbn_interface *itf, struct unixconn *cn, char *err) {
  struct unixdat *dat = (struct unixdat *)itf->data;
  unsigned char buf[BUFFERSIZE];
  int n, i, j;

  n = recv(cn->socket, (char *)buf, BUFFERSIZE, 0);
  if(n < 0 && errno == EINTR)
    return 0;

  /* error, close connection */
  if(n <= 0) {
    close(cn->socket);
    /* oops, this was our remote connection, we shouldn't lose this one! */
    if(dat->client_socket == cn->socket) {
      mbnWriteLogMessage(itf, "Lost connection to server");
      sprintf(err, "Lost connection to server");
      return 1;
    }
    cn->socket = -1;
    mbnWriteLogMessage(itf, "Closed connection for socket %d", cn->socket);
    memset(cn->remote_path, 0, 108);
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
          for(j=0; j<MAX_CONNECTIONS; j++)
            if(dat->conn[j].socket >= 0 && &(dat->conn[j]) != cn) {
              unix_transmit(itf, cn->buf, cn->buflen, (void *)&(dat->conn[j]), err);
            }
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


void *unix_receiver(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct unixdat *dat = (struct unixdat *)itf->data;
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
    if(dat->listen_socket >= 0) {
      FD_SET(dat->listen_socket, &rdfd);
      n = MAX(n, dat->listen_socket);
    }
    for(i=0; i<MAX_CONNECTIONS; i++)
      if(dat->conn[i].socket >= 0) {
        FD_SET(dat->conn[i].socket, &rdfd);
        n = MAX(n, dat->conn[i].socket);
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
    if(dat->listen_socket >= 0 && FD_ISSET(dat->listen_socket, &rdfd))
      new_unix_connection(itf, dat);

    /* check for data on all connections */
    for(i=0; i<MAX_CONNECTIONS; i++)
      if(dat->conn[i].socket >=0 && FD_ISSET(dat->conn[i].socket, &rdfd))
        if(read_unix_connection(itf, &(dat->conn[i]), err))
          mbnInterfaceReadError(itf, err);
  }
  return NULL;
}


int unix_transmit(struct mbn_interface *itf, unsigned char *buf, int length, void *ifaddr, char *err) {
  struct unixconn *cn = (struct unixconn *)ifaddr;
  struct unixdat *dat = (struct unixdat *)itf->data;
  int i;

  for(i=0; i<MAX_CONNECTIONS; i++) {
    if(dat->conn[i].socket < 0 || (cn != NULL && cn != &(dat->conn[i])))
      continue;

    send(dat->conn[i].socket, (char *)buf, length, MSG_DONTWAIT);
  }
  return 0;
  err = NULL;
}


