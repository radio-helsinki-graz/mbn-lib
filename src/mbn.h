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

/* General project-wide TODO list (in addition to `grep TODO *.c`)
 *  - Provide error handling for about everything (using callbacks, most likely)
 *  - Handle object messages
 *  - Improve address table (internal format & API for browsing through the list)
 *  - Improve the API for H/W interface modules
 *  - Add automated acknowledge handling
 *  - Add more H/W interfaces:
 *    > Ethernet (windows)
 *    > TCP/IP (server AND client?)
 *    > Serial line
 *  - Test/port to windows (and probably OS X)
 *  - Functions for fetching another node's default objects?
*/

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

#define MBN_PROTOCOL_VERSION_MAJOR 0
#define MBN_PROTOCOL_VERSION_MINOR 0

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

/* flags for mbnSendMessage() */
#define MBN_SEND_IGNOREVALID  0x01 /* send the message regardless of our valid bit */
#define MBN_SEND_FORCEADDR    0x02 /* don't overwrite the AddressTo field with our address */
#define MBN_SEND_NOCREATE     0x04 /* don't try to parse the structs, just send the "buffer" member */
#define MBN_SEND_RAWDATA      (0x08 | 0x04 | 0x02) /* don't even create the header, just send the "raw" member */

#define MBN_ADDR_EQ(a, b) ( \
    ((a)->ManufacturerID     == 0 || (b)->ManufacturerID     == 0 || (a)->ManufacturerID     == (b)->ManufacturerID)  && \
    ((a)->ProductID          == 0 || (b)->ProductID          == 0 || (a)->ProductID          == (b)->ProductID)       && \
    ((a)->UniqueIDPerProduct == 0 || (b)->UniqueIDPerProduct == 0 || (a)->UniqueIDPerProduct == (b)->UniqueIDPerProduct) \
  )

/* forward declarations, because many types depend on other types */
struct mbn_node_info;
struct mbn_object;
struct mbn_interface;
struct mbn_message_address;
union  mbn_data;
struct mbn_message_object_information;
struct mbn_message_object;
struct mbn_message;
struct mbn_address_node;
struct mbn_handler;


/* Callback function prototypes */
typedef int(*mbn_cb_ReceiveMessage)(struct mbn_handler *, struct mbn_message *);
typedef void(*mbn_cb_AddressTableChange)(struct mbn_handler *, struct mbn_address_node *, struct mbn_address_node *);
typedef void(*mbn_cb_OnlineStatus)(struct mbn_handler *, unsigned long, char);
typedef int(*mbn_cb_NameChange)(struct mbn_handler *, unsigned char *);
typedef int(*mbn_cb_DefaultEngineAddrChange)(struct mbn_handler *, unsigned long);
typedef int(*mbn_cb_SetActuatorData)(struct mbn_handler *, unsigned short, union mbn_data);
typedef int(*mbn_cb_GetSensorData)(struct mbn_handler *, unsigned short, union mbn_data *);
typedef void(*mbn_cb_ObjectFrequencyChange)(struct mbn_handler *, unsigned short, unsigned char);

typedef int(*mbn_cb_ObjectInformationResponse)(struct mbn_handler *, struct mbn_message *, unsigned short, struct mbn_message_object_information *);
typedef int(*mbn_cb_ObjectFrequencyResponse)(struct mbn_handler *, struct mbn_message *, unsigned short, unsigned char);
typedef int(*mbn_cb_ObjectDataResponse)(struct mbn_handler *, struct mbn_message *, unsigned short, unsigned char, union mbn_data);

typedef void(*mbn_cb_FreeInterface)(struct mbn_handler *);
typedef void(*mbn_cb_FreeInterfaceAddress)(void *);
typedef void(*mbn_cb_InterfaceTransmit)(struct mbn_handler *, unsigned char *, int, void *);


/* large data types should be allocated (and freed)
 * manually, to preserve memory for smaller data types */
union mbn_data {
  long SInt;
  float Float;
  unsigned long UInt;
  unsigned long State;
  unsigned char Bits[8];
  unsigned char *Octets;
  unsigned char *Error;
  struct mbn_message_object_information *Info;
};

/* All information required for the default objects of a node */
struct mbn_node_info {
  unsigned int MambaNetAddr;      /* Variable */
  char Services;                  /* Variable (MSBit) */
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned char Description[64];
  unsigned char Name[32];         /* Variable */
  unsigned char HardwareMajorRevision, HardwareMinorRevision;
  unsigned char FirmwareMajorRevision, FirmwareMinorRevision;
  unsigned short NumberOfObjects;
  unsigned int DefaultEngineAddr; /* Variable */
  unsigned char HardwareParent[6];
};


struct mbn_object {
  unsigned char Description[32];
  unsigned int EngineAddr;        /* Variable */
  unsigned char UpdateFrequency;  /* Variable */
  unsigned char SensorType;
  unsigned char SensorSize;
  union mbn_data SensorMin, SensorMax;
  union mbn_data SensorData;   /* Variable (only from the application) */
  unsigned char ActuatorType;
  unsigned char ActuatorSize;
  union mbn_data ActuatorMin, ActuatorMax;
  union mbn_data ActuatorDefault;
  union mbn_data ActuatorData; /* Variable */
  /* Services is always 0x03 for sensors and 0x00 for actuators */
  unsigned int timeout; /* internal, sensor change will be sent when timeout reaches 0 */
  char changed; /* internal, used for signaling a change */
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

struct mbn_message_object_information {
  unsigned char Description[33];
  unsigned char Services;
  unsigned char SensorType;
  unsigned char SensorSize;
  union mbn_data SensorMin;
  union mbn_data SensorMax;
  unsigned char ActuatorType;
  unsigned char ActuatorSize;
  union mbn_data ActuatorMin;
  union mbn_data ActuatorMax;
  union mbn_data ActuatorDefault;
};

struct mbn_message_object {
  unsigned short Number;
  unsigned char Action;
  unsigned char DataType;
  unsigned char DataSize;
  union mbn_data Data;
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
  struct mbn_object *objects;
  int pongtimeout;
  pthread_t timeout_thread; /* make this a void pointer? now the app requires pthread.h */
  pthread_t throttle_thread;
  pthread_mutex_t mbn_mutex; /* mutex to lock all data in the mbn_handler struct (except the mutex itself, of course) */
  /* callbacks */
  mbn_cb_ReceiveMessage cb_ReceiveMessage;
  mbn_cb_AddressTableChange cb_AddressTableChange;
  mbn_cb_OnlineStatus cb_OnlineStatus;
  mbn_cb_NameChange cb_NameChange;
  mbn_cb_DefaultEngineAddrChange cb_DefaultEngineAddrChange;
  mbn_cb_SetActuatorData cb_SetActuatorData;
  mbn_cb_GetSensorData cb_GetSensorData;
  mbn_cb_ObjectFrequencyChange cb_ObjectFrequencyChange;
  mbn_cb_ObjectInformationResponse cb_ObjectInformationResponse;
  mbn_cb_ObjectFrequencyResponse cb_ObjectFrequencyResponse;
  mbn_cb_ObjectDataResponse cb_SensorDataResponse;
  mbn_cb_ObjectDataResponse cb_SensorDataChanged;
  mbn_cb_ObjectDataResponse cb_ActuatorDataResponse;
};


/* Function prototypes */
#ifdef __cplusplus
extern "C" {
#endif

struct mbn_handler * MBN_IMPORT mbnInit(struct mbn_node_info, struct mbn_object *);
void MBN_IMPORT mbnFree(struct mbn_handler *);
int MBN_IMPORT mbnEthernetInit(struct mbn_handler *, char *interface);

void MBN_IMPORT mbnProcessRawMessage(struct mbn_handler *, unsigned char *, int, void *);
void MBN_IMPORT mbnSendMessage(struct mbn_handler *, struct mbn_message *, int);

void MBN_IMPORT mbnSendPingRequest(struct mbn_handler *, unsigned long);
struct mbn_address_node * MBN_IMPORT mbnNodeStatus(struct mbn_handler *, unsigned long);

void MBN_IMPORT mbnSensorDataChange(struct mbn_handler *, unsigned short, union mbn_data);
void MBN_IMPORT mbnGetSensorData(struct mbn_handler *, unsigned long, unsigned short, char);
void MBN_IMPORT mbnGetActuatorData(struct mbn_handler *, unsigned long, unsigned short, char);
void MBN_IMPORT mbnGetObjectInformation(struct mbn_handler *, unsigned long, unsigned short, char);
void MBN_IMPORT mbnGetObjectFrequency(struct mbn_handler *, unsigned long, unsigned short, char);
void MBN_IMPORT mbnSetActuatorData(struct mbn_handler *, unsigned long, unsigned short, unsigned char, unsigned char, union mbn_data, char);
void MBN_IMPORT mbnSetObjectFrequency(struct mbn_handler *, unsigned long, unsigned short, unsigned char, char);

#ifdef __cplusplus
}
#endif


/* Things that look like functions to the application but are secretly macros.
 * (This is both faster because no extra functions have to be called, and makes
 *  the library smaller because no extra functions have to be exported)
 * These macros may still be replaced with proper functions when needed in the future.
 */
#define mbnSetInterface(mbn, itf)                          (mbn->interface = interface)
#define mbnSetReceiveMessageCallback(mbn, func)            (mbn->cb_ReceiveMessage = func)
#define mbnUnsetReceiveMessageCallback(mbn)                (mbn->cb_ReceiveMessage = NULL)
#define mbnSetAddressTableChangeCallback(mbn, func)        (mbn->cb_AddressTableChange = func)
#define mbnUnsetAddressTableChangeCallback(mbn)            (mbn->cb_AddressTableChange = NULL)
#define mbnSetOnlineStatusCallback(mbn, func)              (mbn->cb_OnlineStatus = func)
#define mbnUnsetOnlineStatusCallback(mbn)                  (mbn->cb_OnlineStatus = NULL)
#define mbnSetNameChangeCallback(mbn, func)                (mbn->cb_NameChange = func)
#define mbnUnsetNameChangeCallback(mbn)                    (mbn->cb_NameChange = NULL)
#define mbnSetDefaultEngineAddrChangeCallback(mbn, func)   (mbn->cb_DefaultEngineAddrChange = func)
#define mbnUnsetDefaultEngineAddrChangeCallback(mbn)       (mbn->cb_DefaultEngineAddrChange = NULL)
#define mbnSetSetActuatorDataCallback(mbn, func)           (mbn->cb_SetActuatorData = func)
#define mbnUnsetSetActuatorDataCallback(mbn)               (mbn->cb_SetActuatorData = NULL)
#define mbnSetGetSensorDataCallback(mbn, func)             (mbn->cb_GetSensorData = func)
#define mbnUnsetGetSensorDataCallback(mbn)                 (mbn->cb_GetSensorData = NULL)
#define mbnSetObjectFrequencyChangeCallback(mbn, func)     (mbn->cb_ObjectFrequencyChange = func)
#define mbnUnsetObjectFrequencyChangeCallback(mbn)         (mbn->cb_ObjectFrequencyChange = NULL)
#define mbnSetObjectInformationResponseCallback(mbn, func) (mbn->cb_ObjectInformationResponse = func)
#define mbnUnsetObjectInformationResponseCallback(mbn)     (mbn->cb_ObjectInformationResponse = NULL)
#define mbnSetObjectFrequencyResponseCallback(mbn, func)   (mbn->cb_ObjectFrequencyResponse = func)
#define mbnUnsetObjectFrequencyResponseCallback(mbn)       (mbn->cb_ObjectFrequencyResponse = NULL)
#define mbnSetSensorDataResponseCallback(mbn, func)        (mbn->cb_SensorDataResponse = func)
#define mbnUnsetSensorDataResponseCallback(mbn)            (mbn->cb_SensorDataResponse = NULL)
#define mbnSetSensorDataChangedCallback(mbn, func)         (mbn->cb_SensorDataChanged = func)
#define mbnUnsetSensorDataChangedCallback(mbn)             (mbn->cb_SensorDataChanged = NULL)
#define mbnSetActuatorDataResponseCallback(mbn, func)      (mbn->cb_ActuatorDataResponse = func)
#define mbnUnsetActuatorDataResponseCallback(mbn)          (mbn->cb_ActuatorDataResponse = NULL)

#endif


