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

#include "mbn.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>


struct mbn_node_info this_node = {
  0x00031337, 0x00, /* MambaNet Addr + Services */
  1, 50, 1,   /* UniqueMediaAccessId */
  "MambaNet Stack Test Application",
  ">> YorHel's Power Node! <<",
  0, 0,       /* Hardware revision */
  0, 0,       /* Firmware revision */
  2,          /* NumberOfObjects */
  0,          /* DefaultEngineAddr */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } /* Hardwareparent */
};

struct mbn_object objects[] = {
  /* Only works on a C99 compiler */
  /* Description  Engine  Freq, Sensor: type       size  min        max          cur          Actuator: type      size  min        max          def          cur       */
  { "Object #1",    0x00,    1, MBN_DATATYPE_UINT,    2, {.UInt=0}, {.UInt=512}, {.UInt=256}, MBN_DATATYPE_NODATA,   0, {.UInt=0}, {.UInt=  0}, {.UInt=  0}, {.UInt=  0} },
  { "Object #2",    0x00,    0, MBN_DATATYPE_NODATA,  0, {.UInt=0}, {.UInt=  0}, {.UInt=  0}, MBN_DATATYPE_UINT,     2, {.UInt=0}, {.UInt=512}, {.UInt=256}, {.UInt=256} },
};


void AddressTableChange(struct mbn_handler *mbn, struct mbn_address_node *old, struct mbn_address_node *new) {
  struct mbn_address_node *cur;
  cur = new == NULL ? old : new;
  printf("%s: %08lX  ->  %04X:%04X:%04X (%02X)\n",
    old == NULL ? "New node" : new == NULL ? "Removed node" : "Node changed",
    cur->MambaNetAddr, cur->ManufacturerID, cur->ProductID, cur->UniqueIDPerProduct, cur->Services);
}


void OnlineStatus(struct mbn_handler *mbn, unsigned long addr, char valid) {
  printf("OnlineStatus: %08lX %s\n", addr, valid ? "validated" : "invalid");
  if(valid)
    mbnSendPingRequest(mbn, MBN_BROADCAST_ADDRESS);
}


int NameChange(struct mbn_handler *mbn, unsigned char *name) {
  printf("NameChange(\"%s\")\n", name);
  return 0;
}


int DefaultEngineAddrChange(struct mbn_handler *mbn, unsigned long engine) {
  printf("DefaultEngineAddrChange(0x%08lX)\n", engine);
  return 0;
}


int SetActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data dat) {
  printf("SetActuatorData(%d, {.", object);
  switch(objects[object].ActuatorType) {
    case MBN_DATATYPE_UINT:  printf("UInt = %d", dat.UInt); break;
    case MBN_DATATYPE_SINT:  printf("SInt = %d", dat.SInt); break;
    case MBN_DATATYPE_STATE: printf("State = %04X", dat.State); break;
    case MBN_DATATYPE_FLOAT: printf("Float = %f", dat.Float); break;
    default: printf("Something_else");
  }
  printf("})\n");
  mbnSensorDataChange(mbn, 0, dat);
  return 0;
}


int GetSensorData(struct mbn_handler *mbn, unsigned short object, union mbn_data *dat) {
  printf("GetSensorData(%d, &dat)\n", object);
  dat->UInt = 9999;
  return 0;
}


void ObjectFrequencyChange(struct mbn_handler *mbn, unsigned short object, unsigned char freq) {
  printf("ObjectFrequencyChange(%d, 0x%02X)\n", object, freq);
}


int SensorDataResponse(struct mbn_handler *mbn, struct mbn_message *msg, unsigned short object, unsigned char type, union mbn_data dat) {
  if(type != MBN_DATATYPE_OCTETS)
    return 0;
  printf("(Sensor|Actuator)DataResponse({.AddressFrom = 0x%08lX }, %d, %d, \"%s\")\n", msg->AddressFrom, object, type, dat.Octets);
  return 1;
}


int main(void) {
  struct mbn_handler *mbn;
  struct timeval before, after;
  struct mbn_message msg;

  mbn = mbnInit(this_node, objects);
  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);
  mbnSetOnlineStatusCallback(mbn, OnlineStatus);
  mbnSetNameChangeCallback(mbn, NameChange);
  mbnSetDefaultEngineAddrChangeCallback(mbn, DefaultEngineAddrChange);
  mbnSetSetActuatorDataCallback(mbn, SetActuatorData);
  mbnSetGetSensorDataCallback(mbn, GetSensorData);
  mbnSetObjectFrequencyChangeCallback(mbn, ObjectFrequencyChange);
  mbnSetSensorDataResponseCallback(mbn, SensorDataResponse);
  mbnSetActuatorDataResponseCallback(mbn, SensorDataResponse);
  mbnEthernetInit(mbn, "eth0");

  /*
  sleep(3);
  union mbn_data dat = {.Octets="4321"};
  mbnSetActuatorData(mbn, 0x00000008, 1, MBN_DATATYPE_OCTETS, 5, dat, 0);
  sleep(1);
  mbnGetActuatorData(mbn, 0x00000008, 1, 0);
  */

  pthread_exit(NULL);
  return 0;
}


