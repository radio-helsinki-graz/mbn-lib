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

#define MBN_VARARG
#include "mbn.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdlib.h>


struct mbn_node_info this_node = {
  0x00031337, 0x00, /* MambaNet Addr + Services */
  "MambaNet Stack Test Application",
  ">> YorHel's Power Node! <<",
  0xFFFF, 0x0001, 0x0001,   /* UniqueMediaAccessId */
  0, 0,          /* Hardware revision */
  0, 0,          /* Firmware revision */
  0, 0,          /* FPGAFirmware revision */
  2,             /* NumberOfObjects */
  0,             /* DefaultEngineAddr */
  {0,0,0,0,0,0}, /* Hardwareparent */
  0              /* Service request */
};

struct mbn_object objects[2];


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
  mbnUpdateSensorData(mbn, 0, dat);
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


void Error(struct mbn_handler *mbn, int code, const char *msg) {
  printf("Error(%d, \"%s\")\n", code, msg);
}


void AcknowledgeTimeout(struct mbn_handler *mbn, struct mbn_message *msg) {
  printf("AcknowledgeTimeout({.AddressTo = %08lX, .MessageID = %06X})\n",
    msg->AddressTo, msg->MessageID);
}


void AcknowledgeReply(struct mbn_handler *mbn, struct mbn_message *msg, struct mbn_message *reply, int retries) {
  printf("AcknowledgeReply({.AddressTo = %08lX, .MessageID = %06X}, {.MessageID = %06X}, %d)\n",
    msg->AddressTo, msg->MessageID, reply->MessageID, retries);
}


int main(void) {
  struct mbn_handler *mbn;
  struct mbn_address_node *node;
  struct mbn_interface *itf;

  objects[0] = MBN_OBJ("Object #1", 1, MBN_DATATYPE_UINT, 2, 0, 512, 256, MBN_DATATYPE_NODATA);
  objects[1] = MBN_OBJ("Object #2", 0, MBN_DATATYPE_NODATA, MBN_DATATYPE_UINT, 2, 0, 512, 0, 256);

  itf = calloc(1, sizeof(struct mbn_interface));
  mbn = mbnInit(this_node, objects, mbnTCPOpen("localhost", NULL, NULL, NULL));
  if(mbn == NULL) {
    printf("mbn = NULL\n");
    return 1;
  }
  mbnSetAddressTableChangeCallback(mbn, AddressTableChange);
  mbnSetOnlineStatusCallback(mbn, OnlineStatus);
  mbnSetNameChangeCallback(mbn, NameChange);
  mbnSetDefaultEngineAddrChangeCallback(mbn, DefaultEngineAddrChange);
  mbnSetSetActuatorDataCallback(mbn, SetActuatorData);
  mbnSetGetSensorDataCallback(mbn, GetSensorData);
  mbnSetObjectFrequencyChangeCallback(mbn, ObjectFrequencyChange);
  mbnSetSensorDataResponseCallback(mbn, SensorDataResponse);
  mbnSetActuatorDataResponseCallback(mbn, SensorDataResponse);
  mbnSetErrorCallback(mbn, Error);
  mbnSetAcknowledgeTimeoutCallback(mbn, AcknowledgeTimeout);
  mbnSetAcknowledgeReplyCallback(mbn, AcknowledgeReply);

  /*sleep(60);*/
  pthread_exit(NULL);
  mbnFree(mbn);
  return 0;
}


