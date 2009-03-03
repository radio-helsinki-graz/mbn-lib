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
#include <sys/time.h>


struct mbn_node_info this_node = {
  0x00050000, // MambaNet Addr
  1, 50, 0,   // UniqueMediaAccessId
  "MambaNet Stack Test Application",
  "MambaNet Test",
  0, 0,       // Hardware revision
  0, 0,       // Firmware revision
  0,          // NumberOfObjects
  0,          // DefaultEngineAddr
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } // Hardwareparent
};


int ReceiveMessage(struct mbn_handler *mbn, struct mbn_message *msg) {
  int i;
  return 0;

  if(msg->MessageType == MBN_MSGTYPE_OBJECT) {
    printf("Object Message: number %02X, action %2d, type %3d, size %2dB\n",
      msg->Data.Object.Number, msg->Data.Object.Action, msg->Data.Object.DataType, msg->Data.Object.DataSize);

    for(i=0; i<msg->bufferlength; i++)
      printf(" %02X", msg->buffer[i]);
    printf("\n");
    if(msg->Data.Object.DataType == MBN_DATATYPE_SINT)
      printf(" -> SInt: %ld\n", msg->Data.Object.Data.SInt);
  }
  return 0;
}

void AddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct mbn_address_node *cur;
  cur = new == NULL ? old : new;
  printf("%s: %08lX  ->  %04X:%04X:%04X\n",
    old == NULL ? "New node" : new == NULL ? "Removed node" : "Node changed",
    cur->MambaNetAddr, cur->ManufacturerID, cur->ProductID, cur->UniqueIDPerProduct);
}


int main(void) {
  struct mbn_handler *mbn;
  struct timeval before, after;

  mbn = mbnInit(this_node);
  mbnSetReceiveMessageCallback(mbn, ReceiveMessage);
  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);
  mbnEthernetInit(mbn, "eth0");

  sleep(2);
  gettimeofday(&before, NULL);
  mbnFree(mbn);
  gettimeofday(&after, NULL);
  printf("mbnFree() finished in %.3fms\n",
    (((float) after.tv_sec)+((float) after.tv_usec)/1000000.0f) - (((float) before.tv_sec)+((float) before.tv_usec)/1000000.0f));

  pthread_exit(NULL);
  return 0;
}


