
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "mbn.h"
#include "codec.h"

/* sleep() */
#ifdef MBN_LINUX
# include <unistd.h>
#elif
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif



struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info node, struct mbn_interface interface) {
  struct mbn_handler *mbn;

  mbn = (struct mbn_handler *) malloc(sizeof(struct mbn_handler));
  mbn->node = node;
  mbn->interface = interface;
  mbn->interface.data = NULL;

  return mbn;
}


/* Entry point for all incoming MambaNet messages */
void MBN_EXPORT mbnProcessRawMambaNetMessage(struct mbn_handler *mbn, unsigned char *buffer, int length) {
  int r;
  struct mbn_message msg;

  memset(&msg, 0, sizeof(struct mbn_message));
  msg.raw = buffer;
  msg.rawlength = length;

  /* parse message */
  if((r = parse_message(&msg)) != 0) {
    MBN_TRACE(printf("Received invalid message (error %02X), dropping", r));
    return;
  }

  MBN_TRACE(printf("Received MambaNet message of %dB from 0x%08lX to 0x%08lX, ctrl 0x%02X, id 0x%06lX, type 0x%04X, %dB data",
     length, msg.AddressFrom, msg.AddressTo, msg.ControlByte, msg.MessageID, msg.MessageType, msg.DataLength));

  if(msg.MessageType == MBN_MSGTYPE_ADDRESS)
    MBN_TRACE(printf(" -> Address Reservation from %04X:%04X:%04X, type 0x%02X, engine 0x%08lX, services 0x%02X",
      msg.Data.Address.ManufacturerID, msg.Data.Address.ProductID, msg.Data.Address.UniqueIDPerProduct,
      msg.Data.Address.Type, msg.Data.Address.EngineAddr, msg.Data.Address.Services));

  /* TODO: process message and send callbacks */
}


