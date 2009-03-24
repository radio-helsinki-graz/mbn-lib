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
#include "codec.h"


/* Converts 7bits data to 8bits. The result buffer must
 * be at least 8/7 times as large as the input buffer.
 * (shamelessly stolen from Anton's Decode7to8bits()) */
int convert_7to8bits(unsigned char *buffer, unsigned char length, unsigned char *result) {
  int i, reslength = 0;
  unsigned char mask1, mask2;

  result[reslength] = buffer[0]&0x7F;
  for(i=1; i<length; i++) {
    mask1 = (0x7F>>(i&0x07))<<(i&0x07);
    mask2 = mask1^0x7F;

    if(mask2 != 0x00)
      result[reslength++] |= (buffer[i] & mask2) << (8 - (i & 0x07));
    result[reslength] = (buffer[i] & mask1) >> (i & 0x07);
  }

  return reslength;
}


/* Opposite of above function */
int convert_8to7bits(unsigned char *buffer, unsigned char length, unsigned char *result) {
  int i, reslength = 0;
  unsigned char mask1, mask2, shift;

  result[reslength] = 0;
  for(i=0; i<length; i++) {
    shift = i%7;
    mask1 = 0x7F>>shift;
    mask2 = mask1^0xFF;

    result[reslength++] |= (buffer[i] & mask1) << shift;
    result[reslength  ]  = (buffer[i] & mask2) >> (7-shift);
    if(mask2 == 0xFE)
      result[++reslength] = 0x00;
  }
  if((i%7) != 0)
    reslength++;

  return reslength;
}


/* Converts variable float into the native float type
 * of the current CPU. Returns non-zero on failure.
 * (shamelessly stolen from Anton's VariableFloat2Float())
 * Note: this function does assume that the CPU
 *  represenation of the float type is a 32 bits
 *  IEEE 754, but we're probably quite safe with that */
int convert_varfloat_to_float(unsigned char *buffer, unsigned char length, float *result) {
  unsigned long tmp;
  int exponent;
  unsigned long mantessa;
  char signbit;

  /* check length */
  if(length == 0 || length == 3 || length > 4)
    return 1;

  /* check for zero (is this even necessary?) */
  tmp = 0x00000000;
  if(memcmp((void *)&tmp, (void *)buffer, length) == 0) {
    *result = *((float *)&tmp);
    return 0;
  }

  /* otherwise, calculate float */
  switch(length) {
    case 1:
      signbit  = (buffer[0]>>7)&0x01;
      exponent = (buffer[0]>>4)&0x07;
      mantessa = (buffer[0]   )&0x0F;
      if(exponent == 0) /* denormalized */
        exponent = -127;
      else if(exponent == 7) /* +/-INF or NaN, depends on sign */
        exponent = 128;
      else
        exponent -= 3;

      tmp = (signbit<<31) | (((exponent+127)&0xFF)<<23) | ((mantessa&0x0F)<<19);
      *result = *((float *)&tmp);
      break;
    case 2:
      signbit  = (buffer[0]>>7)&0x01;
      exponent = (buffer[0]>>2)&0x1F;
      mantessa = (((unsigned long) buffer[0]&0x03)<<8) | (buffer[1]&0xFF);
      if(exponent == 0) /* denormalized */
        exponent = -127;
      else if(exponent == 31) /* +/-INF, NaN, depends on sign */
        exponent = 128;
      else
        exponent -= 15;

      tmp = (signbit<<31) | (((exponent+127)&0xFF)<<23) | (((mantessa>>8)&0x03)<<21) | ((mantessa&0xFF)<<13);
      *result = *((float *)&tmp);
      break;
    case 4:
      tmp = (buffer[0]<<24) | (buffer[1]<<16) | (buffer[2]<<8) | buffer[3];
      *result = *((float *)&tmp);
      break;
  }

  return 0;
}


/* opposite of above function */
int convert_float_to_varfloat(unsigned char *buffer, unsigned char length, float flt) {
  unsigned long tmp;
  int exponent;
  unsigned long mantessa;
  char signbit;

  if(length < 1 || length > 4 || length == 3)
    return 1;

  tmp = *((unsigned long *)&flt);
  mantessa = tmp & 0x007FFFFF;
  exponent = (tmp>>23)&0xFF;
  exponent -= 127;
  signbit  = (tmp>>(23+8))&0x01;

  if(tmp == 0x00000000) {
    memset((void *)buffer, 0, length);
    return 0;
  }

  switch(length) {
    case 1:
      exponent += 3;
      if(exponent < 0)
        exponent = 0;
      if(exponent > 6) {
        exponent = 7;
        mantessa = 0;
      }
      buffer[0] = (signbit<<7) | ((exponent&0x07)<<4) | ((mantessa>>19)&0x0F);
      break;
    case 2:
      exponent += 15;
      if(exponent < 0)
        exponent = 0;
      if(exponent > 30) {
        exponent = 31;
        mantessa = 0;
      }
      buffer[0] = (signbit<<7) | ((exponent&0x1F)<<2) | ((mantessa>>21)&0x03);
      buffer[1] = (mantessa>>13)&0xFF;
      break;
    case 4:
      buffer[0] = (tmp>>24)&0xFF;
      buffer[1] = (tmp>>16)&0xFF;
      buffer[2] = (tmp>> 8)&0xFF;
      buffer[3] =  tmp     &0xFF;
      break;
  }
  return 0;
}


/* Parses the data part of Address Reservation Messages,
 * returns non-zero on failure */
int parsemsg_address(struct mbn_message *msg) {
  struct mbn_message_address *addr = &(msg->Message.Address);

  if(msg->bufferlength != 16)
    return 1;
  addr->Action = msg->buffer[0];
  addr->ManufacturerID     = ((unsigned short) msg->buffer[ 1]<< 8) | (unsigned short) msg->buffer[ 2];
  addr->ProductID          = ((unsigned short) msg->buffer[ 3]<< 8) | (unsigned short) msg->buffer[ 4];
  addr->UniqueIDPerProduct = ((unsigned short) msg->buffer[ 5]<< 8) | (unsigned short) msg->buffer[ 6];
  addr->MambaNetAddr       = ((unsigned long)  msg->buffer[ 7]<<24) |((unsigned long)  msg->buffer[ 8]<<16)
                           | ((unsigned long)  msg->buffer[ 9]<< 8) | (unsigned long)  msg->buffer[10];
  addr->EngineAddr         = ((unsigned long)  msg->buffer[11]<<24) |((unsigned long)  msg->buffer[12]<<16)
                           | ((unsigned long)  msg->buffer[13]<< 8) | (unsigned long)  msg->buffer[14];
  addr->Services = msg->buffer[15];
  return 0;
}


/* Converts a data type into a union, allocating memory for the
 * types that need it. Returns non-zero on failure. */
int parse_datatype(unsigned char type, unsigned char *buffer, int length, union mbn_data *result) {
  struct mbn_object *nfo;
  int i;

  switch(type) {
    case MBN_DATATYPE_NODATA:
      if(length > 0)
        return 1;
      break;

    case MBN_DATATYPE_UINT:
    case MBN_DATATYPE_STATE:
      if(length < 1 || length > 4)
        return 1;
      result->UInt = 0;
      for(i=0; i<length; i++) {
        result->UInt <<= 8;
        result->UInt |= buffer[i];
      }
      if(type == MBN_DATATYPE_STATE)
        result->State = result->UInt;
      break;

    case MBN_DATATYPE_SINT:
      if(length < 1 || length > 4)
        return 1;
      /* parse it as a normal unsigned int */
      for(i=0; i<length; i++) {
        result->UInt <<= 8;
        result->UInt |= buffer[i];
      }
      /* check for sign bit, and set the MSB bits to 1 */
      /* This trick assumes that signed long types are always
       * two's complement and 32 bits.
       * TODO: test this on a 64bit cpu */
      if(buffer[0] & 0x80) {
        result->UInt |= length == 1 ? 0xFFFFFF80
                      : length == 2 ? 0xFFFF8000
                      : length == 3 ? 0xFF800000
                      :               0x80000000;
      }
      /* this shouldn't be necessary, but does guarantee portability */
      break;

    case MBN_DATATYPE_OCTETS:
    case MBN_DATATYPE_ERROR:
      if((type == MBN_DATATYPE_OCTETS && length < 1) || length > 64)
        return 1;
      /* Note: we add an extra \0 to the octets so using string functions won't
       * trash the application. The MambaNet protocol doesn't require this. */
      result->Octets = malloc(length+1);
      memcpy(result->Octets, buffer, length);
      result->Octets[length] = 0;
      if(type == MBN_DATATYPE_ERROR)
        result->Error = result->Octets;
      break;

    case MBN_DATATYPE_FLOAT:
      if(convert_varfloat_to_float(buffer, length, &(result->Float)) != 0)
        return 1;
      break;

    case MBN_DATATYPE_BITS:
      if(length < 1 || length > 8)
        return 1;
      memcpy(result->Bits, buffer, length);
      break;

    case MBN_DATATYPE_OBJINFO:
      if(length < 37 || length > 77)
        return 1;
      nfo = (struct mbn_object *) calloc(1, sizeof(struct mbn_object));
      i = 32;
      memcpy(nfo->Description, buffer, i);
      nfo->Services = buffer[i++];
      nfo->SensorType = buffer[i++];
      nfo->SensorSize = buffer[i++];
      if((nfo->SensorSize*2)+i > length) {
        free(nfo);
        return 4;
      }
      if(parse_datatype(nfo->SensorType, &(buffer[i]), nfo->SensorSize, &(nfo->SensorMin)) != 0) {
        free(nfo);
        return 3;
      }
      i += nfo->SensorSize;
      if(parse_datatype(nfo->SensorType, &(buffer[i]), nfo->SensorSize, &(nfo->SensorMax)) != 0) {
        free(nfo);
        return 3;
      }
      i += nfo->SensorSize;
      nfo->ActuatorType = buffer[i++];
      nfo->ActuatorSize = buffer[i++];
      if((nfo->ActuatorSize*3)+i > length) {
        free(nfo);
        return 4;
      }
      if(parse_datatype(nfo->ActuatorType, &(buffer[i]), nfo->ActuatorSize, &(nfo->ActuatorMin)) != 0) {
        free(nfo);
        return 3;
      }
      i += nfo->ActuatorSize;
      if(parse_datatype(nfo->ActuatorType, &(buffer[i]), nfo->ActuatorSize, &(nfo->ActuatorMax)) != 0) {
        free(nfo);
        return 3;
      }
      i += nfo->SensorSize;
      if(parse_datatype(nfo->ActuatorType, &(buffer[i]), nfo->ActuatorSize, &(nfo->ActuatorDefault)) != 0) {
        free(nfo);
        return 3;
      }
      break;

    default:
      return 2;
  }
  return 0;
}


/* Parses the data part of Object Messages,
 * returns non-zero on failure */
int parsemsg_object(struct mbn_message *msg) {
  int r;
  struct mbn_message_object *obj = &(msg->Message.Object);

  if(msg->bufferlength < 4)
    return 1;
  /* header */
  obj->Number   = ((unsigned short) msg->buffer[0]<<8) | (unsigned short) msg->buffer[1];
  obj->Action   = msg->buffer[2];
  obj->DataType = msg->buffer[3];
  obj->DataSize = 0;

  /* No data? stop processing */
  if(obj->DataType == MBN_DATATYPE_NODATA) {
    if(msg->bufferlength > 4)
      return 2;
    /* we need data for these actions... */
    if( obj->Action == MBN_OBJ_ACTION_ENGINE_RESPONSE    || obj->Action == MBN_OBJ_ACTION_SET_ENGINE     ||
        obj->Action == MBN_OBJ_ACTION_FREQUENCY_RESPONSE || obj->Action == MBN_OBJ_ACTION_SET_FREQUENCY  ||
        obj->Action == MBN_OBJ_ACTION_SENSOR_RESPONSE    || obj->Action == MBN_OBJ_ACTION_SENSOR_CHANGED ||
        obj->Action == MBN_OBJ_ACTION_ACTUATOR_RESPONSE  || obj->Action == MBN_OBJ_ACTION_SET_ACTUATOR)
      return 4;
    return 0;
  }

  /* we can always receive an error */
  if(obj->DataType != MBN_DATATYPE_ERROR) {

    /* we don't need data for these actions... */
    if( obj->Action == MBN_OBJ_ACTION_GET_INFO      || obj->Action == MBN_OBJ_ACTION_GET_ENGINE ||
        obj->Action == MBN_OBJ_ACTION_GET_FREQUENCY || obj->Action == MBN_OBJ_ACTION_GET_SENSOR ||
        obj->Action == MBN_OBJ_ACTION_GET_ACTUATOR)
      return 4;

    /* and for some actions we must get data of only one type */
    if( (obj->Action == MBN_OBJ_ACTION_INFO_RESPONSE      && obj->DataType != MBN_DATATYPE_OBJINFO) ||
        (obj->Action == MBN_OBJ_ACTION_ENGINE_RESPONSE    && obj->DataType != MBN_DATATYPE_UINT)    ||
        (obj->Action == MBN_OBJ_ACTION_SET_ENGINE         && obj->DataType != MBN_DATATYPE_UINT)    ||
        (obj->Action == MBN_OBJ_ACTION_FREQUENCY_RESPONSE && obj->DataType != MBN_DATATYPE_STATE)   ||
        (obj->Action == MBN_OBJ_ACTION_SET_FREQUENCY      && obj->DataType != MBN_DATATYPE_STATE))
      return 4;
  }

  /* Data, so parse it */
  obj->DataSize = msg->buffer[4];
  if(obj->DataSize != msg->bufferlength-5)
    return 3;

  if((r = parse_datatype(obj->DataType, &(msg->buffer[5]), obj->DataSize, &(obj->Data))) != 0)
    return r | 8;

  return 0;
}


/* Parses a raw MambaNet message and puts the results back in the struct,
 *  allocating memory where necessary.
 * returns non-zero on failure */
int parse_message(struct mbn_message *msg) {
  int l, err, datlen;

  /* Message is too small for a header to fit */
  if(msg->rawlength < 15)
    return 0x01;

  /* decode MambaNet header */
  msg->AcknowledgeReply = msg->raw[0] == 0x82 ? 1 : 0;
  msg->AddressTo    = ((unsigned long)  msg->raw[ 0]<<28) & 0x10000000;
  msg->AddressTo   |= ((unsigned long)  msg->raw[ 1]<<21) & 0x0FE00000;
  msg->AddressTo   |= ((unsigned long)  msg->raw[ 2]<<14) & 0x001FC000;
  msg->AddressTo   |= ((unsigned long)  msg->raw[ 3]<< 7) & 0x00003F80;
  msg->AddressTo   |= ((unsigned long)  msg->raw[ 4]    ) & 0x0000007F;
  msg->AddressFrom  = ((unsigned long)  msg->raw[ 5]<<21) & 0x0FE00000;
  msg->AddressFrom |= ((unsigned long)  msg->raw[ 6]<<14) & 0x001FC000;
  msg->AddressFrom |= ((unsigned long)  msg->raw[ 7]<< 7) & 0x00003F80;
  msg->AddressFrom |= ((unsigned long)  msg->raw[ 8]    ) & 0x0000007F;
  msg->MessageID    = ((unsigned long)  msg->raw[ 9]<<14) & 0x001FC000;
  msg->MessageID   |= ((unsigned long)  msg->raw[10]<< 7) & 0x00003F80;
  msg->MessageID   |= ((unsigned long)  msg->raw[11]    ) & 0x0000007F;
  msg->MessageType  = ((unsigned short) msg->raw[12]<< 7) &     0x3F80;
  msg->MessageType |= ((unsigned short) msg->raw[13]    ) &     0x007F;
  datlen            = ((unsigned char)  msg->raw[14]    ) &       0x7F;

  /* done parsing if there's no data */
  if(datlen == 0)
    return 0;

  /* check for the validness of the DataLength */
  for(l=0; msg->raw[l+15] != 0xFF && l+15 < msg->rawlength; l++)
    ;
  if(datlen != l)
    return 0x03;

  /* fill the 8bit buffer */
  msg->bufferlength = convert_7to8bits(&(msg->raw[15]), datlen, msg->buffer);

  /* parse the data part */
  if(msg->MessageType == MBN_MSGTYPE_ADDRESS) {
    if((err = parsemsg_address(msg)) != 0)
      return err | 0x10;
  } else if(msg->MessageType == MBN_MSGTYPE_OBJECT) {
    if((err = parsemsg_object(msg)) != 0)
      return err | 0x20;
  }

  return 0;
}


/* recursively free()'s data type unions/structs allocated by parse_datatype() */
void free_datatype(unsigned char type, union mbn_data *data) {
  if(type == MBN_DATATYPE_ERROR)
    free(data->Error);
  if(type == MBN_DATATYPE_OCTETS)
    free(data->Octets);
  if(type == MBN_DATATYPE_OBJINFO) {
    if(data->Info->SensorSize > 0) {
      free_datatype(data->Info->SensorType, &(data->Info->SensorMin));
      free_datatype(data->Info->SensorType, &(data->Info->SensorMax));
    }
    if(data->Info->ActuatorSize > 0) {
      free_datatype(data->Info->ActuatorType, &(data->Info->ActuatorMin));
      free_datatype(data->Info->ActuatorType, &(data->Info->ActuatorMax));
      free_datatype(data->Info->ActuatorType, &(data->Info->ActuatorDefault));
    }
    free(data->Info);
  }
}


/* free()'s an mbn_message struct allocated by parse_message().
 * The struct itself and the 'raw' data aren't freed, as these aren't
 * allocated by parse_message(), either.*/
void free_message(struct mbn_message *msg) {
  /* no buffer, no data, nothing to free */
  if(msg->bufferlength == 0)
    return;

  if(msg->MessageType == MBN_MSGTYPE_ADDRESS)
    return;
  else if(msg->MessageType == MBN_MSGTYPE_OBJECT) {
    if(msg->Message.Object.DataSize > 0)
      free_datatype(msg->Message.Object.DataType, &(msg->Message.Object.Data));
  }
}


/* address struct -> 8bit data */
int createmsg_address(struct mbn_message *msg) {
  struct mbn_message_address *addr = &(msg->Message.Address);

  msg->bufferlength = 16;
  msg->buffer[ 0] =  addr->Action;
  msg->buffer[ 1] = (addr->ManufacturerID    >> 8) & 0xFF;
  msg->buffer[ 2] =  addr->ManufacturerID          & 0xFF;
  msg->buffer[ 3] = (addr->ProductID         >> 8) & 0xFF;
  msg->buffer[ 4] =  addr->ProductID               & 0xFF;
  msg->buffer[ 5] = (addr->UniqueIDPerProduct>> 8) & 0xFF;
  msg->buffer[ 6] =  addr->UniqueIDPerProduct      & 0xFF;
  msg->buffer[ 7] = (addr->MambaNetAddr      >>24) & 0xFF;
  msg->buffer[ 8] = (addr->MambaNetAddr      >>16) & 0xFF;
  msg->buffer[ 9] = (addr->MambaNetAddr      >> 8) & 0xFF;
  msg->buffer[10] =  addr->MambaNetAddr            & 0xFF;
  msg->buffer[11] = (addr->EngineAddr        >>24) & 0xFF;
  msg->buffer[12] = (addr->EngineAddr        >>16) & 0xFF;
  msg->buffer[13] = (addr->EngineAddr        >> 8) & 0xFF;
  msg->buffer[14] =  addr->EngineAddr              & 0xFF;
  msg->buffer[15] =  addr->Services;
  return 0;
}


int create_datatype(unsigned char type, union mbn_data *dat, int length, unsigned char *buffer) {
  int i;

  switch(type) {
    case MBN_DATATYPE_NODATA:
      if(length != 0)
        return 1;
      break;

    case MBN_DATATYPE_UINT:
    case MBN_DATATYPE_STATE:
      if(length < 1 || length > 4)
        return 1;
      for(i=0; i<length; i++)
        buffer[i] = (dat->UInt>>(8*(length-1-i))) & 0xFF;
      break;

    case MBN_DATATYPE_SINT:
      if(length < 1 || length > 4)
        return 1;
      /* again, this assumes unsigned int is 4 bytes in two's complement */
      if(dat->SInt >= 0 || length == 4)
        for(i=0; i<length; i++)
          buffer[i] = (dat->SInt>>(8*(length-1-i))) & 0xFF;
      if(length == 2) {
        buffer[0] = (0x80 | (dat->SInt>>8)) & 0xFF;
        buffer[1] = dat->SInt & 0xFF;
      } else
        buffer[0] = (0x80 | dat->SInt) & 0xFF;
      break;

    case MBN_DATATYPE_OCTETS:
    case MBN_DATATYPE_ERROR:
      if((type == MBN_DATATYPE_OCTETS && length < 1) || length > 64)
        return 1;
      memcpy((void *)buffer, (void *)(type == MBN_DATATYPE_ERROR ? dat->Error : dat->Octets), length);
      break;

    case MBN_DATATYPE_BITS:
      if(length < 1 || length > 8)
        return 1;
      memcpy((void *)buffer, (void *)dat->Bits, length);
      break;

    case MBN_DATATYPE_FLOAT:
      if(convert_float_to_varfloat(buffer, length, dat->Float) != 0)
        return 1;
      break;

    case MBN_DATATYPE_OBJINFO:
      i = 32;
      memcpy((void *)buffer, (void *)dat->Info->Description, i);
      buffer[i++] = dat->Info->Services;
      buffer[i++] = dat->Info->SensorType;
      buffer[i++] = dat->Info->SensorSize;
      if((dat->Info->SensorSize*2)+i > length)
        return 4;
      if(create_datatype(dat->Info->SensorType, &(dat->Info->SensorMin), dat->Info->SensorSize, &(buffer[i])) != 0)
        return 3;
      i += dat->Info->SensorSize;
      if(create_datatype(dat->Info->SensorType, &(dat->Info->SensorMax), dat->Info->SensorSize, &(buffer[i])) != 0)
        return 3;
      i += dat->Info->SensorSize;
      buffer[i++] = dat->Info->ActuatorType;
      buffer[i++] = dat->Info->ActuatorSize;
      if((dat->Info->ActuatorSize*3)+i > length)
        return 4;
      if(create_datatype(dat->Info->ActuatorType, &(dat->Info->ActuatorMin), dat->Info->ActuatorSize, &(buffer[i])) != 0)
        return 3;
      i += dat->Info->ActuatorSize;
      if(create_datatype(dat->Info->ActuatorType, &(dat->Info->ActuatorMax), dat->Info->ActuatorSize, &(buffer[i])) != 0)
        return 3;
      i += dat->Info->ActuatorSize;
      if(create_datatype(dat->Info->ActuatorType, &(dat->Info->ActuatorDefault), dat->Info->ActuatorSize, &(buffer[i])) != 0)
        return 3;
      break;

    default:
      return 2;
  }
  return 0;
}


/* object message struct -> 8bit data 8 */
int createmsg_object(struct mbn_message *msg) {
  struct mbn_message_object *obj = &(msg->Message.Object);
  int l = 0, r;

  /* set the data size for some fixed types */
  if(obj->DataType == MBN_DATATYPE_NODATA)
    obj->DataSize = 0;
  if(obj->DataType == MBN_DATATYPE_OBJINFO) {
    if(obj->Data.Info->SensorType == MBN_DATATYPE_NODATA)
      obj->Data.Info->SensorSize = 0;
    if(obj->Data.Info->ActuatorType == MBN_DATATYPE_NODATA)
      obj->Data.Info->ActuatorSize = 0;
    obj->DataSize = 37 + 2*obj->Data.Info->SensorSize + 3*obj->Data.Info->ActuatorSize;
  }

  /* header */
  msg->buffer[l++] = (obj->Number>>8) & 0xFF;
  msg->buffer[l++] = obj->Number & 0xFF;
  msg->buffer[l++] = obj->Action;
  msg->buffer[l++] = obj->DataType;
  if(obj->DataType != MBN_DATATYPE_NODATA)
    msg->buffer[l++] = obj->DataSize;

  if(obj->DataSize + l > MBN_MAX_MESSAGE_SIZE)
    return 1;

  /* data */
  if((r = create_datatype(obj->DataType, &(obj->Data), obj->DataSize, &(msg->buffer[l]))) != 0)
    return r<<1;
  l += obj->DataSize;
  msg->bufferlength = l;

  return 0;
}


/* Converts the data in the structs to the "raw" member, which
 * is assumed to be large enough to contain the entire packet. */
/* returns non-zero on error */
int create_message(struct mbn_message *msg, char onlyheader) {
  int datlen, r;

  /* encode the data part */
  if(!onlyheader) {
    if(msg->MessageType == MBN_MSGTYPE_ADDRESS) {
      if((r = createmsg_address(msg)) != 0)
        return r | 0x10;
    } else if(msg->MessageType == MBN_MSGTYPE_OBJECT) {
      if((r = createmsg_object(msg)) != 0)
        return r | 0x20;
    } else
      return 1;
  }

  /* header */
  msg->rawlength = 0;
  msg->raw[msg->rawlength++] = 0x80 | (msg->AcknowledgeReply?0x82:0x00) | ((msg->AddressTo>>28)&0x01);
  msg->raw[msg->rawlength++] = (msg->AddressTo  >>21) & 0x7F;
  msg->raw[msg->rawlength++] = (msg->AddressTo  >>14) & 0x7F;
  msg->raw[msg->rawlength++] = (msg->AddressTo  >> 7) & 0x7F;
  msg->raw[msg->rawlength++] =  msg->AddressTo        & 0x7F;
  msg->raw[msg->rawlength++] = (msg->AddressFrom>>21) & 0x7F;
  msg->raw[msg->rawlength++] = (msg->AddressFrom>>14) & 0x7F;
  msg->raw[msg->rawlength++] = (msg->AddressFrom>> 7) & 0x7F;
  msg->raw[msg->rawlength++] =  msg->AddressFrom      & 0x7F;
  msg->raw[msg->rawlength++] = (msg->MessageID  >>14) & 0x7F;
  msg->raw[msg->rawlength++] = (msg->MessageID  >> 7) & 0x7F;
  msg->raw[msg->rawlength++] =  msg->MessageID        & 0x7F;
  msg->raw[msg->rawlength++] = (msg->MessageType>> 7) & 0x7F;
  msg->raw[msg->rawlength++] =  msg->MessageType      & 0x7F;

  /* data + footer */
  datlen = convert_8to7bits(msg->buffer, msg->bufferlength, &(msg->raw[msg->rawlength+1]));
  msg->raw[msg->rawlength++] = datlen;
  msg->rawlength += datlen;
  msg->raw[msg->rawlength++] = 0xFF;

  return 0;
}


/* recursively allocates memory and copies data type unions/structs */
void copy_datatype(unsigned char type, int size, const union mbn_data *src, union mbn_data *dest) {
  if(type == MBN_DATATYPE_OCTETS) {
    dest->Octets = malloc(size);
    memcpy((void *)dest->Octets, (void *)src->Octets, size);
  } else if(type == MBN_DATATYPE_ERROR) {
    dest->Error = malloc(size);
    memcpy((void *)dest->Error, (void *)src->Error, size);
  } else if(type == MBN_DATATYPE_OBJINFO) {
    dest->Info = malloc(sizeof(struct mbn_object));
    memcpy((void *)dest->Info, (void *)src->Info, sizeof(struct mbn_object));
    if(src->Info->SensorSize > 0) {
      copy_datatype(src->Info->SensorType, src->Info->SensorSize, &(src->Info->SensorMin), &(dest->Info->SensorMin));
      copy_datatype(src->Info->SensorType, src->Info->SensorSize, &(src->Info->SensorMax), &(dest->Info->SensorMax));
    }
    if(src->Info->ActuatorSize > 0) {
      copy_datatype(src->Info->ActuatorType, src->Info->ActuatorSize, &(src->Info->ActuatorMin), &(dest->Info->ActuatorMin));
      copy_datatype(src->Info->ActuatorType, src->Info->ActuatorSize, &(src->Info->ActuatorMax), &(dest->Info->ActuatorMax));
      copy_datatype(src->Info->ActuatorType, src->Info->ActuatorSize, &(src->Info->ActuatorDefault), &(dest->Info->ActuatorDefault));
    }
  }
}


/* makes a deep copy of a msg struct, can be deallocated later with free_message() */
void copy_message(const struct mbn_message *src, struct mbn_message *dest) {
  memcpy((void *)dest, (void *)src, sizeof(struct mbn_message));
  if(src->bufferlength == 0)
    return;

  if(src->MessageType == MBN_MSGTYPE_ADDRESS)
    return;
  else if(src->MessageType == MBN_MSGTYPE_OBJECT) {
    if(src->Message.Object.DataSize > 0)
      copy_datatype(src->Message.Object.DataType, src->Message.Object.DataSize, &(src->Message.Object.Data), &(dest->Message.Object.Data));
  }
}


