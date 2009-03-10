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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "mbn.h"
#include "address.h"

/* sleep() */
#ifdef MBN_LINUX
# include <unistd.h>
#elif
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif


void init_addresses(struct mbn_handler *mbn) {
  pthread_mutex_lock(&(mbn->mbn_mutex));
  mbn->addrsize = 32;
  mbn->addresses = calloc(mbn->addrsize, sizeof(struct mbn_address_node));
  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


void remove_node(struct mbn_handler *mbn, struct mbn_address_node *node) {
  int i;
  pthread_mutex_lock(&(mbn->mbn_mutex));

  /* send callback */
  if(mbn->cb_AddressTableChange != NULL)
    mbn->cb_AddressTableChange(mbn, node, NULL);

  /* remove node (and free the ifaddr pointer) */
  node->used = 0;
  if(node->ifaddr != 0 && mbn->interface.cb_free_addr != NULL) {
    for(i=0; i<mbn->addrsize; i++)
      if(mbn->addresses[i].used && &(mbn->addresses[i]) != node && mbn->addresses[i].ifaddr == node->ifaddr)
        break;
    if(i >= mbn->addrsize)
      mbn->interface.cb_free_addr(node->ifaddr);
  }
  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* Get information about a node address reservation information given
 * a MambaNet address. Returns NULL if not found. */
struct mbn_address_node * MBN_EXPORT mbnNodeStatus(struct mbn_handler *mbn, unsigned long addr) {
  int i;

  for(i=0; i<mbn->addrsize; i++)
    if(mbn->addresses[i].used && mbn->addresses[i].MambaNetAddr == addr)
      return &(mbn->addresses[i]);
  return NULL;
}


/* free()'s the entire address list */
void free_addresses(struct mbn_handler *mbn) {
  int i, j;
  pthread_mutex_lock(&(mbn->mbn_mutex));

  /* free all ifaddr pointers */
  for(i=0; i<mbn->addrsize; i++) {
    if(mbn->addresses[i].used && mbn->addresses[i].ifaddr != NULL) {
      for(j=i+1; i<mbn->addrsize; i++)
        if(mbn->addresses[j].used && mbn->addresses[j].ifaddr == mbn->addresses[j].ifaddr)
          break;
      if(j >= mbn->addrsize)
        mbn->interface.cb_free_addr(mbn->addresses[i].ifaddr);
    }
  }
  /* free the array */
  free(mbn->addresses);
  mbn->addrsize = 0;
  mbn->addresses = NULL;
  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* send an address reservation broadcast message */
void send_info(struct mbn_handler *mbn) {
  struct mbn_message msg;
  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo   = MBN_BROADCAST_ADDRESS;
  msg.AddressFrom = mbn->node.MambaNetAddr;
  msg.MessageID   = 0;
  msg.MessageType = MBN_MSGTYPE_ADDRESS;
  msg.Data.Address.Type               = MBN_ADDR_TYPE_INFO;
  msg.Data.Address.ManufacturerID     = mbn->node.ManufacturerID;
  msg.Data.Address.ProductID          = mbn->node.ProductID;
  msg.Data.Address.UniqueIDPerProduct = mbn->node.UniqueIDPerProduct;
  msg.Data.Address.MambaNetAddr       = mbn->node.MambaNetAddr;
  msg.Data.Address.EngineAddr         = mbn->node.DefaultEngineAddr;
  msg.Data.Address.Services           = mbn->node.Services;
  mbnSendMessage(mbn, &msg, MBN_SEND_IGNOREVALID);

  pthread_mutex_lock(&(mbn->mbn_mutex));
  mbn->pongtimeout = MBN_ADDR_MSG_TIMEOUT;
  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* Thread waiting for timeouts */
void *node_timeout_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  int i;

  while(1) {
    /* working on mbn_handler, so lock */
    pthread_mutex_lock(&(mbn->mbn_mutex));

    /* check the address list */
    for(i=0; i<mbn->addrsize; i++) {
      if(!mbn->addresses[i].used)
        continue;

      if(--mbn->addresses[i].Alive > 0)
        continue;

      /* if we're here, it means this node timed out - remove it */
      remove_node(mbn, &(mbn->addresses[i]));
    }

    /* send address reservation information messages, if needed */
    if(!(mbn->node.Services & MBN_ADDR_SERVICES_VALID) || --mbn->pongtimeout <= 0)
      send_info(mbn);

    pthread_mutex_unlock(&(mbn->mbn_mutex));

    /* we can safely cancel here */
    pthread_testcancel();

    /* sleep() is our method of timing
     * (not really precise, but good enough) */
    sleep(1);
  }
}


/* handle address reservation information packets and update
 * the internal address list */
void process_reservation_information(struct mbn_handler *mbn, struct mbn_message_address *nfo, void *ifaddr) {
  struct mbn_address_node *node, new;
  int i;

  pthread_mutex_lock(&(mbn->mbn_mutex));

  /* look for existing node with this address */
  node = mbnNodeStatus(mbn, nfo->MambaNetAddr);

  /* address could've changed, search for UniqueMediaAccessID */
  if(node == NULL && mbn->addresses != NULL) {
    for(i=0; i<mbn->addrsize; i++)
      if(mbn->addresses[i].used && MBN_ADDR_EQ(&(mbn->addresses[i]), nfo)) {
        node = &(mbn->addresses[i]);
        break;
      }
  }

  /* we found the node in our list, but its address isn't
   * validated (anymore), so remove it. */
  if(node != NULL && !(nfo->Services & MBN_ADDR_SERVICES_VALID)) {
    remove_node(mbn, node);
    node = NULL;
  }

  /* not found but validated? insert new node in the table */
  else if(node == NULL && (nfo->Services & MBN_ADDR_SERVICES_VALID)) {
    /* look for some free space */
    for(i=0; i<mbn->addrsize; i++)
      if(!mbn->addresses[i].used)
        break;
    /* none found, allocate new memory */
    if(i >= mbn->addrsize) {
      mbn->addresses = realloc(mbn->addresses, mbn->addrsize*2*sizeof(struct mbn_address_node));
      memset((void *)&(mbn->addresses[mbn->addrsize]), 0, mbn->addrsize*sizeof(struct mbn_address_node));
      i = mbn->addrsize;
      mbn->addrsize *= 2;
    }
    node = &(mbn->addresses[i]);
    node->MambaNetAddr = 0;
    node->used = 1;
  }

  if(node != NULL) {
    memcpy((void *)&new, (void *)node, sizeof(struct mbn_address_node));
    new.ManufacturerID = nfo->ManufacturerID;
    new.ProductID = nfo->ProductID;
    new.UniqueIDPerProduct = nfo->UniqueIDPerProduct;
    new.MambaNetAddr = nfo->MambaNetAddr;
    new.EngineAddr = nfo->EngineAddr;
    new.Services = nfo->Services;
    /* something changed, update & send callback */
    if(memcmp((void *)&new, (void *)node, sizeof(struct mbn_address_node)) != 0) {
      if(mbn->cb_AddressTableChange != NULL)
        mbn->cb_AddressTableChange(mbn, node->MambaNetAddr == 0 ? NULL : node, &new);
      memcpy((void *)node, (void *)&new, sizeof(struct mbn_address_node));
    }
    node->Alive = MBN_ADDR_TIMEOUT;
    /* update hardware address */
    if(node->ifaddr != NULL && ifaddr != node->ifaddr && mbn->interface.cb_free_addr != NULL) {
      /* check for nodes with the same pointer, and free the memory if none found */
      for(i=0; i<mbn->addrsize; i++)
        if(mbn->addresses[i].used && mbn->addresses[i].ifaddr == node->ifaddr)
          break;
      if(i >= mbn->addrsize)
        mbn->interface.cb_free_addr(node->ifaddr);
    }
    node->ifaddr = ifaddr;
  }

  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* Returns nonzero if the message has been processed */
int process_address_message(struct mbn_handler *mbn, struct mbn_message *msg, void *ifaddr) {
  if(msg->MessageType != MBN_MSGTYPE_ADDRESS)
    return 0;

  switch(msg->Data.Address.Type) {
    case MBN_ADDR_TYPE_INFO:
      process_reservation_information(mbn, &(msg->Data.Address), ifaddr);
      break;

    case MBN_ADDR_TYPE_RESPONSE:
      if(MBN_ADDR_EQ(&(msg->Data.Address), &(mbn->node))) {
        pthread_mutex_lock(&(mbn->mbn_mutex));
        /* check for mambanet address/valid bit change */
        if(mbn->node.MambaNetAddr != msg->Data.Address.MambaNetAddr || !(mbn->node.Services & MBN_ADDR_SERVICES_VALID)) {
          mbn->node.MambaNetAddr = msg->Data.Address.MambaNetAddr;
          mbn->node.Services |= MBN_ADDR_SERVICES_VALID;
          if(mbn->cb_OnlineStatus != NULL)
            mbn->cb_OnlineStatus(mbn, mbn->node.MambaNetAddr, 1);
        }
        /* check for engine address change */
        if(mbn->node.DefaultEngineAddr != msg->Data.Address.EngineAddr) {
          mbn->node.DefaultEngineAddr = msg->Data.Address.EngineAddr;
          if(mbn->cb_DefaultEngineAddrChange != NULL)
            mbn->cb_DefaultEngineAddrChange(mbn, mbn->node.DefaultEngineAddr);
        }
        /* always reply with a broadcast */
        send_info(mbn);
        pthread_mutex_unlock(&(mbn->mbn_mutex));
      }
      break;

    case MBN_ADDR_TYPE_PING:
      if(MBN_ADDR_EQ(&(msg->Data.Address), &(mbn->node)) &&
          (msg->Data.Address.MambaNetAddr == 0 || msg->Data.Address.MambaNetAddr == mbn->node.MambaNetAddr) &&
          (msg->Data.Address.EngineAddr   == 0 || msg->Data.Address.EngineAddr   == mbn->node.DefaultEngineAddr)) {
        send_info(mbn);
      }
      break;

    default:
      return 0;
  }

  return 1;
}


void MBN_EXPORT mbnSendPingRequest(struct mbn_handler *mbn, unsigned long addr) {
  struct mbn_message msg;

  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo = addr;
  msg.MessageType = MBN_MSGTYPE_ADDRESS;
  msg.Data.Address.Type = MBN_ADDR_TYPE_PING;
  mbnSendMessage(mbn, &msg, MBN_SEND_IGNOREVALID);
}


