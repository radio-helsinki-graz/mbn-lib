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
#ifdef MBNP_mingw
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#else
# include <unistd.h>
#endif


void init_addresses(struct mbn_handler *mbn) {
  mbn->addrsize = 32;
  mbn->addresses = calloc(mbn->addrsize, sizeof(struct mbn_address_node));
}


void remove_node(struct mbn_handler *mbn, struct mbn_address_node *node) {
  int i;

  /* send callback */
  if(mbn->cb_AddressTableChange != NULL)
    mbn->cb_AddressTableChange(mbn, node, NULL);

  /* remove node (and free the ifaddr pointer) */
  node->used = 0;
  if(node->ifaddr != 0 && mbn->itf->cb_free_addr != NULL) {
    for(i=0; i<mbn->addrsize; i++)
      if(mbn->addresses[i].used && &(mbn->addresses[i]) != node && mbn->addresses[i].ifaddr == node->ifaddr)
        break;
    if(i >= mbn->addrsize)
      mbn->itf->cb_free_addr(node->ifaddr);
  }
}


/* TODO: The struct returned by the following two functions could be marked
 *  unused by an other thread while the application still uses the data, giving
 *  unexpected results. Make a copy of the node? */

/* Get information about a node address reservation information given
 * a MambaNet address. Returns NULL if not found. */
struct mbn_address_node * MBN_EXPORT mbnNodeStatus(struct mbn_handler *mbn, unsigned long addr) {
  int i;

  for(i=0; i<mbn->addrsize; i++)
    if(mbn->addresses[i].used && mbn->addresses[i].MambaNetAddr == addr)
      return &(mbn->addresses[i]);
  return NULL;
}


/* get the address node in the list after the one pointed to
 * by the argument, fetch the first node in the list if
 * node == NULL. Returns NULL if node isn't found, or end
 * of list has been reached */
struct mbn_address_node * MBN_EXPORT mbnNextNode(struct mbn_handler *mbn, struct mbn_address_node *node) {
  int i, next = node == NULL ? 1 : 0;

  for(i=0; i<mbn->addrsize; i++) {
    if(next && mbn->addresses[i].used)
      return &(mbn->addresses[i]);
    if(&(mbn->addresses[i]) == node)
      next = 1;
  }
  return NULL;
}


/* free()'s the entire address list */
void free_addresses(struct mbn_handler *mbn) {
  int i, j;

  /* free all ifaddr pointers */
  for(i=0; i<mbn->addrsize; i++) {
    if(mbn->addresses[i].used && mbn->addresses[i].ifaddr != NULL) {
      for(j=i+1; i<mbn->addrsize; i++)
        if(mbn->addresses[j].used && mbn->addresses[j].ifaddr == mbn->addresses[j].ifaddr)
          break;
      if(j >= mbn->addrsize)
        mbn->itf->cb_free_addr(mbn->addresses[i].ifaddr);
    }
  }
  /* free the array */
  free(mbn->addresses);
  mbn->addrsize = 0;
  mbn->addresses = NULL;
}


/* send an address reservation broadcast message */
void send_info(struct mbn_handler *mbn) {
  struct mbn_message msg;
  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo   = MBN_BROADCAST_ADDRESS;
  msg.AddressFrom = mbn->node.MambaNetAddr;
  msg.MessageID   = 0;
  msg.MessageType = MBN_MSGTYPE_ADDRESS;
  msg.Message.Address.Action             = MBN_ADDR_ACTION_INFO;
  msg.Message.Address.ManufacturerID     = mbn->node.ManufacturerID;
  msg.Message.Address.ProductID          = mbn->node.ProductID;
  msg.Message.Address.UniqueIDPerProduct = mbn->node.UniqueIDPerProduct;
  msg.Message.Address.MambaNetAddr       = mbn->node.MambaNetAddr;
  msg.Message.Address.EngineAddr         = mbn->node.DefaultEngineAddr;
  msg.Message.Address.Services           = mbn->node.Services;
  mbnSendMessage(mbn, &msg, MBN_SEND_IGNOREVALID);

  mbn->pongtimeout = MBN_ADDR_MSG_TIMEOUT;
}


/* Thread waiting for timeouts */
void *node_timeout_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  int i;

  mbn->timeout_run = 1;

  while(1) {
    sleep(1);
    pthread_testcancel();

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
  }
}


/* handle address reservation information packets and update
 * the internal address list */
void process_reservation_information(struct mbn_handler *mbn, struct mbn_message_address *nfo, void *ifaddr) {
  struct mbn_address_node *node, new;
  int i;

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
    if(node->ifaddr != NULL && ifaddr != node->ifaddr && mbn->itf->cb_free_addr != NULL) {
      /* check for nodes with the same pointer, and free the memory if none found */
      for(i=0; i<mbn->addrsize; i++)
        if(mbn->addresses[i].used && mbn->addresses[i].ifaddr == node->ifaddr)
          break;
      if(i >= mbn->addrsize)
        mbn->itf->cb_free_addr(node->ifaddr);
    }
    node->ifaddr = ifaddr;
  }
}


/* Returns nonzero if the message has been processed */
int process_address_message(struct mbn_handler *mbn, struct mbn_message *msg, void *ifaddr) {
  if(msg->MessageType != MBN_MSGTYPE_ADDRESS)
    return 0;

  switch(msg->Message.Address.Action) {
    case MBN_ADDR_ACTION_INFO:
      process_reservation_information(mbn, &(msg->Message.Address), ifaddr);
      break;

    case MBN_ADDR_ACTION_RESPONSE:
      if(MBN_ADDR_EQ(&(msg->Message.Address), &(mbn->node))) {
        /* check for mambanet address/valid bit change */
        if(mbn->node.MambaNetAddr != msg->Message.Address.MambaNetAddr) {
          mbn->node.MambaNetAddr = msg->Message.Address.MambaNetAddr;
          if(msg->Message.Address.Services & MBN_ADDR_SERVICES_VALID && mbn->node.MambaNetAddr > 0)
            mbn->node.Services |= MBN_ADDR_SERVICES_VALID;
          else
            mbn->node.Services &= ~MBN_ADDR_SERVICES_VALID;
          if(mbn->cb_OnlineStatus != NULL)
            mbn->cb_OnlineStatus(mbn, mbn->node.MambaNetAddr, mbn->node.Services & MBN_ADDR_SERVICES_VALID);
        }
        /* check for engine address change */
        if(mbn->node.DefaultEngineAddr != msg->Message.Address.EngineAddr) {
          mbn->node.DefaultEngineAddr = msg->Message.Address.EngineAddr;
          if(mbn->cb_DefaultEngineAddrChange != NULL)
            mbn->cb_DefaultEngineAddrChange(mbn, mbn->node.DefaultEngineAddr);
        }
        /* always reply with a broadcast */
        send_info(mbn);
      }
      break;

    case MBN_ADDR_ACTION_PING:
      if(MBN_ADDR_EQ(&(msg->Message.Address), &(mbn->node)) &&
          (msg->Message.Address.MambaNetAddr == 0 || msg->Message.Address.MambaNetAddr == mbn->node.MambaNetAddr) &&
          (msg->Message.Address.EngineAddr   == 0 || msg->Message.Address.EngineAddr   == mbn->node.DefaultEngineAddr)) {
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
  msg.Message.Address.Action = MBN_ADDR_ACTION_PING;
  mbnSendMessage(mbn, &msg, MBN_SEND_IGNOREVALID);
}


/* Force the use of a MambaNet Address */
void MBN_EXPORT mbnForceAddress(struct mbn_handler *mbn, unsigned long addr) {
  mbn->node.MambaNetAddr = addr;
  mbn->node.Services |= MBN_ADDR_SERVICES_VALID;
  send_info(mbn);
}


