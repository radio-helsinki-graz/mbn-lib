
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "mbn.h"

/* sleep() */
#ifdef MBN_LINUX
# include <unistd.h>
#elif
# include <windows.h>
# define sleep(x) Sleep(x*1000)
#endif



struct mbn_handler * MBN_EXPORT mbnInit(struct mbn_node_info node, struct mbn_interface interface) {
  int r;
  pthread_t thr;
  struct mbn_handler *mbn;

  mbn = (struct mbn_handler *) malloc(sizeof(struct mbn_handler));
  mbn->node = node;
  mbn->interface = interface;
  mbn->interface.data = NULL;

  return mbn;
}


/* Entry point for all incoming MambaNet messages */
void MBN_EXPORT mbnProcessRawMambaNetMessage(struct mbn_handler *mbn, unsigned char *buffer, int length) {
  struct mbn_message *msg;

  msg = malloc(sizeof(struct mbn_message));
  msg->buffer = buffer;
  msg->bufferlength = length;

  /* decode MambaNet header */
  msg->ControlByte = buffer[0];
  msg->AddressTo    = ((unsigned long)  buffer[ 0]<<28) & 0x10000000;
  msg->AddressTo   |= ((unsigned long)  buffer[ 1]<<21) & 0x0FE00000;
  msg->AddressTo   |= ((unsigned long)  buffer[ 2]<<14) & 0x001FC000;
  msg->AddressTo   |= ((unsigned long)  buffer[ 3]<< 7) & 0x00003F80;
  msg->AddressTo   |= ((unsigned long)  buffer[ 4]    ) & 0x0000007F;
  msg->AddressFrom  = ((unsigned long)  buffer[ 5]<<21) & 0x0FE00000;
  msg->AddressFrom |= ((unsigned long)  buffer[ 6]<<14) & 0x001FC000;
  msg->AddressFrom |= ((unsigned long)  buffer[ 7]<< 7) & 0x00003F80;
  msg->AddressFrom |= ((unsigned long)  buffer[ 8]    ) & 0x0000007F;
  msg->MessageID    = ((unsigned long)  buffer[ 9]<<14) & 0x001FC000;
  msg->MessageID   |= ((unsigned long)  buffer[10]<< 7) & 0x00003F80;
  msg->MessageID   |= ((unsigned long)  buffer[11]    ) & 0x0000007F;
  msg->MessageType  = ((unsigned short) buffer[12]<< 7) & 0x3F80;
  msg->MessageType |= ((unsigned short) buffer[13]    ) & 0x007F;
  msg->DataLength   = ((unsigned int)   buffer[14]    ) & 0x007F;

  MBN_TRACE(printf("Received MambaNet message of %d B from 0x%08X to 0x%08X, ctrl 0x%02X, id 0x%06X, type 0x%04X, %d B data",
     length, msg->AddressFrom, msg->AddressTo, msg->ControlByte, msg->MessageID, msg->MessageType, msg->DataLength));
}




