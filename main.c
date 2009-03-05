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

#include "mbn.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>


struct mbn_node_info this_node = {
  0x00031337, 0x00, // MambaNet Addr + Services
  1, 50, 0,   // UniqueMediaAccessId
  "MambaNet Stack Test Application",
  ">> YorHel's Power Node! <<",
  0, 0,       // Hardware revision
  0, 0,       // Firmware revision
  0,          // NumberOfObjects
  0,          // DefaultEngineAddr
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } // Hardwareparent
};


void AddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct mbn_address_node *cur;
  cur = new == NULL ? old : new;
  printf("%s: %08lX  ->  %04X:%04X:%04X (%02X)\n",
    old == NULL ? "New node" : new == NULL ? "Removed node" : "Node changed",
    cur->MambaNetAddr, cur->ManufacturerID, cur->ProductID, cur->UniqueIDPerProduct, cur->Services);
}


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  printf("OnlineStatus: %08lX %s\n", addr, valid ? "validated" : "invalid");
  if(valid)
    mbnSendPingRequest(mbn, MBN_BROADCAST_ADDRESS);
}


int NameChange(struct mbn_handler *mbn, unsigned char *name) {
  printf("NameChange(\"%s\")\n", name);
  return 0;
}


int DefaultEngineAddrChange(struct mbn_handler *mbn, unsigned long engine) {
  printf("DefaultEngineAddrChange(0x%08lX)\n", engine);
  return 0;
}


int main(void) {
  struct mbn_handler *mbn;
  struct timeval before, after;
  struct mbn_message msg;

  mbn = mbnInit(this_node);
  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);
  mbnSetOnlineStatusCallback(mbn, OnlineStatus);
  mbnSetNameChangeCallback(mbn, NameChange);
  mbnSetDefaultEngineAddrChangeCallback(mbn, DefaultEngineAddrChange);
  mbnEthernetInit(mbn, "eth0");

  pthread_exit(NULL);
  return 0;
}


