
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


int main(void) {
  struct mbn_interface *eth0;
  struct mbn_handler *mbn;

  mbn = mbnInit(this_node, *eth0);
  mbnEthernetInit(mbn, "eth0");

  pthread_exit(NULL);
  return 0;
}


