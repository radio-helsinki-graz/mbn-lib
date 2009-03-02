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

#include "mbn.h"
#include "address.h"

/* sleep() */
#ifdef MBN_LINUX
# include <unistd.h>
#elif
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif


/* Thread waiting for timeouts */
/* TODO: thread cancellation? !!variable locking!! */
void *node_timeout_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  struct mbn_address_node *node, *last, *tmp;

  while(1) {
    sleep(1);
    node = last = mbn->addresses;
    while(node != NULL) {
      if(--node->Alive <= 0) {
        MBN_TRACE(printf("Removing address table entry for 0x%08lX (timeout)", node->MambaNetAddr));
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
  }
}


/* handle address reservation information packets and update
 * the internal address list */
void process_reservation_information(struct mbn_handler *mbn, struct mbn_message_address *nfo) {
  struct mbn_address_node *node, *last, new;

  /* look for existing node with this address */
  node = mbnNodeStatus(mbn, nfo->MambaNetAddr);

  /* address could've changed, search for UniqueMediaAccessID */
  if(node == NULL && mbn->addresses != NULL) {
    last = mbn->addresses;
    do {
      if(last->ManufacturerID == nfo->ManufacturerID && last->ProductID == nfo->ProductID
          && last->UniqueIDPerProduct == nfo->UniqueIDPerProduct) {
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
      free(node);
      node = NULL;
    }
    MBN_TRACE(printf("Removing address table entry for 0x%08lX", nfo->MambaNetAddr));
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
    MBN_TRACE(printf("Inserting new address table entry for 0x%08lX", nfo->MambaNetAddr));
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
      node->Alive = MBN_ADDR_TIMEOUT;
    }
  }
}


/* Returns nonzero if the message has been processed */
int process_address_message(struct mbn_handler *mbn, struct mbn_message *msg) {
  if(msg->Data.Address.Type == MBN_ADDR_TYPE_INFO)
    process_reservation_information(mbn, &(msg->Data.Address));

  /* TODO: process other message types */

  return 1;
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


