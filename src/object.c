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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbn.h"
#include "object.h"


/* convenience function to reply to an object message */
void send_object_reply(struct mbn_handler *mbn, struct mbn_message *msg, unsigned char action,
                       unsigned char type, int length, union mbn_data *dat) {
  struct mbn_message reply;
  memset((void *)&reply, 0, sizeof(struct mbn_message));
  reply.AddressTo = msg->AddressFrom;
  reply.MessageID = msg->MessageID;
  if(reply.MessageID)
    reply.AcknowledgeReply = 1;
  reply.MessageType = MBN_MSGTYPE_OBJECT;
  reply.Data.Object.Action = action;
  reply.Data.Object.Number = msg->Data.Object.Number;
  reply.Data.Object.DataType = type;
  reply.Data.Object.DataSize = length;
  memcpy((void *)&(reply.Data.Object.Data), (void *)dat, sizeof(union mbn_data));
  mbnSendMessage(mbn, &reply, 0);
}


int get_sensor(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_message_object *obj = &(msg->Data.Object);
  union mbn_data dat;
  unsigned char a = MBN_OBJ_ACTION_SENSOR_RESPONSE;
  int i, r;

  switch(obj->Number) {
    case 0: /* Description */
      dat.Octets = mbn->node.Description;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 64, &dat);
      break;
    case 1: /* Name (not a sensor) */
      send_object_reply(mbn, msg, a, MBN_DATATYPE_NODATA, 0, &dat);
      break;
    case 2: /* Manufacturer ID */
      dat.UInt = mbn->node.ManufacturerID;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 2, &dat);
      break;
    case 3: /* Product ID */
      dat.UInt = mbn->node.ProductID;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 2, &dat);
      break;
    case 4: /* UniqueIDPerProduct */
      dat.UInt = mbn->node.UniqueIDPerProduct;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 2, &dat);
      break;
    case 5: /* Hardware Major Revision */
      dat.UInt = mbn->node.HardwareMajorRevision;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 6: /* Hardware Minor Revision */
      dat.UInt = mbn->node.HardwareMinorRevision;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 7: /* Firmware Major Revision */
      dat.UInt = mbn->node.FirmwareMajorRevision;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 8: /* Firmware Minor Revision */
      dat.UInt = mbn->node.FirmwareMinorRevision;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 9: /* Protocol Major Revision */
      dat.UInt = MBN_PROTOCOL_VERSION_MAJOR;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 10: /* Protocol Minor Revision */
      dat.UInt = MBN_PROTOCOL_VERSION_MINOR;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 11: /* Number of objects */
      dat.UInt = mbn->node.NumberOfObjects;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 2, &dat);
      break;
    case 12: /* Default engine address (not a sensor) */
      send_object_reply(mbn, msg, a, MBN_DATATYPE_NODATA, 0, &dat);
      break;
    case 13: /* Hardware Parent */
      dat.Octets = mbn->node.HardwareParent;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 6, &dat);
      break;
    default:
      i = obj->Number-1024;
      /* we don't have this object! */
      if(i < 0 || i > mbn->node.NumberOfObjects) {
        dat.Error = (unsigned char *) "Object not found";
        send_object_reply(mbn, msg, a, MBN_DATATYPE_ERROR, strlen((char *)dat.Error), &dat);
      /* we have, send callback if exists, and reply if we're allowed to */
      } else {
        r = 0;
        if(mbn->cb_GetSensorData == NULL || mbn->objects[i].SensorType == MBN_DATATYPE_NODATA)
          dat = mbn->objects[i].SensorData;
        else {
          if((r = mbn->cb_GetSensorData(mbn, i, &dat)) == 0)
            mbn->objects[i].SensorData = dat;
        }
        if(r == 0)
          send_object_reply(mbn, msg, a, mbn->objects[i].SensorType, mbn->objects[i].SensorSize, &dat);
      }
      break;
  }
  return 1;
}


int get_actuator(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_message_object *obj = &(msg->Data.Object);
  union mbn_data dat;
  unsigned char a = MBN_OBJ_ACTION_ACTUATOR_RESPONSE;

  switch(obj->Number) {
    case 0: /* These are not actuators */
    case 2: case 3: case  4: case  5: case  6:
    case 7: case 9: case 10: case 11: case 13:
      send_object_reply(mbn, msg, a, MBN_DATATYPE_NODATA, 0, &dat);
      break;
    case 1: /* Name */
      dat.Octets = mbn->node.Name;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 32, &dat);
      break;
    case 12: /* Default Engine Address */
      dat.UInt = mbn->node.DefaultEngineAddr;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 4, &dat);
      break;
    default:
      if(obj->Number >= 1024 && obj->Number < 1024+mbn->node.NumberOfObjects)
        send_object_reply(mbn, msg, a, mbn->objects[obj->Number-1024].ActuatorType,
          mbn->objects[obj->Number-1024].ActuatorSize, &(mbn->objects[obj->Number-1024].ActuatorData));
      else {
        dat.Error = (unsigned char *) "Object not found";
        send_object_reply(mbn, msg, a, MBN_DATATYPE_ERROR, strlen((char *)dat.Error), &dat);
      }
      break;
  }
  return 1;
}


int set_actuator(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_message_object *obj = &(msg->Data.Object);
  union mbn_data dat;
  int r, i = obj->Number-1024;

  /* Name */
  if(obj->Number == 1 && obj->DataType == MBN_DATATYPE_OCTETS && obj->DataSize <= 32) {
    r = mbn->cb_NameChange == NULL ? 0 : mbn->cb_NameChange(mbn, obj->Data.Octets);
    if(r == 0) {
      memset((void *)mbn->node.Name, 0, 32);
      memcpy((void *)mbn->node.Name, (void *)obj->Data.Octets, obj->DataSize);
      if(msg->MessageID > 0 && r == 0) {
        dat.Octets = obj->Data.Octets;
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_ACTUATOR_RESPONSE, MBN_DATATYPE_OCTETS, obj->DataSize, &dat);
      }
    }

  /* Default Engine Address */
  } else if(obj->Number == 12 && obj->DataType == MBN_DATATYPE_UINT && obj->DataSize == 4) {
    r = mbn->cb_DefaultEngineAddrChange == NULL ? 0 : mbn->cb_DefaultEngineAddrChange(mbn, obj->Data.UInt);
    if(r == 0) {
      dat.UInt = mbn->node.DefaultEngineAddr = obj->Data.UInt;
      if(msg->MessageID > 0 && r == 0)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_ACTUATOR_RESPONSE, MBN_DATATYPE_UINT, 4, &dat);
    }

  /* custom object */
  } else if(i >= 0 && i < mbn->node.NumberOfObjects && mbn->cb_SetActuatorData != NULL &&
      mbn->objects[i].ActuatorType != MBN_DATATYPE_NODATA && mbn->objects[i].ActuatorType == obj->DataType) {
    if(mbn->cb_SetActuatorData(mbn, i, obj->Data) == 0) {
      dat = mbn->objects[i].ActuatorData = obj->Data;
      if(msg->MessageID > 0)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_ACTUATOR_RESPONSE, obj->DataType, mbn->objects[i].ActuatorSize, &dat);
    }

  /* something else */
  } else {
    dat.Error = (unsigned char *) "Not implemented";
    send_object_reply(mbn, msg, MBN_OBJ_ACTION_ACTUATOR_RESPONSE, MBN_DATATYPE_ERROR, strlen((char *)dat.Error), &dat);
  }

  return 1;
}


int process_object_message(struct mbn_handler *mbn, struct mbn_message *msg) {
  if(msg->MessageType != MBN_MSGTYPE_OBJECT)
    return 0;

  switch(msg->Data.Object.Action) {
    case MBN_OBJ_ACTION_GET_SENSOR:
      return get_sensor(mbn, msg);
    case MBN_OBJ_ACTION_GET_ACTUATOR:
      return get_actuator(mbn, msg);
    case MBN_OBJ_ACTION_SET_ACTUATOR:
      return set_actuator(mbn, msg);
    /* TODO: handle other actions */
  }
  return 0;
}


