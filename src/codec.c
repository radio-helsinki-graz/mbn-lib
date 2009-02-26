

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


/* Parses the data part of Address Reservation Messages */
void parsemsg_address(struct mbn_message *msg) {
  struct mbn_message_address *addr = &(msg->Data.Address);

  addr->Type = msg->buffer[0];
  addr->ManufacturerID     = ((unsigned short) msg->buffer[ 1]<< 8) | (unsigned short) msg->buffer[ 2];
  addr->ProductID          = ((unsigned short) msg->buffer[ 3]<< 8) | (unsigned short) msg->buffer[ 4];
  addr->UniqueIDPerProduct = ((unsigned short) msg->buffer[ 5]<< 8) | (unsigned short) msg->buffer[ 6];
  addr->MambaNetAddr       = ((unsigned long)  msg->buffer[ 7]<<24) |((unsigned long)  msg->buffer[ 8]<<16)
                           | ((unsigned long)  msg->buffer[ 9]<< 8) | (unsigned long)  msg->buffer[10];
  addr->EngineAddr         = ((unsigned long)  msg->buffer[11]<<24) |((unsigned long)  msg->buffer[12]<<16)
                           | ((unsigned long)  msg->buffer[13]<< 8) | (unsigned long)  msg->buffer[14];
  addr->Services = msg->buffer[15];
}


/* Parses a raw MambaNet message and puts the results back in the struct */
void parse_message(struct mbn_message *msg) {
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
    return;

  /* fill the 8bit buffer */
  msg->bufferlength = convert_7to8bits(&(msg->raw[15]), msg->DataLength, msg->buffer);

  /* parse address reservation messages */
  if(msg->MessageType == MBN_MSGTYPE_ADDRESS)
    parsemsg_address(msg);
}


