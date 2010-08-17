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

/* General project-wide TODO list (in addition to `grep TODO *.c`)
 *  - Add more H/W interfaces:
 *    > Serial line?
 *  - Test/port to OS X?
 *  - Test suite?
 *  - Buffering of outgoing packets (to make all mbn* calls non-blocking)
*/


#define _XOPEN_SOURCE 500

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "mbn.h"
#include "address.h"
#include "codec.h"
#include "object.h"

/* sleep() */
#ifdef MBNP_mingw
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#else
# include <unistd.h>
#endif

#define MMTYPE_SIZE(t, s)\
  (t == MBN_DATATYPE_OCTETS || t == MBN_DATATYPE_BITS ? MBN_DATATYPE_UINT : t),\
  (t == MBN_DATATYPE_OCTETS || t == MBN_DATATYPE_BITS ? 1 : s)

#define MMTYPE(t)\
  (t == MBN_DATATYPE_OCTETS || t == MBN_DATATYPE_BITS ? MBN_DATATYPE_UINT : t)

int mbnhandlers = 0;


/* thread that keeps track of messages requiring an acknowledge reply,
 * and retries the message after a timeout (of one second, currently) */
void *msgqueue_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  struct mbn_msgqueue *q, *last, *tmp;

  mbn->msgqueue_run = 1;

  /* this is the only thread that can free() items from the msgqueue list,
   * so we don't have to lock while reading from it */
  while(1) {
    for(q=last=mbn->queue; q!=NULL; ) {
      /* Remove item from the list */
      if(q->retries == -1 || q->retries++ >= MBN_ACKNOWLEDGE_RETRIES) {
        /* send callback if the message timed out */
        if(q->retries >= 0 && mbn->cb_AcknowledgeTimeout != NULL)
          mbn->cb_AcknowledgeTimeout(mbn, &(q->msg));
        /* remove item from the queue */
        LCK();
        if(last == mbn->queue) {
          mbn->queue = last = q->next;
        } else
          last->next = q->next;
        tmp = q->next;
        free_message(&(q->msg));
        free(q);
        q = tmp;
        ULCK();
        continue;
      }
      /* Wait a sec. */
      if(q->retries == 1)
        continue;
      /* No reply yet, let's try again */
      mbnSendMessage(mbn, &(q->msg), MBN_SEND_NOCREATE | MBN_SEND_FORCEID);
      last = q;
      q = q->next;
    }

    pthread_testcancel();
    sleep(1);
  }
}


struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info *node, struct mbn_object *objects, struct mbn_interface *itf, char *err) {
  struct mbn_handler *mbn;
  struct mbn_object *obj;
  int i, l;

#ifdef PTW32_STATIC_LIB
  if(!mbnhandlers++)
    pthread_win32_process_attach_np();
#endif

  if(itf == NULL) {
    sprintf(err, "No interface specified");
    return NULL;
  }

#ifdef MBN_MANUFACTURERID
  if(node->ManufacturerID != 0xFFFF && node->ManufacturerID != MBN_MANUFACTURERID) {
    sprintf(err, "This library has been built to only allow ManufacturerID %d", MBN_MANUFACTURERID);
    return NULL;
  }
#endif

  mbn = (struct mbn_handler *) calloc(1, sizeof(struct mbn_handler));
  memcpy((void *)&(mbn->node), (void *)node, sizeof(struct mbn_node_info));
  mbn->node.Services &= 0x7F; /* turn off validated bit */
  mbn->itf = itf;
  itf->mbn = mbn;

  /* pad descriptions and name with zero and clear some other things */
  l = strlen(mbn->node.Description);
  if(l < 64)
    memset((void *)&(mbn->node.Description[l]), 0, 64-l);
  l = strlen(mbn->node.Name);
  if(l < 32)
    memset((void *)&(mbn->node.Name[l]), 0, 32-l);

  /* create a copy of the objects, and make some small changes for later use */
  if(objects) {
    mbn->objects = (struct mbn_object *) malloc(mbn->node.NumberOfObjects*sizeof(struct mbn_object));
    memcpy((void *)mbn->objects, (void *)objects, mbn->node.NumberOfObjects*sizeof(struct mbn_object));
    for(i=0;i<mbn->node.NumberOfObjects;i++) {
      obj = &(mbn->objects[i]);
      if(objects[i].SensorSize > 0) {
        copy_datatype(MMTYPE_SIZE(objects[i].SensorType, objects[i].SensorSize), &(objects[i].SensorMin), &(mbn->objects[i].SensorMin));
        copy_datatype(MMTYPE_SIZE(objects[i].SensorType, objects[i].SensorSize), &(objects[i].SensorMax), &(mbn->objects[i].SensorMax));
        copy_datatype(objects[i].SensorType, objects[i].SensorSize, &(objects[i].SensorData), &(mbn->objects[i].SensorData));
      }
      if(objects[i].ActuatorSize > 0) {
        copy_datatype(MMTYPE_SIZE(objects[i].ActuatorType, objects[i].ActuatorSize), &(objects[i].ActuatorMin), &(mbn->objects[i].ActuatorMin));
        copy_datatype(MMTYPE_SIZE(objects[i].ActuatorType, objects[i].ActuatorSize), &(objects[i].ActuatorMax), &(mbn->objects[i].ActuatorMax));
        copy_datatype(MMTYPE_SIZE(objects[i].ActuatorType, objects[i].ActuatorSize), &(objects[i].ActuatorDefault), &(mbn->objects[i].ActuatorDefault));
        copy_datatype(objects[i].ActuatorType, objects[i].ActuatorSize, &(objects[i].ActuatorData), &(mbn->objects[i].ActuatorData));
      }
      mbn->objects[i].changed = mbn->objects[i].timeout = 0;
      l = strlen(mbn->objects[i].Description);
      if(l < 32)
        memset((void *)&(mbn->objects[i].Description[l]), 0, 32-l);
      mbn->objects[i].Services = mbn->objects[i].SensorType != MBN_DATATYPE_NODATA ? 0x03 : 0x00;
    }
  } else
    mbn->node.NumberOfObjects = 0;

  /* init and allocate some pthread objects */
  mbn->mbn_mutex = malloc(sizeof(pthread_mutex_t));
  mbn->timeout_thread = malloc(sizeof(pthread_t));
  mbn->throttle_thread = malloc(sizeof(pthread_t));
  mbn->msgqueue_thread = malloc(sizeof(pthread_t));
  pthread_mutex_init((pthread_mutex_t *) mbn->mbn_mutex, NULL);

  /* initialize address list */
  init_addresses(mbn);

  /* create threads to keep track of timeouts */
  if(    (i = pthread_create((pthread_t *)mbn->timeout_thread,  NULL, node_timeout_thread, (void *) mbn)) != 0
      || (i = pthread_create((pthread_t *)mbn->throttle_thread, NULL, throttle_thread,     (void *) mbn)) != 0
      || (i = pthread_create((pthread_t *)mbn->msgqueue_thread, NULL, msgqueue_thread,     (void *) mbn)) != 0) {
    sprintf("Can't create thread: %s (%d)", strerror(i), i);
    free(mbn);
    return NULL;
  }

  return mbn;
}

void MBN_EXPORT mbnStartInterface(struct mbn_interface *itf, char *err) {
  /* init interface */
  if(itf->cb_init != NULL)
  {
    itf->cb_init(itf, err);
  }
}

/* IMPORTANT: must not be called in a thread which has a lock on mbn_mutex */
void MBN_EXPORT mbnFree(struct mbn_handler *mbn) {
  int i;

  /* disable all callbacks so the application won't see all kinds of activities
   * while we're freeing everything */
  mbn->cb_ReceiveMessage = NULL;
  mbn->cb_AddressTableChange = NULL;
  mbn->cb_WriteLogMessage = NULL;
  mbn->cb_OnlineStatus = NULL;
  mbn->cb_NameChange = NULL;
  mbn->cb_DefaultEngineAddrChange = NULL;
  mbn->cb_SetActuatorData = NULL;
  mbn->cb_GetSensorData = NULL;
  mbn->cb_ObjectFrequencyChange = NULL;
  mbn->cb_ObjectInformationResponse = NULL;
  mbn->cb_ObjectFrequencyResponse = NULL;
  mbn->cb_SensorDataResponse = NULL;
  mbn->cb_SensorDataChanged = NULL;
  mbn->cb_ActuatorDataResponse = NULL;
  mbn->cb_ObjectError = NULL;
  mbn->cb_Error = NULL;
  mbn->cb_AcknowledgeTimeout = NULL;
  mbn->cb_AcknowledgeReply = NULL;

  /* wait for the threads to be running
   * (normally they should be running right after mbnInit(),
   *  but there can be some slight lag on pthread-win32) */
  for(i=0; !mbn->msgqueue_run || !mbn->timeout_run || !mbn->throttle_run; i++) {
    if(i > 10)
      break; /* shouldn't happen, but silently ignore if it somehow does. */
    sleep(1);
  }

  /* Stop the interface receiving */
  if(mbn->itf->cb_stop != NULL)
    mbn->itf->cb_stop(mbn->itf);

  /* request cancellation for the threads */
  pthread_cancel(*((pthread_t *)mbn->timeout_thread));
  pthread_cancel(*((pthread_t *)mbn->throttle_thread));
  pthread_cancel(*((pthread_t *)mbn->msgqueue_thread));

  /* wait for the threads
   * (make sure no locks on mbn->mbn_mutex are present here) */
  pthread_join(*((pthread_t *)mbn->timeout_thread), NULL);
  pthread_join(*((pthread_t *)mbn->throttle_thread), NULL);
  pthread_join(*((pthread_t *)mbn->msgqueue_thread), NULL);
  free(mbn->timeout_thread);
  free(mbn->throttle_thread);
  free(mbn->msgqueue_thread);

  /* free address list */
  free_addresses(mbn);

  /* free interface */
  if(mbn->itf->cb_free != NULL)
    mbn->itf->cb_free(mbn->itf);

  /* free objects */
  for(i=0; i<mbn->node.NumberOfObjects; i++) {
    if(mbn->objects[i].SensorSize > 0) {
      free_datatype(MMTYPE(mbn->objects[i].SensorType), &(mbn->objects[i].SensorMin));
      free_datatype(MMTYPE(mbn->objects[i].SensorType), &(mbn->objects[i].SensorMax));
      free_datatype(mbn->objects[i].SensorType, &(mbn->objects[i].SensorData));
    }
    if(mbn->objects[i].ActuatorSize > 0) {
      free_datatype(MMTYPE(mbn->objects[i].ActuatorType), &(mbn->objects[i].ActuatorMin));
      free_datatype(MMTYPE(mbn->objects[i].ActuatorType), &(mbn->objects[i].ActuatorMax));
      free_datatype(MMTYPE(mbn->objects[i].ActuatorType), &(mbn->objects[i].ActuatorDefault));
      free_datatype(mbn->objects[i].ActuatorType, &(mbn->objects[i].ActuatorData));
    }
  }
  free(mbn->objects);

  /* and get rid of our mutex */
  pthread_mutex_destroy((pthread_mutex_t *)mbn->mbn_mutex);
  free(mbn->mbn_mutex);
  free(mbn);

#ifdef PTW32_STATIC_LIB
  /* attach_np() doesn't seem to work again after we've
   * detached, so let's not call this function...
  if(--mbnhandlers == 0)
    pthread_win32_process_detach_np();*/
#endif
}


int process_acknowledge_reply(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_msgqueue *q;
  struct mbn_message orig;
  int ret = 1, tries = -1;

  if(!msg->AcknowledgeReply || msg->MessageID == 0)
    return 0;

  LCK();
  /* search for the message ID in our queue */
  for(q=mbn->queue; q!=NULL; q=q->next) {
    if(q->id == msg->MessageID)
      break;
  }

  /* found! */
  if(q != NULL && q->id == msg->MessageID) {
    /* make a copy for the callback */
    copy_message(&(q->msg), &orig);
    tries = q->retries;
    /* determine whether we need to process this message further,
     * If the original message is a GET action, then we should continue processing */
    if(q->msg.MessageType == MBN_MSGTYPE_OBJECT) {
      switch(q->msg.Message.Object.Action) {
        case MBN_OBJ_ACTION_GET_INFO:
        case MBN_OBJ_ACTION_GET_ENGINE:
        case MBN_OBJ_ACTION_GET_FREQUENCY:
        case MBN_OBJ_ACTION_GET_SENSOR:
        case MBN_OBJ_ACTION_GET_ACTUATOR:
          ret = 0;
          break;
        default:
          ret = 1;
      }
    }
    /* ...and signal the msgqueue thread to free() the message */
    q->retries = -1;
  }
  ULCK();

  /* send callback (if any) */
  if(tries >= 0 && mbn->cb_AcknowledgeReply != NULL) {
    mbn->cb_AcknowledgeReply(mbn, &orig, msg, tries);
    free_message(&orig);
  }
  return ret;
}


/* Entry point for all incoming MambaNet messages */
void MBN_EXPORT mbnProcessRawMessage(struct mbn_interface *itf, unsigned char *buffer, int length, void *ifaddr) {
  struct mbn_handler *mbn = itf->mbn;
  int r, processed = 0;
  struct mbn_message msg;
  char err[MBN_ERRSIZE];

  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.raw = buffer;
  msg.rawlength = length;

  /* parse message */
  if((r = parse_message(&msg)) != 0) {
    if(mbn->cb_Error) {
      sprintf(err, "Couldn't parse incoming message (%d)", r);
      mbn->cb_Error(mbn, MBN_ERROR_PARSE_MESSAGE, err);
    }
    return;
  }

  /* Oh my, the interface is echoing back packets, let's ignore them */
  if((mbn->node.Services & MBN_ADDR_SERVICES_VALID) && msg.AddressFrom == mbn->node.MambaNetAddr)
    processed++;

  /* send ReceiveMessage() callback, and stop processing if it returned non-zero */
  if(!processed && mbn->cb_ReceiveMessage != NULL && mbn->cb_ReceiveMessage(mbn, &msg) != 0)
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

  free_message(&msg);
}


void MBN_EXPORT mbnSendMessage(struct mbn_handler *mbn, struct mbn_message *msg, int flags) {
  unsigned char raw[MBN_MAX_MESSAGE_SIZE];
  char err[MBN_ERRSIZE];
  struct mbn_address_node *dest;
  struct mbn_msgqueue *q, *n, *prev_q;
  void *ifaddr;
  int r;

  if(mbn->itf->cb_transmit == NULL) {
    if(mbn->cb_Error) {
      sprintf(err, "Registered interface can't send messages");
      mbn->cb_Error(mbn, MBN_ERROR_NO_INTERFACE, err);
    }
    return;
  }

  if(!(flags & MBN_SEND_IGNOREVALID) && !(mbn->node.Services & MBN_ADDR_SERVICES_VALID)) {
    if(mbn->cb_Error) {
      sprintf(err, "Can't send message: we don't have a validated MambaNet address");
      mbn->cb_Error(mbn, MBN_ERROR_INVALID_ADDR, err);
    }
    return;
  }

  /* just forward the raw data to the interface, if we don't need to do any processing */
  if(flags & MBN_SEND_RAWDATA) {
    if(mbn->itf->cb_transmit(mbn->itf, raw, msg->rawlength, NULL, err) != 0) {
      if(mbn->cb_Error)
        mbn->cb_Error(mbn, MBN_ERROR_ITF_WRITE, err);
    }
    return;
  }

  if(!(flags & MBN_SEND_FORCEADDR))
    msg->AddressFrom = mbn->node.MambaNetAddr;

  /* lock, to make sure we have a unique message ID */
  LCK();

  if(!(flags & MBN_SEND_FORCEID) && !msg->AcknowledgeReply) {
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
    }
  }

  msg->raw = raw;
  msg->rawlength = 0;

  /* create the message */
  if((r = create_message(msg, (flags & MBN_SEND_NOCREATE)?1:0)) != 0) {
    ULCK();
    if(mbn->cb_Error) {
      sprintf(err, "Couldn't create message (%d)", r);
      mbn->cb_Error(mbn, MBN_ERROR_CREATE_MESSAGE, err);
    }
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
      prev_q = NULL;
      q = mbn->queue;
      while(q != NULL) {
        /* check for duplicate retry message, if found replace object-data*/
        if ((q->msg.AddressTo == n->msg.AddressTo) &&
            (q->msg.MessageType == MBN_MSGTYPE_OBJECT) &&
            (n->msg.MessageType == MBN_MSGTYPE_OBJECT) &&
            (q->msg.Message.Object.Action == n->msg.Message.Object.Action) &&
            (q->msg.Message.Object.Number == n->msg.Message.Object.Number)) {
          n->next = q->next;
          if (prev_q == NULL) {
            mbn->queue = n;
          } else {
            prev_q->next = n;
          }
          break;
        }
        prev_q = q;
        q = q->next;
      }
      if (q == NULL) {
        prev_q->next = n;
      } else {
        free_message(&(q->msg));
        free(q);
      }
    }
  }
  ULCK();

  /* determine interface address */
  if(msg->AddressTo == MBN_BROADCAST_ADDRESS)
    ifaddr = NULL;
  else {
    if((dest = mbnNodeStatus(mbn, msg->AddressTo)) == NULL)
      ifaddr = NULL;
    else
      ifaddr = dest->ifaddr;
  }

  /* send the data to the interface transmit callback */
  if(mbn->itf->cb_transmit(mbn->itf, raw, msg->rawlength, ifaddr, err) != 0) {
    if(mbn->cb_Error)
      mbn->cb_Error(mbn, MBN_ERROR_ITF_WRITE, err);
  }
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

void MBN_EXPORT mbnWriteLogMessage(struct mbn_interface *itf, const char *fmt, ...) {
  struct mbn_handler *mbn = itf->mbn;
  if(mbn->cb_WriteLogMessage != NULL) {
    va_list ap;
    char buf[500];
    va_start(ap, fmt);
    vsnprintf(buf, 500, fmt, ap);
    va_end(ap);

    mbn->cb_WriteLogMessage(mbn, buf);
  }
}

const char *MBN_EXPORT mbnVersion() {
  return "MambaNet Library V1.4 - 17 August 2010";
}
