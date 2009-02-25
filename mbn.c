
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "mbn.h"

/* cross platform sleep()
 * (better to check for the existence of unistd.h
 *  than __GNUC__, oh well...) */
#ifdef __GNUC__
# include <unistd.h>
#else
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif


mbn_callback cb;


void *thread(void *arg) {
  int *nr, ret;
  struct mbn_handler *h = (struct mbn_handler *) arg;
  printf("[mbnThread] Thread init called! Sleeping for a sec...\n");

  sleep(1);
  printf("[mbnThread] Sending the number '5' to application.\n");

  nr = (int *) malloc(sizeof(int));
  *nr = 5;
  ret = h->cb(nr);
  printf("[mbnThread] Application returned %d\n", ret);

  printf("[mbnThread] Node Description: %s\n", h->node.Description);
  
  free(nr);
}


struct mbn_handler *mbnInit(struct mbn_node_info node, mbn_callback cl) {
  int r;
  pthread_t thr;
  struct mbn_handler *h;

  h = (struct mbn_handler *) malloc(sizeof(struct mbn_handler));
  h->cb = cl;
  h->node = node;

  printf("[mbnInit] Creating thread\n");
  r = pthread_create(&thr, NULL, thread, h);
  if(r)
    printf("[mbnInit] ERROR creating thread: %d\n", r);
  printf("[mbnInit] Thread creation done\n");
}


