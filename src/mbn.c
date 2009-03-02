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

#include "mbn.h"
#include "address.h"
#include "codec.h"


struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info node) {
  struct mbn_handler *mbn;

  mbn = (struct mbn_handler *) calloc(1, sizeof(struct mbn_handler));
  mbn->node = node;

  /* create thread to keep track of timeouts */
  if(pthread_create(&(mbn->timeout_thread), NULL, node_timeout_thread, (void *) mbn) != 0) {
    perror("Error creating timeout thread");
    free(mbn);
    return NULL;
  }

  return mbn;
}


/* Entry point for all incoming MambaNet messages */
void MBN_EXPORT mbnProcessRawMessage(struct mbn_handler *mbn, unsigned char *buffer, int length) {
  int r;
  struct mbn_message msg;

  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.raw = buffer;
  msg.rawlength = length;

  /* parse message */
  if((r = parse_message(&msg)) != 0) {
    MBN_TRACE(printf("Received invalid message (error %02X), dropping", r));
    return;
  }

  MBN_TRACE(printf("Received MambaNet message of %dB from 0x%08lX to 0x%08lX, ctrl 0x%02X, id 0x%06lX, type 0x%04X, %dB data",
     length, msg.AddressFrom, msg.AddressTo, msg.ControlByte, msg.MessageID, msg.MessageType, msg.DataLength));

  if(0 && msg.MessageType == MBN_MSGTYPE_ADDRESS)
    MBN_TRACE(printf(" -> Address Reservation from %04X:%04X:%04X, type 0x%02X, engine 0x%08lX, services 0x%02X",
      msg.Data.Address.ManufacturerID, msg.Data.Address.ProductID, msg.Data.Address.UniqueIDPerProduct,
      msg.Data.Address.Type, msg.Data.Address.EngineAddr, msg.Data.Address.Services));

  /* send ReceiveMessage() callback, and stop processing if it returned non-zero */
  if(mbn->cb_ReceiveMessage != NULL && mbn->cb_ReceiveMessage(mbn, &msg) != 0) {
    free_message(&msg);
    return;
  }

  /* handle address reservation messages */
  if(process_address_message(mbn, &msg) != 0) {
    free_message(&msg);
    return;
  }

  /* TODO: process message and send callbacks */

  free_message(&msg);
}


