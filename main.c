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

#define MBN_VARARG
#include "mbn.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>


struct mbn_node_info this_node = {
  0x00031337, 0x00, /* MambaNet Addr + Services */
  "MambaNet Stack Test Application",
  ">> YorHel's Power Node! <<",
  0xFFFF, 0x0001, 0x0001,   /* UniqueMediaAccessId */
  0, 0,          /* Hardware revision */
  0, 0,          /* Firmware revision */
  0, 0,          /* FPGAFirmware revision */
  2,             /* NumberOfObjects */
  0,             /* DefaultEngineAddr */
  {0,0,0,0,0,0}, /* Hardwareparent */
  0              /* Service request */
};

struct mbn_object objects[2];


void AddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct mbn_address_node *cur;
  cur = new == NULL ? old : new;
  printf("%s: %08lX  ->  %04X:%04X:%04X (%02X)\n",
    old == NULL ? "New node" : new == NULL ? "Removed node" : "Node changed",
    cur->MambaNetAddr, cur->ManufacturerID, cur->ProductID, cur->UniqueIDPerProduct, cur->Services);
  mbn += 1;
}


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  printf("OnlineStatus: %08lX %s\n", addr, valid ? "validated" : "invalid");
  if(valid)
    mbnSendPingRequest(mbn, MBN_BROADCAST_ADDRESS);
  mbn += 1;
}


void Error(struct mbn_handler *mbn, int code, const char *msg) {
  printf("Error(%d, \"%s\")\n", code, msg);
  mbn += 1;
}


int main(void) {
  struct mbn_handler *mbn;
  struct mbn_interface *itf = NULL;
  char err[MBN_ERRSIZE];

#ifdef MBN_IF_ETHERNET
  struct mbn_if_ethernet *ifl, *n;
  char *ifname = NULL;
  ifl = mbnEthernetIFList(err);
  if(ifl == NULL) {
    printf("Error: %s\n", err);
    return 1;
  }
  for(n=ifl; n!=NULL; n=n->next) {
    if(!ifname)
      ifname = n->name;
    printf("ethernet interface \"%s\": %02X:%02X:%02X:%02X:%02X:%02X (%s)\n",
      n->name, n->addr[0], n->addr[1], n->addr[2], n->addr[3], n->addr[4], n->addr[5],
      n->desc ? n->desc : "no description");
  }
  itf = mbnEthernetOpen(ifname, err);
  if(itf == NULL) {
    printf("Error: %s\n", err);
    return 1;
  }
  mbnEthernetIFFree(ifl);
#endif

  objects[0] = MBN_OBJ("Object #1", 1, MBN_DATATYPE_UINT, 2, 0, 512, 256, MBN_DATATYPE_NODATA);
  objects[1] = MBN_OBJ("Object #2", 0, MBN_DATATYPE_NODATA, MBN_DATATYPE_UINT, 2, 0, 512, 0, 256);

  mbn = mbnInit(this_node, objects, itf, err);
  if(mbn == NULL) {
    printf("Error initializing mbn: %s\n", err);
    return 1;
  }

  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);
  mbnSetOnlineStatusCallback(mbn, OnlineStatus);
  mbnSetErrorCallback(mbn, Error);

  /*sleep(60);*/
  pthread_exit(NULL);
  mbnFree(mbn);
  return 0;
}


