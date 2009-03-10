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

/* sleep() */
#ifdef MBN_LINUX
# include <unistd.h>
#elif
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif


/* IMPORTANT: keep this in the same order as enum mbn_errors */
const char *mbn_errormessages[] = {
  "No interface registered",
  "Cannot send message without valid address",
  "Cannot create message: invalid mbn_message struct",
  "Received invalid message",
  "Could not read for interface",
  "Could not write to interface"
};


/* thread that keeps track of messages requiring an acknowledge reply,
 * and retries the message after a timeout (of one second, currently) */
void *msgqueue_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  struct mbn_msgqueue *q, *last, *tmp;

  while(1) {
    /* working on mbn_handler, so lock */
    pthread_mutex_lock((pthread_mutex_t *) mbn->mbn_mutex);

    for(q=last=mbn->queue; q!=NULL; ) {
      /* Oh my, no reply after all those retries :( */
      if(++q->retries >= MBN_ACKNOWLEDGE_RETRIES) {
        /* send callback */
        if(mbn->cb_AcknowledgeTimeout != NULL)
          mbn->cb_AcknowledgeTimeout(mbn, &(q->msg));
        /* remove item from the queue */
        if(last == mbn->queue)
          mbn->queue = q->next;
        else
          last->next = q->next;
        tmp = q->next;
        free_message(&(q->msg));
        free(q);
        q = tmp;
        continue;
      }
      /* No reply yet, let's try again */
      mbnSendMessage(mbn, &(q->msg), MBN_SEND_NOCREATE | MBN_SEND_FORCEID);
      last = q;
      q = q->next;
    }

    pthread_mutex_unlock((pthread_mutex_t *) mbn->mbn_mutex);
    pthread_testcancel();
    sleep(1);
  }
}


struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info node, struct mbn_object *objects) {
  struct mbn_handler *mbn;
  struct mbn_object *obj;
  int i, l;
  pthread_mutexattr_t mattr;

  mbn = (struct mbn_handler *) calloc(1, sizeof(struct mbn_handler));
  mbn->node = node;
  mbn->node.Services &= 0x7F; /* turn off validated bit */

  /* pad descriptions and name with zero and clear some other things */
  l = strlen((char *)mbn->node.Description);
  if(l < 64)
    memset((void *)&(mbn->node.Description[l]), 0, 64-l);
  l = strlen((char *)mbn->node.Name);
  if(l < 32)
    memset((void *)&(mbn->node.Name[l]), 0, 32-l);

  /* create a copy of the objects, and make some small changes for later use */
  mbn->objects = (struct mbn_object *) malloc(mbn->node.NumberOfObjects*sizeof(struct mbn_object));
  memcpy((void *)mbn->objects, (void *)objects, mbn->node.NumberOfObjects*sizeof(struct mbn_object));
  for(i=0;i<mbn->node.NumberOfObjects;i++) {
    obj = &(mbn->objects[i]);
    if(objects[i].SensorSize > 0) {
      copy_datatype(objects[i].SensorType, objects[i].SensorSize, &(objects[i].SensorMin), &(mbn->objects[i].SensorMin));
      copy_datatype(objects[i].SensorType, objects[i].SensorSize, &(objects[i].SensorMax), &(mbn->objects[i].SensorMax));
      copy_datatype(objects[i].SensorType, objects[i].SensorSize, &(objects[i].SensorData), &(mbn->objects[i].SensorData));
    }
    if(objects[i].ActuatorSize > 0) {
      copy_datatype(objects[i].ActuatorType, objects[i].ActuatorSize, &(objects[i].ActuatorMin), &(mbn->objects[i].ActuatorMin));
      copy_datatype(objects[i].ActuatorType, objects[i].ActuatorSize, &(objects[i].ActuatorMax), &(mbn->objects[i].ActuatorMax));
      copy_datatype(objects[i].ActuatorType, objects[i].ActuatorSize, &(objects[i].ActuatorDefault), &(mbn->objects[i].ActuatorDefault));
      copy_datatype(objects[i].ActuatorType, objects[i].ActuatorSize, &(objects[i].ActuatorData), &(mbn->objects[i].ActuatorData));
    }
    mbn->objects[i].changed = mbn->objects[i].timeout = 0;
    l = strlen((char *)mbn->objects[i].Description);
    if(l < 32)
      memset((void *)&(mbn->objects[i].Description[l]), 0, 32-l);
  }

  /* init the mutex for locking the mbn_handler data.
   * Recursive type, because we call other functions
   *  that may lock the mutex themselves as well */
  mbn->mbn_mutex = malloc(sizeof(pthread_mutex_t));
  mbn->timeout_thread = malloc(sizeof(pthread_t));
  mbn->throttle_thread = malloc(sizeof(pthread_t));
  mbn->msgqueue_thread = malloc(sizeof(pthread_t));
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init((pthread_mutex_t *) mbn->mbn_mutex, &mattr);

  /* initialize address list */
  init_addresses(mbn);

  /* create threads to keep track of timeouts */
  if(    pthread_create((pthread_t *)mbn->timeout_thread,  NULL, node_timeout_thread, (void *) mbn) != 0
      || pthread_create((pthread_t *)mbn->throttle_thread, NULL, throttle_thread,     (void *) mbn) != 0
      || pthread_create((pthread_t *)mbn->msgqueue_thread, NULL, msgqueue_thread,     (void *) mbn) != 0) {
    perror("Error creating threads");
    free(mbn);
    return NULL;
  }

  return mbn;
}


/* IMPORTANT: must not be called in a thread which has a lock on mbn_mutex */
void MBN_EXPORT mbnFree(struct mbn_handler *mbn) {
  int i;

  /* request cancellation for the threads */
  pthread_cancel(*((pthread_t *)mbn->timeout_thread));
  pthread_cancel(*((pthread_t *)mbn->throttle_thread));
  pthread_cancel(*((pthread_t *)mbn->msgqueue_thread));

  /* free interface */
  if(mbn->interface.cb_free != NULL)
    mbn->interface.cb_free(mbn);

  /* free address list */
  free_addresses(mbn);

  /* free objects */
  for(i=0; i<mbn->node.NumberOfObjects; i++) {
    if(mbn->objects[i].SensorSize > 0) {
      free_datatype(mbn->objects[i].SensorType, &(mbn->objects[i].SensorMin));
      free_datatype(mbn->objects[i].SensorType, &(mbn->objects[i].SensorMax));
      free_datatype(mbn->objects[i].SensorType, &(mbn->objects[i].SensorData));
    }
    if(mbn->objects[i].ActuatorSize > 0) {
      free_datatype(mbn->objects[i].ActuatorType, &(mbn->objects[i].ActuatorMin));
      free_datatype(mbn->objects[i].ActuatorType, &(mbn->objects[i].ActuatorMax));
      free_datatype(mbn->objects[i].ActuatorType, &(mbn->objects[i].ActuatorDefault));
      free_datatype(mbn->objects[i].ActuatorType, &(mbn->objects[i].ActuatorData));
    }
  }
  free(mbn->objects);


  /* wait for the threads
   * (make sure no locks on mbn->mbn_mutex are present here) */
  pthread_join(*((pthread_t *)mbn->timeout_thread), NULL);
  pthread_join(*((pthread_t *)mbn->throttle_thread), NULL);
  pthread_join(*((pthread_t *)mbn->msgqueue_thread), NULL);
  pthread_mutex_destroy((pthread_mutex_t *)mbn->mbn_mutex);
  free(mbn->mbn_mutex);
  free(mbn->timeout_thread);
  free(mbn->throttle_thread);
  free(mbn->msgqueue_thread);
}


int process_acknowledge_reply(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_msgqueue *q, *last;

  if(!msg->AcknowledgeReply || msg->MessageID == 0)
    return 0;

  pthread_mutex_lock((pthread_mutex_t *)&(mbn->mbn_mutex));
  /* search for the message ID in our queue */
  for(q=last=mbn->queue; q!=NULL; q=q->next) {
    if(q->id == msg->MessageID)
      break;
    last = q;
  }

  /* found! */
  if(q != NULL && q->id == msg->MessageID) {
    /* send callback (if any) */
    if(mbn->cb_AcknowledgeReply != NULL)
      mbn->cb_AcknowledgeReply(mbn, &(q->msg), msg, q->retries);
    /* ...and remove the message from the queue */
    if(last == NULL)
      mbn->queue = q->next;
    else
      last->next = q->next;
    free_message(&(q->msg));
    free(q);
  }

  pthread_mutex_unlock((pthread_mutex_t *)&(mbn->mbn_mutex));
  return 1;
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
    MBN_ERROR(mbn, MBN_ERROR_PARSE_MESSAGE);
    return;
  }

  if(0)
    MBN_TRACE(printf("Received MambaNet message of %dB from 0x%08lX to 0x%08lX, id 0x%06X, type 0x%04X",
       length, msg.AddressFrom, msg.AddressTo, msg.MessageID, msg.MessageType));

  if(0 && msg.MessageType == MBN_MSGTYPE_ADDRESS)
    MBN_TRACE(printf(" -> Address Reservation from %04X:%04X:%04X, type 0x%02X, engine 0x%08lX, services 0x%02X",
      msg.Data.Address.ManufacturerID, msg.Data.Address.ProductID, msg.Data.Address.UniqueIDPerProduct,
      msg.Data.Address.Type, msg.Data.Address.EngineAddr, msg.Data.Address.Services));

  if(0 && msg.MessageType == MBN_MSGTYPE_OBJECT)
    MBN_TRACE(printf(" -> Object message, action %d, object #%d, datatype %d",
      msg.Data.Object.Action, msg.Data.Object.Number, msg.Data.Object.DataType));

  /* we're going to be accessing the mbn struct, lock! */
  pthread_mutex_lock((pthread_mutex_t *)&(mbn->mbn_mutex));

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

  /* acknowledge reply, yay! */
  if(!processed && process_acknowledge_reply(mbn, &msg) != 0)
    processed++;

  /* object messages */
  if(!processed && process_object_message(mbn, &msg) != 0)
    processed++;

  pthread_mutex_unlock((pthread_mutex_t *)&(mbn->mbn_mutex));
  free_message(&msg);
}


void MBN_EXPORT mbnSendMessage(struct mbn_handler *mbn, struct mbn_message *msg, int flags) {
  unsigned char raw[MBN_MAX_MESSAGE_SIZE];
  struct mbn_address_node *dest;
  struct mbn_msgqueue *q, *n;
  void *ifaddr;
  int r;

  if(mbn->interface.cb_transmit == NULL) {
    MBN_ERROR(mbn, MBN_ERROR_NO_INTERFACE);
    return;
  }

  if(!(flags & MBN_SEND_IGNOREVALID) && !(mbn->node.Services & MBN_ADDR_SERVICES_VALID)) {
    MBN_ERROR(mbn, MBN_ERROR_INVALID_ADDR);
    return;
  }

  /* just forward the raw data to the interface, if we don't need to do any processing */
  if(flags & MBN_SEND_RAWDATA) {
    mbn->interface.cb_transmit(mbn, raw, msg->rawlength, NULL);
    return;
  }

  if(!(flags & MBN_SEND_FORCEADDR))
    msg->AddressFrom = mbn->node.MambaNetAddr;

  /* lock, to make sure we have a unique message ID */
  pthread_mutex_lock((pthread_mutex_t *)&(mbn->mbn_mutex));

  if(!(flags & MBN_SEND_FORCEID)) {
    msg->MessageID = 0;
    if(flags & MBN_SEND_ACKNOWLEDGE) {
      /* get a new message ID */
      msg->MessageID = 1;
      if(mbn->queue != NULL) {
        q = mbn->queue;
        do {
          if(q->id >= msg->MessageID)
            msg->MessageID = q->id+1;
        } while((q = q->next) != NULL);
      }
      MBN_TRACE(printf("Assigning MessageID %06X", msg->MessageID));
    }
  }

  msg->raw = raw;
  msg->rawlength = 0;

  /* create the message */
  if((r = create_message(msg, (flags & MBN_SEND_NOCREATE)?1:0)) != 0) {
    MBN_ERROR(mbn, MBN_ERROR_CREATE_MESSAGE);
    MBN_TRACE(printf("Error creating message: %02X", r));
    pthread_mutex_unlock((pthread_mutex_t *)&(mbn->mbn_mutex));
    return;
  }

  /* save the message to the queue if we need to check for acknowledge replies */
  if(flags & MBN_SEND_ACKNOWLEDGE) {
    /* create struct */
    n = malloc(sizeof(struct mbn_msgqueue));
    n->id = msg->MessageID;
    copy_message(msg, &(n->msg));
    n->retries = 0;
    n->next = NULL;
    /* add to the list */
    if(mbn->queue == NULL)
      mbn->queue = n;
    else {
      q = mbn->queue;
      while(q->next != NULL)
        q = q->next;
      q->next = n;
    }
  }
  pthread_mutex_unlock((pthread_mutex_t *)&(mbn->mbn_mutex));

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


void MBN_EXPORT mbnUpdateNodeName(struct mbn_handler *mbn, char *name) {
  memset((void *)mbn->node.Name, 0, 32);
  memcpy((void *)mbn->node.Name, (void *)name, strlen(name));
}
void MBN_EXPORT mbnUpdateEngineAddr(struct mbn_handler *mbn, unsigned long addr) {
  mbn->node.DefaultEngineAddr = addr;
}
void MBN_EXPORT mbnUpdateServiceRequest(struct mbn_handler *mbn, char srv) {
  mbn->node.ServiceRequest = srv;
}


