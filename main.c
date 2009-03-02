
#include "mbn.h"
#include <stdio.h>


struct mbn_node_info this_node = {
  0x00050000, // MambaNet Addr
  1, 50, 0,   // UniqueMediaAccessId
  "MambaNet Stack Test Application",
  "MambaNet Test",
  0, 0,       // Hardware revision
  0, 0,       // Firmware revision
  0,          // NumberOfObjects
  0,          // DefaultEngineAddr
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } // Hardwareparent
};


int ReceiveMessage(struct mbn_handler *mbn, struct mbn_message *msg) {
  int i;

  if(msg->MessageType == MBN_MSGTYPE_OBJECT) {
    printf("Object Message: number %02X, action %2d, type %3d, size %2dB\n",
      msg->Data.Object.Number, msg->Data.Object.Action, msg->Data.Object.DataType, msg->Data.Object.DataSize);

    for(i=0; i<msg->bufferlength; i++)
      printf(" %02X", msg->buffer[i]);
    printf("\n");
    if(msg->Data.Object.DataType == MBN_DATATYPE_SINT)
      printf(" -> SInt: %ld\n", msg->Data.Object.Data.SInt);
  }
  return 0;
}


int main(void) {
  struct mbn_handler *mbn;

  mbn = mbnInit(this_node);
  mbnSetReceiveMessageCallback(mbn, ReceiveMessage);
  mbnEthernetInit(mbn, "eth0");

  pthread_exit(NULL);
  return 0;
}


