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

#include <pthread.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ipexport.h>
#include <pcap.h>

#include "mbn.h"


#define BUFFERSIZE 512

struct pcapdat {
  pcap_t *pc;
  pthread_t thread;
  unsigned char mymac[6];
};


void free_pcap(struct mbn_interface *);
void init_pcap(struct mbn_interface *);
void *receive_packets(void *ptr);
void transmit(struct mbn_interface *, unsigned char *, int, void *);


/* TODO: provide API for getting an interface list */
/* TODO: WinPcap docs don't mention multi-threading, provide manual locking? */


struct mbn_interface * MBN_EXPORT mbnPcapOpen(int ifnum) {
  struct mbn_interface *itf;
  struct pcapdat *dat;
  pcap_addr_t *a;
  pcap_if_t *devs, *d;
  pcap_t *pc = NULL;
  struct bpf_program fp;
  char err[PCAP_ERRBUF_SIZE];
  int i, suc, error = 0;
  unsigned long alen;

  MBN_TRACE(printf("%s", pcap_lib_version()));

  itf = (struct mbn_interface *)calloc(1, sizeof(struct mbn_interface));
  dat = (struct pcapdat *)calloc(1, sizeof(struct pcapdat));
  itf->data = (void *) dat;


  /* get device list */
  if(pcap_findalldevs(&devs, err) < 0) {
    fprintf(stderr, "pcap_findalldevs(): %s\n", err);
    error++;
  }
  if(devs == NULL) {
    fprintf(stderr, "No interfaces found.\n");
    error++;
  }

  /* open device */
  for(i=0, d=devs; d!=NULL; d=d->next)
    if(i++ == ifnum)
      break;
  if(!error && d == NULL) {
    fprintf(stderr, "Selected device doesn't exist\n");
    error++;
  }
  if(!error && (pc = pcap_open_live(d->name, BUFFERSIZE, 0, 1000, err)) == NULL) {
    fprintf(stderr, "pcap_open_live: %s\n", err);
    error++;
  }

  /* get MAC address */
  suc = 0;
  for(a=d->addresses; a!=NULL; a=a->next) {
    if(a->addr != NULL && a->addr->sa_family == AF_INET) {
      alen = 6;
      if((i = SendARP((IPAddr)((struct sockaddr_in *)a->addr)->sin_addr.s_addr, 0, (unsigned long *)dat->mymac, &alen)) != NO_ERROR) {
        fprintf(stderr, "Error: SendARP returned %d\n", i);
        error++;
      } else
        suc = 1;
    }
  }
  if(!suc) {
    fprintf(stderr, "Error: couldn't get MAC address\n");
    error++;
  }

  /* set filter for MambaNet */
  if(!error && pcap_compile(pc, &fp, "ether proto 34848", 1, 0) == -1) {
    fprintf(stderr, "pcap_compile: %s\n", pcap_geterr(pc));
    error++;
  }
  if(!error && pcap_setfilter(pc, &fp) == -1) {
    fprintf(stderr, "pcap_setfilter: %s\n", pcap_geterr(pc));
    error++;
  }
  pcap_freecode(&fp);

  if(devs != NULL)
    pcap_freealldevs(devs);

  if(error) {
    free(dat);
    free(itf);
    return NULL;
  }

  dat->pc = pc;
  itf->cb_free = free_pcap;
  itf->cb_init = init_pcap;
  itf->cb_free_addr = free;
  itf->cb_transmit = transmit;

  return itf;
}


void free_pcap(struct mbn_interface *itf) {
  struct pcapdat *dat = (struct pcapdat *)itf->data;
  pcap_close(dat->pc);
  pthread_cancel(dat->thread);
  pthread_join(dat->thread, NULL);
  free(dat);
  free(itf);
}


void init_pcap(struct mbn_interface *itf) {
  struct pcapdat *dat = (struct pcapdat *)itf->data;

  /* create thread to wait for packets */
  if(pthread_create(&(dat->thread), NULL, receive_packets, (void *) itf) != 0)
    perror("Error creating thread");
}


/* Waits for input from network */
void *receive_packets(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct pcapdat *dat = (struct pcapdat *) itf->data;
  struct pcap_pkthdr *hdr;
  void *ifaddr;
  unsigned char *buffer;
  int i;

  while(1) {
    pthread_testcancel();

    /* wait for/read packet */
    i = pcap_next_ex(dat->pc, &hdr, (u_char *)&buffer);
    if(i < 0)
      break;
    if(i == 0)
      continue;

    /* directly forward message to mbn, with source MAC address as ifaddr */
    ifaddr = malloc(6);
    memcpy(ifaddr, (void *)buffer+6, 6);
    mbnProcessRawMessage(itf, buffer+14, (int)hdr->caplen-14, ifaddr);
  }

  MBN_ERROR(itf->mbn, MBN_ERROR_ITF_READ);

  return NULL;
}


void transmit(struct mbn_interface *itf, unsigned char *buf, int len, void *ifaddr) {
  struct pcapdat *dat = (struct pcapdat *) itf->data;
  unsigned char send[MBN_MAX_MESSAGE_SIZE];

  if(ifaddr != 0)
    memcpy((void *)send, ifaddr, 6);
  else
    memset((void *)send, 0, 6);
  memcpy((void *)send+6, (void *)dat->mymac, 6);
  send[12] = 0x88;
  send[13] = 0x20;
  memcpy((void *)send+14, (void *)buf, len);

  if(pcap_sendpacket(dat->pc, send, len+14) < 0)
    MBN_ERROR(itf->mbn, MBN_ERROR_ITF_WRITE);
}

