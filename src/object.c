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
void send_object_reply(struct mbn_handler *mbn, struct mbn_message *msg, char action,
                       char type, int length, union mbn_message_object_data *dat) {
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
  memcpy((void *)&(reply.Data.Object.Data), (void *)dat, sizeof(union mbn_message_object_data));
  mbnSendMessage(mbn, &reply, 0);
}


int get_sensor(struct mbn_handler *mbn, struct mbn_message *msg) {
  struct mbn_message_object *obj = &(msg->Data.Object);
  union mbn_message_object_data dat;
  char a = MBN_OBJ_ACTION_SENSOR_RESPONSE;

  switch(obj->Number) {
    case 0: /* Description */
      dat.Octets = mbn->node.Description;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 64, &dat);
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
    case 12: /* Default engine address */
      dat.UInt = mbn->node.DefaultEngineAddr;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_UINT, 4, &dat);
      break;
    case 13: /* Hardware Parent */
      dat.Octets = mbn->node.HardwareParent;
      send_object_reply(mbn, msg, a, MBN_DATATYPE_OCTETS, 6, &dat);
      break;
  }
  return 1;
}


int process_object_message(struct mbn_handler *mbn, struct mbn_message *msg) {
  if(msg->MessageType != MBN_MSGTYPE_OBJECT)
    return 0;

  MBN_TRACE(printf("Got OBJ action: %d, Object: %d", msg->Data.Object.Action, msg->Data.Object.Number));
  switch(msg->Data.Object.Action) {
    case MBN_OBJ_ACTION_GET_SENSOR:
      return get_sensor(mbn, msg);
      break;
  }
  return 0;
}


