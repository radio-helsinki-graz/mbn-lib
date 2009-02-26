
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "mbn.h"

/* sleep() */
#ifdef MBN_LINUX
# include <unistd.h>
#elif
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif



struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info node, struct mbn_interface interface) {
  int r;
  pthread_t thr;
  struct mbn_handler *mbn;

  mbn = (struct mbn_handler *) malloc(sizeof(struct mbn_handler));
  mbn->node = node;
  mbn->interface = interface;
  mbn->interface.data = NULL;

  return mbn;
}


void MBN_EXPORT mbnProcessRawMambaNetMessage(struct mbn_handler *mbn, unsigned char *buffer, int length) {
  MBN_TRACE(printf("Received MambaNet message of %d bytes, start = %02X, end = %02X",
     length, buffer[0], buffer[length-1]));
}


