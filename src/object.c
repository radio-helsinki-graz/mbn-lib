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


#ifdef MBNP_mingw
# include <windows.h>
# include <winsock2.h>
#else
# include <unistd.h>
# include <sys/select.h>
# include <sys/time.h>
#endif
#include <pthread.h>


void send_object_changed(struct mbn_handler *mbn, unsigned short obj) {
  struct mbn_message msg;
  unsigned long dest;

  /* determine destination address */
  dest = mbn->node.DefaultEngineAddr;
  if(dest == 0)
    dest = MBN_BROADCAST_ADDRESS;

  /* create and send message */
  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo = dest;
  msg.MessageType = MBN_MSGTYPE_OBJECT;
  msg.Data.Object.Action = MBN_OBJ_ACTION_SENSOR_CHANGED;
  msg.Data.Object.Number = obj+1024;
  msg.Data.Object.DataType = mbn->objects[obj].SensorType;
  msg.Data.Object.DataSize = mbn->objects[obj].SensorSize;
  msg.Data.Object.Data = mbn->objects[obj].SensorData;
  mbnSendMessage(mbn, &msg, 0);
}


/* special thread to throttle sensor change messages */
/* Timing info:
 *   S  Freq     Sec     timeout (*0.05s)
 *   2  25  Hz   0.04s    1   -> Actual max. frequency = 20Hz
 *   3  10  Hz   0.10s    2
 *   4   5  Hz   0.20s    4
 *   5   1  Hz   1.00s   20
 *   6   0.2Hz   5.00s  100
 *   7   0.1Hz  10.00s  200
 * Note: this method of timing is not precise, actual send frequencies
 *  are likely lower, depening on CPU speed and kernel interrupt resolution
 */
void *throttle_thread(void *arg) {
  struct mbn_handler *mbn = (struct mbn_handler *) arg;
  struct timeval tv;
  int i, f;

  if(!mbn->objects)
    return NULL;

  while(1) {
    pthread_testcancel();
    /* wait 50ms */
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    select(0, NULL, NULL, NULL, &tv);

    /* lock */
    pthread_mutex_lock((pthread_mutex_t *)mbn->mbn_mutex);

    /* check for changed sensors */
    for(i=0; i<mbn->node.NumberOfObjects; i++) {
      if(mbn->objects[i].timeout != 0) {
        mbn->objects[i].timeout--;
        continue;
      }
      if(!mbn->objects[i].changed)
        continue;
      /* we can send the message */
      send_object_changed(mbn, i);
      /* reset the timeout (see timing info above for detailed explanation) */
      f = mbn->objects[i].UpdateFrequency;
      mbn->objects[i].timeout =
        f == 2 ?   1 : f == 3 ?   2 :
        f == 4 ?   4 : f == 5 ?  20 :
        f == 6 ? 100 : f == 7 ? 200 : 0;
      mbn->objects[i].changed = 0;
    }

    /* unlock */
    pthread_mutex_unlock((pthread_mutex_t *)mbn->mbn_mutex);
  }

  return NULL;
}


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
    case 9: /* FPGAFirmware Major Revision */
      dat.UInt = mbn->node.FPGAFirmwareMajorRevision;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 10: /* FPGAFirmware Minor Revision */
      dat.UInt = mbn->node.FPGAFirmwareMinorRevision;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 11: /* Protocol Major Revision */
      dat.UInt = MBN_PROTOCOL_VERSION_MAJOR;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 12: /* Protocol Minor Revision */
      dat.UInt = MBN_PROTOCOL_VERSION_MINOR;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 1, &dat);
      break;
    case 13: /* Number of objects */
      dat.UInt = mbn->node.NumberOfObjects;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 2, &dat);
      break;
    case 14: /* Default engine address (not a sensor) */
      send_object_reply(mbn, msg, a, MBN_DATATYPE_NODATA, 0, &dat);
      break;
    case 15: /* Hardware Parent */
      dat.Octets = mbn->node.HardwareParent;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 6, &dat);
      break;
    case 16: /* Service request */
      dat.State = mbn->node.ServiceRequest;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_STATE, 1, &dat);
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
    case 2: case  3: case  4: case  5: case  6:  case 7: case  8:
    case 9: case 10: case 11: case 12: case 13: case 15: case 16:
      send_object_reply(mbn, msg, a, MBN_DATATYPE_NODATA, 0, &dat);
      break;
    case 1: /* Name */
      dat.Octets = mbn->node.Name;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 32, &dat);
      break;
    case 14: /* Default Engine Address */
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

  /* TODO: check object min/max? */

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
  } else if(obj->Number == 14 && obj->DataType == MBN_DATATYPE_UINT && obj->DataSize == 4) {
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


int get_info(struct mbn_handler *mbn, struct mbn_message *msg) {
  int i = msg->Data.Object.Number-1024;
  struct mbn_message_object_information nfo;
  struct mbn_object *obj;
  union mbn_data dat;

  /* Wrong object number! */
  if(i < 0 || i >= mbn->node.NumberOfObjects) {
    dat.Error = (unsigned char *) "Object not found";
    send_object_reply(mbn, msg, MBN_OBJ_ACTION_INFO_RESPONSE, MBN_DATATYPE_ERROR, strlen((char *)dat.Error)+1, &dat);
    return 1;
  }

  obj = &(mbn->objects[i]);
  dat.Info = &nfo;
  memcpy((void *)nfo.Description, (void *)obj->Description, 32);
  nfo.Services        = obj->SensorType != MBN_DATATYPE_NODATA ? 0x03 : 0x00;
  nfo.SensorType      = obj->SensorType;
  nfo.SensorSize      = obj->SensorSize;
  nfo.SensorMin       = obj->SensorMin;
  nfo.SensorMax       = obj->SensorMax;
  nfo.ActuatorType    = obj->ActuatorType;
  nfo.ActuatorSize    = obj->ActuatorSize;
  nfo.ActuatorMin     = obj->ActuatorMin;
  nfo.ActuatorMax     = obj->ActuatorMax;
  nfo.ActuatorDefault = obj->ActuatorDefault;
  send_object_reply(mbn, msg, MBN_OBJ_ACTION_INFO_RESPONSE, MBN_DATATYPE_OBJINFO, 0, &dat);

  return 1;
}


int process_object_message(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_message_object *obj = &(msg->Data.Object);
  union mbn_data dat;
  int i;

  if(msg->MessageType != MBN_MSGTYPE_OBJECT)
    return 0;

  i = obj->Number-1024;

  /* TODO: handle object messages with MBN_DATATYPE_ERROR in the data field */

  switch(obj->Action) {
    /* Get object info */
    case MBN_OBJ_ACTION_GET_INFO:
      return get_info(mbn, msg);

    /* Object specific engines addresses are reserved for future use, so don't accept them now */
    case MBN_OBJ_ACTION_SET_ENGINE:
      dat.Error = (unsigned char *) "Not implemented";
      send_object_reply(mbn, msg, MBN_OBJ_ACTION_ENGINE_RESPONSE, MBN_DATATYPE_ERROR, strlen((char *)dat.Error)+1, &dat);
      return 1;
    case MBN_OBJ_ACTION_GET_ENGINE:
      dat.UInt = 0;
      send_object_reply(mbn, msg, MBN_OBJ_ACTION_ENGINE_RESPONSE, MBN_DATATYPE_UINT, 4, &dat);
      return 1;

    /* Frequency information */
    case MBN_OBJ_ACTION_GET_FREQUENCY:
      if(i < 0 || i >= mbn->node.NumberOfObjects) {
        dat.Error = (unsigned char *) "Object not found";
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_FREQUENCY_RESPONSE, MBN_DATATYPE_ERROR, strlen((char *)dat.Error)+1, &dat);
      } else {
        dat.State = mbn->objects[i].UpdateFrequency;
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_FREQUENCY_RESPONSE, MBN_DATATYPE_STATE, 1, &dat);
      }
      return 1;
    case MBN_OBJ_ACTION_SET_FREQUENCY: /* this one is pretty much internal, is the callback really useful? */
      if(i < 0 || i >= mbn->node.NumberOfObjects || obj->DataType != MBN_DATATYPE_STATE || obj->DataType > 7) {
        dat.Error = (unsigned char *) "Object not found";
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_FREQUENCY_RESPONSE, MBN_DATATYPE_ERROR, strlen((char *)dat.Error)+1, &dat);
      } else {
        if(mbn->objects[i].UpdateFrequency != obj->Data.State && mbn->cb_ObjectFrequencyChange != NULL)
          mbn->cb_ObjectFrequencyChange(mbn, i, obj->Data.State);
        mbn->objects[i].UpdateFrequency = obj->Data.State;
        if(msg->MessageID)
          send_object_reply(mbn, msg, MBN_OBJ_ACTION_FREQUENCY_RESPONSE, MBN_DATATYPE_STATE, 1, &dat);
      }
      return 1;

    /* Sensor/Actuator get/set actions */
    case MBN_OBJ_ACTION_GET_SENSOR:
      return get_sensor(mbn, msg);
    case MBN_OBJ_ACTION_GET_ACTUATOR:
      return get_actuator(mbn, msg);
    case MBN_OBJ_ACTION_SET_ACTUATOR:
      return set_actuator(mbn, msg);

    /* Various responses, directly forward them to the application, and reply if necessary */
    /* MBN_OBJ_ACTION_ENGINE_RESPONSE not implemented, because we can't receive it */
    case MBN_OBJ_ACTION_INFO_RESPONSE:
      if(mbn->cb_ObjectInformationResponse != NULL
          && mbn->cb_ObjectInformationResponse(mbn, msg, obj->Number, obj->Data.Info) == 0
          && msg->MessageID)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_INFO_RESPONSE, MBN_DATATYPE_OBJINFO, obj->DataSize, &(obj->Data));
      return 1;
    case MBN_OBJ_ACTION_FREQUENCY_RESPONSE:
      if(mbn->cb_ObjectFrequencyResponse != NULL
          && mbn->cb_ObjectFrequencyResponse(mbn, msg, obj->Number, obj->Data.State) == 0
          && msg->MessageID)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_FREQUENCY_RESPONSE, MBN_DATATYPE_STATE, obj->DataSize, &(obj->Data));
      return 1;
    case MBN_OBJ_ACTION_SENSOR_RESPONSE:
      if(mbn->cb_SensorDataResponse != NULL
          && mbn->cb_SensorDataResponse(mbn, msg, obj->Number, obj->DataType, obj->Data) == 0
          && msg->MessageID)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_SENSOR_RESPONSE, MBN_DATATYPE_STATE, obj->DataSize, &(obj->Data));
      return 1;
    case MBN_OBJ_ACTION_SENSOR_CHANGED:
      if(mbn->cb_SensorDataChanged != NULL
          && mbn->cb_SensorDataChanged(mbn, msg, obj->Number, obj->DataType, obj->Data) == 0
          && msg->MessageID)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_SENSOR_RESPONSE, MBN_DATATYPE_STATE, obj->DataSize, &(obj->Data));
      return 1;
    case MBN_OBJ_ACTION_ACTUATOR_RESPONSE:
      if(mbn->cb_ActuatorDataResponse != NULL
          && mbn->cb_ActuatorDataResponse(mbn, msg, obj->Number, obj->DataType, obj->Data) == 0
          && msg->MessageID)
        send_object_reply(mbn, msg, MBN_OBJ_ACTION_ACTUATOR_RESPONSE, MBN_DATATYPE_STATE, obj->DataSize, &(obj->Data));
      return 1;
  }
  return 0;
}


void MBN_EXPORT mbnUpdateSensorData(struct mbn_handler *mbn, unsigned short object, union mbn_data dat) {
  pthread_mutex_lock((pthread_mutex_t *)mbn->mbn_mutex);

  /* update internal sensor data */
  mbn->objects[object].SensorData = dat;

  /* object frequency = 1, create & send message now */
  if(mbn->objects[object].UpdateFrequency == 1)
    send_object_changed(mbn, object);

  /* object frequency > 1, messages are throttled, let the sending be handled by a separate thread */
  else if(mbn->objects[object].UpdateFrequency > 1)
    mbn->objects[object].changed = 1;

  pthread_mutex_unlock((pthread_mutex_t *)mbn->mbn_mutex);
}

void MBN_EXPORT mbnUpdateActuatorData(struct mbn_handler *mbn, unsigned short object, union mbn_data dat) {
  mbn->objects[object].ActuatorData = dat;
}


/* convenience function */
void request_info(struct mbn_handler *mbn, unsigned long addr, unsigned short object, char ack, unsigned char act) {
  struct mbn_message msg;
  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo = addr;
  msg.MessageType = MBN_MSGTYPE_OBJECT;
  msg.Data.Object.Action = act;
  msg.Data.Object.Number = object;
  msg.Data.Object.DataType = MBN_DATATYPE_NODATA;
  mbnSendMessage(mbn, &msg, ack ? MBN_SEND_ACKNOWLEDGE : 0);
}

void MBN_EXPORT mbnGetSensorData(struct mbn_handler *mbn, unsigned long addr, unsigned short object, char ack) {
  request_info(mbn, addr, object, ack, MBN_OBJ_ACTION_GET_SENSOR);
}

void MBN_EXPORT mbnGetActuatorData(struct mbn_handler *mbn, unsigned long addr, unsigned short object, char ack) {
  request_info(mbn, addr, object, ack, MBN_OBJ_ACTION_GET_ACTUATOR);
}

void MBN_EXPORT mbnGetObjectInformation(struct mbn_handler *mbn, unsigned long addr, unsigned short object, char ack) {
  request_info(mbn, addr, object, ack, MBN_OBJ_ACTION_GET_INFO);
}

void MBN_EXPORT mbnGetObjectFrequency(struct mbn_handler *mbn, unsigned long addr, unsigned short object, char ack) {
  request_info(mbn, addr, object, ack, MBN_OBJ_ACTION_GET_FREQUENCY);
}


void MBN_EXPORT mbnSetActuatorData(struct mbn_handler *mbn, unsigned long addr, unsigned short object,
                                   unsigned char type, unsigned char length, union mbn_data dat, char ack) {
  struct mbn_message msg;
  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo = addr;
  msg.MessageType = MBN_MSGTYPE_OBJECT;
  msg.Data.Object.Action = MBN_OBJ_ACTION_SET_ACTUATOR;
  msg.Data.Object.Number = object;
  msg.Data.Object.DataType = type;
  msg.Data.Object.DataSize = length;
  msg.Data.Object.Data = dat;
  mbnSendMessage(mbn, &msg, ack ? MBN_SEND_ACKNOWLEDGE : 0);
}


void MBN_EXPORT mbnSetObjectFrequency(struct mbn_handler *mbn, unsigned long addr, unsigned short object,
                                      unsigned char freq, char ack) {
  struct mbn_message msg;
  memset((void *)&msg, 0, sizeof(struct mbn_message));
  msg.AddressTo = addr;
  msg.MessageType = MBN_MSGTYPE_OBJECT;
  msg.Data.Object.Action = MBN_OBJ_ACTION_SET_FREQUENCY;
  msg.Data.Object.Number = object;
  msg.Data.Object.DataType = MBN_DATATYPE_STATE;
  msg.Data.Object.DataSize = 1;
  msg.Data.Object.Data.State = freq;
  mbnSendMessage(mbn, &msg, ack ? MBN_SEND_ACKNOWLEDGE : 0);
}


