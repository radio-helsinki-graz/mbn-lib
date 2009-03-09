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

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "mbn.h"
#include "address.h"
#include "codec.h"
#include "object.h"


struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info node, struct mbn_object *objects) {
  struct mbn_handler *mbn;
  int i, l;
  pthread_mutexattr_t mattr;

  mbn = (struct mbn_handler *) calloc(1, sizeof(struct mbn_handler));
  mbn->node = node;
  mbn->node.Services &= 0x7F; /* turn off validated bit */
  mbn->objects = objects;

  /* pad descriptions and name with zero and clear some other things */
  l = strlen((char *)mbn->node.Description);
  if(l < 64)
    memset((void *)&(mbn->node.Description[l]), 0, 64-l);
  l = strlen((char *)mbn->node.Name);
  if(l < 32)
    memset((void *)&(mbn->node.Name[l]), 0, 32-l);
  for(i=0;i<mbn->node.NumberOfObjects;i++) {
    mbn->objects[i].changed = mbn->objects[i].timeout = 0;
    l = strlen((char *)mbn->objects[i].Description);
    if(l < 32)
      memset((void *)&(mbn->objects[i].Description[l]), 0, 32-l);
  }

  /* init the mutex for locking the mbn_handler data.
   * Recursive type, because we call other functions
   *  that may lock the mutex themselves as well */
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(mbn->mbn_mutex), &mattr);

  /* create threads to keep track of timeouts */
  if(    pthread_create(&(mbn->timeout_thread),  NULL, node_timeout_thread, (void *) mbn) != 0
      || pthread_create(&(mbn->throttle_thread), NULL, throttle_thread,     (void *) mbn) != 0) {
    perror("Error creating threads");
    free(mbn);
    return NULL;
  }

  return mbn;
}


/* IMPORTANT: must not be called in a thread which has a lock on mbn_mutex */
void MBN_EXPORT mbnFree(struct mbn_handler *mbn) {
  /* request cancellation for the threads */
  pthread_cancel(mbn->timeout_thread);
  pthread_cancel(mbn->throttle_thread);

  /* free interface */
  if(mbn->interface.cb_free != NULL)
    mbn->interface.cb_free(mbn);

  /* free address list */
  free_addresses(mbn);

  /* wait for the threads
   * (make sure no locks on mbn->mbn_mutex are present here) */
  pthread_join(mbn->timeout_thread, NULL);
  pthread_join(mbn->throttle_thread, NULL);
}


/* Entry point for all incoming MambaNet messages */
void MBN_EXPORT mbnProcessRawMessage(struct mbn_handler *mbn, unsigned char *buffer, int length, void *ifaddr) {
  int r, processed = 0;
  struct mbn_message msg;

  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.raw = buffer;
  msg.rawlength = length;

  if(0) {
    printf("RAW -> ");
    for(r=0;r<msg.rawlength;r++)
      printf(" %02X", msg.raw[r]);
    printf("\n");
  }

  /* parse message */
  if((r = parse_message(&msg)) != 0) {
    if(0 && msg.bufferlength) {
      printf("BUF -> ");
      for(r=0;r<msg.bufferlength;r++)
        printf(" %02X", msg.buffer[r]);
      printf("\n");
    }
    MBN_TRACE(printf("Received invalid message (error %02X), dropping", r));
    return;
  }

  if(0)
    MBN_TRACE(printf("Received MambaNet message of %dB from 0x%08lX to 0x%08lX, id 0x%06lX, type 0x%04X",
       length, msg.AddressFrom, msg.AddressTo, msg.MessageID, msg.MessageType));

  if(0 && msg.MessageType == MBN_MSGTYPE_ADDRESS)
    MBN_TRACE(printf(" -> Address Reservation from %04X:%04X:%04X, type 0x%02X, engine 0x%08lX, services 0x%02X",
      msg.Data.Address.ManufacturerID, msg.Data.Address.ProductID, msg.Data.Address.UniqueIDPerProduct,
      msg.Data.Address.Type, msg.Data.Address.EngineAddr, msg.Data.Address.Services));

  if(0 && msg.MessageType == MBN_MSGTYPE_OBJECT)
    MBN_TRACE(printf(" -> Object message, action %d, object #%d, datatype %d",
      msg.Data.Object.Action, msg.Data.Object.Number, msg.Data.Object.DataType));

  /* we're going to be accessing the mbn struct, lock! */
  pthread_mutex_lock(&(mbn->mbn_mutex));

  /* send ReceiveMessage() callback, and stop processing if it returned non-zero */
  if(!processed && mbn->cb_ReceiveMessage != NULL && mbn->cb_ReceiveMessage(mbn, &msg) != 0)
    processed++;

  /* we don't handle acknowledge replies yet, ignore them for now */
  if(!processed && msg.AcknowledgeReply)
    processed++;

  /* handle address reservation messages */
  if(!processed && process_address_message(mbn, &msg, ifaddr) != 0)
    processed++;

  /* we can't handle any other messages if we don't have a validated address */
  if(!(mbn->node.Services & MBN_ADDR_SERVICES_VALID))
    processed++;
  /* ...or if it's not targeted at us */
  if(msg.AddressTo != MBN_BROADCAST_ADDRESS && msg.AddressTo != mbn->node.MambaNetAddr)
    processed++;

  /* object messages */
  if(!processed && process_object_message(mbn, &msg) != 0)
    processed++;

  pthread_mutex_unlock(&(mbn->mbn_mutex));
  free_message(&msg);
}


/* TODO: notify application on errors */
void MBN_EXPORT mbnSendMessage(struct mbn_handler *mbn, struct mbn_message *msg, int flags) {
  unsigned char raw[MBN_MAX_MESSAGE_SIZE];
  struct mbn_address_node *dest;
  void *ifaddr;
  int r;

  if(mbn->interface.cb_transmit == NULL)
    return;

  if(!(flags & MBN_SEND_IGNOREVALID) && !(mbn->node.Services & MBN_ADDR_SERVICES_VALID))
    return;

  /* just forward the raw data to the interface, if we don't need to do any processing */
  if(flags & MBN_SEND_RAWDATA) {
    mbn->interface.cb_transmit(mbn, raw, msg->rawlength, NULL);
    return;
  }

  if(!(flags & MBN_SEND_FORCEADDR))
    msg->AddressFrom = mbn->node.MambaNetAddr;

  msg->raw = raw;
  msg->rawlength = 0;

  /* create the message */
  if((r = create_message(msg, (flags & MBN_SEND_NOCREATE)?1:0)) != 0) {
    MBN_TRACE(printf("Error creating message: %02X", r));
    return;
  }

  /* determine interface address */
  if(msg->AddressTo == MBN_BROADCAST_ADDRESS)
    ifaddr = NULL;
  else {
    if((dest = mbnNodeStatus(mbn, msg->AddressTo)) == NULL)
      ifaddr = NULL;
    else
      ifaddr = dest->ifaddr;
  }

  if(0) {
    printf("RAW: ");
    for(r=0; r<msg->rawlength; r++)
      printf(" %02X", msg->raw[r]);
    printf("\nBUF: ");
    for(r=0; r<msg->bufferlength; r++)
      printf(" %02X", msg->buffer[r]);
    printf("\n");
  }

  /* send the data to the interface transmit callback */
  mbn->interface.cb_transmit(mbn, raw, msg->rawlength, ifaddr);
}


