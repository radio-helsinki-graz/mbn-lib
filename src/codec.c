
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


/* Parses the data part of Address Reservation Messages,
 * returns non-zero on failure */
int parsemsg_address(struct mbn_message *msg) {
  struct mbn_message_address *addr = &(msg->Data.Address);

  if(msg->bufferlength != 16)
    return 1;
  addr->Type = msg->buffer[0];
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
int parse_datatype(unsigned char type, unsigned char *buffer, int length, union mbn_message_object_data *result) {
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
      break;
    case MBN_DATATYPE_SINT:
      if(length < 1 || length > 4)
        return 1;
      /* TODO: How do we parse a variable-length signed integer? */
      break;
    case MBN_DATATYPE_OCTETS:
      if(length < 1 || length > 64)
        return 1;
      /* Note: we add an extra \0 to the octets so using string functions
       * won't trash the application. The standard doesn't require this. */
      result->Octets = malloc(length+1);
      memcpy(result->Octets, buffer, length);
      result->Octets[length] = 0;
      break;
    case MBN_DATATYPE_FLOAT:
      if(length < 1 || length > 4 || length == 3)
        return 1;
      /* TODO */
      break;
    case MBN_DATATYPE_BITS:
      if(length < 1 || length > 8)
        return 1;
      memcpy(result->Bits, buffer, length);
      break;
    case MBN_DATATYPE_OBJINFO:
      if(length < 37 || length > 77)
        return 1;
      /* TODO */
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
  struct mbn_message_object *obj = &(msg->Data.Object);

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
    return 0;
  }

  /* Data, so parse it */
  obj->DataSize = msg->buffer[4];
  if(obj->DataSize != msg->bufferlength-5)
    return 3;

  if((r = parse_datatype(obj->DataType, msg->buffer, obj->DataSize, &(obj->Data))) != 0)
    return r | 8;

  return 0;
}


/* Parses a raw MambaNet message and puts the results back in the struct,
 * returns non-zero on failure */
int parse_message(struct mbn_message *msg) {
  int l, err;

  /* Message is too small for a header to fit */
  if(msg->rawlength < 15)
    return 0x01;

  /* decode MambaNet header */
  msg->ControlByte = msg->raw[0];
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
  msg->DataLength   = ((unsigned char)  msg->raw[14]    ) &       0x7F;

  /* done parsing if there's no data */
  if(msg->DataLength == 0)
    return 0;

  /* check for the validness of the DataLength */
  for(l=0; msg->raw[l+15] != 0xFF && l+15 < msg->rawlength; l++)
    ;
  if(msg->DataLength != l)
    return 0x03;

  /* fill the 8bit buffer */
  msg->bufferlength = convert_7to8bits(&(msg->raw[15]), msg->DataLength, msg->buffer);

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


