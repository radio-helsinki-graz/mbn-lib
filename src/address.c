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


/* send an address reservation broadcast message */
void send_info(struct mbn_handler *mbn) {
  struct mbn_message msg;
  pthread_mutex_lock(&(mbn->mbn_mutex));
  msg.ControlByte = 0x81;
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
  mbnSendMessage(mbn, &msg);
  mbn->pongtimeout = MBN_ADDR_MSG_TIMEOUT;
  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* Thread waiting for timeouts */
void *node_timeout_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  struct mbn_address_node *node, *last, *tmp;

  while(1) {
    /* working on mbn_handler, so lock */
    pthread_mutex_lock(&(mbn->mbn_mutex));

    /* check the address list */
    node = last = mbn->addresses;
    while(node != NULL) {
      if(--node->Alive <= 0) {
        /* send callback */
        if(mbn->cb_AddressTableChange != NULL)
          mbn->cb_AddressTableChange(mbn, node, NULL);
        /* remove node from list */
        if(last == mbn->addresses)
          mbn->addresses = node->next;
        else
          last->next = node->next;
        tmp = node->next;
        free(node);
        node = tmp;
      }
      last = node;
      node = node->next;
    }

    /* send address reservation information messages, if needed */
    if(!mbn->validated || --mbn->pongtimeout <= 0)
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
  struct mbn_address_node *node, *last, new;

  pthread_mutex_lock(&(mbn->mbn_mutex));

  /* look for existing node with this address */
  node = mbnNodeStatus(mbn, nfo->MambaNetAddr);

  /* address could've changed, search for UniqueMediaAccessID */
  if(node == NULL && mbn->addresses != NULL) {
    last = mbn->addresses;
    do {
      if(MBN_ADDR_EQ(last, nfo)) {
        node = last;
        break;
      }
    } while((last = last->next) != NULL);
  }

  /* we found the node in our list, but its address isn't
   * validated (anymore), so remove it. */
  if(node != NULL && !(nfo->Services & MBN_ADDR_SERVICES_VALID)) {
    /* send callback */
    if(mbn->cb_AddressTableChange != NULL)
      mbn->cb_AddressTableChange(mbn, node, NULL);
    /* remove node */
    if(mbn->addresses == node)
      mbn->addresses = node->next;
    else {
      last = mbn->addresses;
      do {
        if(last->next == node) {
          last->next = node->next;
          break;
        }
      } while((last = last->next) != NULL);
    }
    free(node);
    node = NULL;
  }

  /* not found but validated? insert new node in the table */
  else if(node == NULL) {
    node = calloc(1, sizeof(struct mbn_address_node));
    if(mbn->addresses == NULL)
      mbn->addresses = node;
    else {
      last = mbn->addresses;
      while(last->next != NULL)
        last = last->next;
      last->next = node;
    }
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
      last = mbn->addresses;
      do
        if(last != node && last->ifaddr == node->ifaddr)
          break;
      while((last = last->next) != NULL);
      if(last == NULL)
        mbn->interface.cb_free_addr(node->ifaddr);
    }
    node->ifaddr = ifaddr;
  }

  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* Returns nonzero if the message has been processed */
int process_address_message(struct mbn_handler *mbn, struct mbn_message *msg, void *ifaddr) {
  switch(msg->Data.Address.Type) {
    case MBN_ADDR_TYPE_INFO:
      process_reservation_information(mbn, &(msg->Data.Address), ifaddr);
      break;

    case MBN_ADDR_TYPE_RESPONSE:
      if(MBN_ADDR_EQ(&(msg->Data.Address), &(mbn->node))) {
        pthread_mutex_lock(&(mbn->mbn_mutex));
        mbn->node.MambaNetAddr = msg->Data.Address.MambaNetAddr;
        mbn->node.Services |= 0x80;
        mbn->validated = 1;
        if(mbn->cb_OnlineStatus != NULL)
          mbn->cb_OnlineStatus(mbn, mbn->node.MambaNetAddr, 1);
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


/* free()'s the entire address list */
void free_addresses(struct mbn_handler *mbn) {
  struct mbn_address_node *tmp, *next, *node = mbn->addresses;

  pthread_mutex_lock(&(mbn->mbn_mutex));
  while(node != NULL) {
    /* check for ifaddr and free it */
    if(node->ifaddr != NULL && mbn->interface.cb_free_addr != NULL) {
      next = node;
      while((next = next->next) != NULL)
        if(next->ifaddr == node->ifaddr)
          break;
      if(next == NULL)
        mbn->interface.cb_free_addr(node->ifaddr);
    }
    /* free node and go to next */
    tmp = node->next;
    free(node);
    node = tmp;
  }
  mbn->addresses = NULL;
  pthread_mutex_unlock(&(mbn->mbn_mutex));
}


/* Get information about a node address reservation information given
 * a MambaNet address. Returns NULL if not found. */
struct mbn_address_node * MBN_EXPORT mbnNodeStatus(struct mbn_handler *mbn, unsigned int addr) {
  struct mbn_address_node *node = mbn->addresses;

  if(node == NULL)
    return NULL;
  do {
    if(node->MambaNetAddr == addr)
      return node;
  } while((node = node->next) != NULL);
  return NULL;
}


