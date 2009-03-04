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

#ifndef MBN_H
#define MBN_H

#include <pthread.h>

/* Global way to determine platform, considering we only
 * provide this stack for windows and linux at this point,
 * this method should be enough. It might be a better idea
 * to check for the existance of each function or interface
 * in the future, to provide better cross-platform
 * compilation.
 */
#ifdef __GNUC__
# define MBN_LINUX
# undef  MBN_WINDOWS
#else
# undef  MVN_LINUX
# define MBN_WINDOWS
#endif

/* Windows DLLs want flags for exportable functions */
#ifdef MBN_WINDOWS
# define MBN_IMPORT __declspec(dllimport) __stdcall
# define MBN_EXPORT __declspec(dllexport) __stdcall
#else
# define MBN_IMPORT
# define MBN_EXPORT
#endif

/* Debugging */
#define MBN_TRACE(x) if(1) { printf("%s:%d:%s(): ", __FILE__, __LINE__, __func__); x; printf("\n"); }

#define MBN_ADDR_TIMEOUT    110 /* seconds */
#define MBN_ADDR_MSG_TIMEOUT 30 /* sending address reservation information packets */

#define MBN_BROADCAST_ADDRESS 0x10000000

#define MBN_MAX_MESSAGE_SIZE 128
#define MBN_MIN_MESSAGE_SIZE 16

#define MBN_MSGTYPE_ADDRESS 0x00
#define MBN_MSGTYPE_OBJECT  0x01

#define MBN_ADDR_TYPE_INFO     0x00
#define MBN_ADDR_TYPE_PING     0x01
#define MBN_ADDR_TYPE_RESPONSE 0x02

#define MBN_ADDR_SERVICES_SERVER 0x01
#define MBN_ADDR_SERVICES_ENGINE 0x02
#define MBN_ADDR_SERVICES_ERROR  0x40
#define MBN_ADDR_SERVICES_VALID  0x80

#define MBN_OBJ_ACTION_GET_INFO           0
#define MBN_OBJ_ACTION_INFO_RESPONSE      1
#define MBN_OBJ_ACTION_GET_ENGINE         2
#define MBN_OBJ_ACTION_ENGINE_RESPONSE    3
#define MBN_OBJ_ACTION_SET_ENGINE         4
#define MBN_OBJ_ACTION_GET_FREQUENCY      5
#define MBN_OBJ_ACTION_FREQUENCY_RESPONSE 6
#define MBN_OBJ_ACTION_SET_FREQUENCY      7
#define MBN_OBJ_ACTION_GET_SENSOR        32
#define MBN_OBJ_ACTION_SENSOR_RESPONSE   33
#define MBN_OBJ_ACTION_SENSOR_CHANGED    34
#define MBN_OBJ_ACTION_GET_ACTUATOR      64
#define MBN_OBJ_ACTION_ACTUATOR_RESPONSE 65
#define MBN_OBJ_ACTION_SET_ACTUATOR      66

#define MBN_DATATYPE_NODATA    0
#define MBN_DATATYPE_UINT      1
#define MBN_DATATYPE_SINT      2
#define MBN_DATATYPE_STATE     3
#define MBN_DATATYPE_OCTETS    4
#define MBN_DATATYPE_FLOAT     5
#define MBN_DATATYPE_BITS      6
#define MBN_DATATYPE_OBJINFO 128
#define MBN_DATATYPE_ERROR   255

#define MBN_ADDR_EQ(a, b) ( \
    ((a)->ManufacturerID     == 0 || (b)->ManufacturerID     == 0 || (a)->ManufacturerID     == (b)->ManufacturerID)  && \
    ((a)->ProductID          == 0 || (b)->ProductID          == 0 || (a)->ProductID          == (b)->ProductID)       && \
    ((a)->UniqueIDPerProduct == 0 || (b)->UniqueIDPerProduct == 0 || (a)->UniqueIDPerProduct == (b)->UniqueIDPerProduct) \
  )

/* forward declarations, because many types depend on other types */
struct mbn_node_info;
struct mbn_interface;
struct mbn_message_address;
union  mbn_message_object_data;
struct mbn_message_object_information;
struct mbn_message_object;
struct mbn_message;
struct mbn_address_node;
struct mbn_handler;


/* Callback function prototypes */
typedef int(*mbn_cb_ReceiveMessage)(struct mbn_handler *, struct mbn_message *);
typedef void(*mbn_cb_AddressTableChange)(struct mbn_handler *, struct mbn_address_node *, struct mbn_address_node *);
typedef void(*mbn_cb_OnlineStatus)(struct mbn_handler *, unsigned long, char);

typedef void(*mbn_cb_FreeInterface)(struct mbn_handler *);
typedef void(*mbn_cb_FreeInterfaceAddress)(void *);
typedef void(*mbn_cb_InterfaceTransmit)(struct mbn_handler *, unsigned char *, int, void *);


/* All information required for the default objects of a node */
struct mbn_node_info {
  unsigned int MambaNetAddr;      /* Variable */
  char Services;                  /* Variable (MSBit) */
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  char Description[64];
  char Name[32];                  /* Variable */
  unsigned char HardwareMajorRevision, HardwareMinorRevision;
  unsigned char FirmwareMajorRevision, FirmwareMinorRevision;
  unsigned short NumberOfObjects;
  unsigned int DefaultEngineAddr; /* Variable */
  unsigned char HardwareParent[6];
};


/* Struct for HW interfaces */
struct mbn_interface {
  void *data; /* can be used by the interface */
  mbn_cb_FreeInterface cb_free;
  mbn_cb_FreeInterfaceAddress cb_free_addr;
  mbn_cb_InterfaceTransmit cb_transmit;
};


/* Packet information structs */
struct mbn_message_address {
  unsigned char Type;
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned long MambaNetAddr, EngineAddr;
  unsigned char Services;
};

/* large data types should be allocated (and freed)
 * manually, to preserve memory for smaller data types */
union mbn_message_object_data {
  float Float;
  unsigned long UInt;
  long SInt;
  unsigned long State;
  unsigned char Bits[8];
  unsigned char *Octets;
  unsigned char *Error;
  struct mbn_message_object_information *Info;
};

struct mbn_message_object_information {
  unsigned char Description[33];
  unsigned char Services;
  unsigned char SensorType;
  unsigned char SensorSize;
  union mbn_message_object_data SensorMin;
  union mbn_message_object_data SensorMax;
  unsigned char ActuatorType;
  unsigned char ActuatorSize;
  union mbn_message_object_data ActuatorMin;
  union mbn_message_object_data ActuatorMax;
  union mbn_message_object_data ActuatorDefault;
};

struct mbn_message_object {
  unsigned short Number;
  unsigned char Action;
  unsigned char DataType;
  unsigned char DataSize;
  union mbn_message_object_data Data;
};

struct mbn_message {
  unsigned char AcknowledgeReply;
  unsigned long AddressTo, AddressFrom;
  unsigned long MessageID;
  unsigned short MessageType;
  union {
    struct mbn_message_address Address;
    struct mbn_message_object Object;
  } Data;
  /* used internally */
  unsigned char *raw;
  int rawlength;
  unsigned char buffer[98];
  int bufferlength;
};

struct mbn_address_node {
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned long MambaNetAddr, EngineAddr;
  unsigned char Services;
  int Alive; /* time since we last heard anything from the node */
  void *ifaddr; /* to be used by HW interfaces */
  struct mbn_address_node *next; /* singly linked list */
};

/* All internal data should go into this struct */
struct mbn_handler {
  struct mbn_node_info node;
  struct mbn_interface interface;
  struct mbn_address_node *addresses;
  char validated;
  int pongtimeout;
  pthread_t timeout_thread; /* make this a void pointer? now the app requires pthread.h */
  pthread_mutex_t mbn_mutex; /* mutex to lock all data in the mbn_handler struct (except the mutex itself, of course) */
  mbn_cb_ReceiveMessage cb_ReceiveMessage;
  mbn_cb_AddressTableChange cb_AddressTableChange;
  mbn_cb_OnlineStatus cb_OnlineStatus;
};


/* Function prototypes */
#ifdef __cplusplus
extern "C" {
#endif

struct mbn_handler * MBN_IMPORT mbnInit(struct mbn_node_info);
void MBN_IMPORT mbnFree(struct mbn_handler *);
int MBN_IMPORT mbnEthernetInit(struct mbn_handler *, char *interface);

void MBN_IMPORT mbnProcessRawMessage(struct mbn_handler *, unsigned char *, int, void *);
void MBN_IMPORT mbnSendMessage(struct mbn_handler *, struct mbn_message *);

struct mbn_address_node * MBN_IMPORT mbnNodeStatus(struct mbn_handler *, unsigned int);

#ifdef __cplusplus
}
#endif


/* Things that look like functions to the application but are secretly macros.
 * (This is both faster because no extra functions have to be called, and makes
 *  the library smaller because no extra functions have to be exported)
 * These macros may still be replaced with proper functions when needed in the future.
 */
#define mbnSetInterface(mbn, itf)                   (mbn->interface = interface)
#define mbnSetReceiveMessageCallback(mbn, func)     (mbn->cb_ReceiveMessage = func)
#define mbnUnsetReceiveMessageCallback(mbn)         (mbn->cb_ReceiveMessage = NULL)
#define mbnSetAddressTableChangeCallback(mbn, func) (mbn->cb_AddressTableChange = func)
#define mbnUnsetAddressTableChangeCallback(mbn)     (mbn->cb_AddressTableChange = NULL)
#define mbnSetOnlineStatusCallback(mbn, func)       (mbn->cb_OnlineStatus = func)
#define mbnUnsetOnlineStatusCallback(mbn)           (mbn->cb_OnlineStatus = NULL)

#endif


