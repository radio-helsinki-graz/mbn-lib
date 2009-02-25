
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

int global;

int call(int *nr) {
  printf("[main] Received %d from stack, returning globally defined number\n", *nr);
  return global;
}

int main(void) {
  global = 1;
  mbnInit(this_node, call);
  global = 10;
  pthread_exit(NULL);
}


