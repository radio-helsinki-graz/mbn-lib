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
 *  - Add more H/W interfaces:
 *    > Ethernet (windows)
 *    > TCP/IP (server AND client?)
 *    > Serial line
 *    > SocketCAN
 *  - Test/port to windows (and probably OS X)
 *  - Test suite?
 *  - Documentation (LaTeX? word? manpages?)
*/

#ifndef MBN_H
#define MBN_H

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

#define MBN_ADDR_TIMEOUT    110 /* seconds */
#define MBN_ADDR_MSG_TIMEOUT 30 /* sending address reservation information packets */

#define MBN_ACKNOWLEDGE_RETRIES 5 /* number of times to retry a message requiring an acknowledge */

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
#define MBN_SEND_RAWDATA      0x08 /* don't even create the header, just send the "raw" member */
#define MBN_SEND_ACKNOWLEDGE  0x10 /* require acknowledge, and re-send message after a timeout */
#define MBN_SEND_FORCEID      0x20 /* don't overwrite MessageID field */

/* Global error codes */
enum mbn_error {
  MBN_ERROR_NO_INTERFACE,
  MBN_ERROR_INVALID_ADDR,
  MBN_ERROR_CREATE_MESSAGE,
  MBN_ERROR_PARSE_MESSAGE,
  MBN_ERROR_ITF_READ,
  MBN_ERROR_ITF_WRITE
};
/* must be in the same order as the above enum */
extern const char *mbn_errormessages[];

/* useful macros */
#define MBN_TRACE(x) if(1) { printf("%s:%d:%s(): ", __FILE__, __LINE__, __func__); x; printf("\n"); }
#define MBN_ADDR_EQ(a, b) ( \
    ((a)->ManufacturerID     == 0 || (b)->ManufacturerID     == 0 || (a)->ManufacturerID     == (b)->ManufacturerID)  && \
    ((a)->ProductID          == 0 || (b)->ProductID          == 0 || (a)->ProductID          == (b)->ProductID)       && \
    ((a)->UniqueIDPerProduct == 0 || (b)->UniqueIDPerProduct == 0 || (a)->UniqueIDPerProduct == (b)->UniqueIDPerProduct) \
  )
#define MBN_ERROR(mbn, e) if((mbn)->cb_Error != NULL) { (mbn)->cb_Error(mbn, e, mbn_errormessages[e]); }

/* forward declarations, because many types depend on other types */
struct mbn_node_info;
struct mbn_object;
struct mbn_interface;
struct mbn_message_address;
union  mbn_data;
struct mbn_message_object_information;
struct mbn_message_object;
struct mbn_message;
struct mbn_msgqueue;
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

typedef void(*mbn_cb_Error)(struct mbn_handler *, int, const char *);
typedef void(*mbn_cb_AcknowledgeTimeout)(struct mbn_handler *, struct mbn_message *);
typedef void(*mbn_cb_AcknowledgeReply)(struct mbn_handler *, struct mbn_message *, struct mbn_message *, int);

typedef void(*mbn_cb_InitInterface)(struct mbn_interface *);
typedef void(*mbn_cb_FreeInterface)(struct mbn_interface *);
typedef void(*mbn_cb_FreeInterfaceAddress)(void *);
typedef void(*mbn_cb_InterfaceTransmit)(struct mbn_interface *, unsigned char *, int, void *);


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
  unsigned char Description[64];
  unsigned char Name[32];         /* Variable */
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned char HardwareMajorRevision, HardwareMinorRevision;
  unsigned char FirmwareMajorRevision, FirmwareMinorRevision;
  unsigned char FPGAFirmwareMajorRevision, FPGAFirmwareMinorRevision;
  unsigned short NumberOfObjects;
  unsigned int DefaultEngineAddr; /* Variable */
  unsigned char HardwareParent[6];
  unsigned char ServiceRequest;
};


struct mbn_object {
  unsigned char Description[32];
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
  unsigned int EngineAddr;     /* Variable - pretty much unused */
  /* Services is always 0x03 for sensors and 0x00 for actuators */
  unsigned int timeout; /* internal, sensor change will be sent when timeout reaches 0 */
  char changed; /* internal, used for signaling a change */
};


/* Struct for HW interfaces */
struct mbn_interface {
  void *data; /* can be used by the interface */
  mbn_cb_InitInterface cb_init;
  mbn_cb_FreeInterface cb_free;
  mbn_cb_FreeInterfaceAddress cb_free_addr;
  mbn_cb_InterfaceTransmit cb_transmit;
  struct mbn_handler *mbn;
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
  unsigned int MessageID;
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

struct mbn_msgqueue {
  unsigned int id;
  struct mbn_message msg;
  unsigned int retries;
  struct mbn_msgqueue *next;
};

struct mbn_address_node {
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  unsigned long MambaNetAddr, EngineAddr;
  unsigned char Services;
  int Alive; /* time since we last heard anything from the node */
  void *ifaddr; /* to be used by HW interfaces */
  char used;
};

/* All internal data should go into this struct */
struct mbn_handler {
  struct mbn_node_info node;
  struct mbn_interface *itf;
  int addrsize;
  struct mbn_address_node *addresses;
  struct mbn_object *objects;
  struct mbn_msgqueue *queue;
  int pongtimeout;
  /* pthread objects */
  void *timeout_thread;
  void *throttle_thread;
  void *msgqueue_thread;
  void *mbn_mutex; /* mutex to lock all data in the mbn_handler struct */
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
  mbn_cb_Error cb_Error;
  mbn_cb_AcknowledgeTimeout cb_AcknowledgeTimeout;
  mbn_cb_AcknowledgeReply cb_AcknowledgeReply;
};


/* Function prototypes */
#ifdef __cplusplus
extern "C" {
#endif

struct mbn_handler * MBN_IMPORT mbnInit(struct mbn_node_info, struct mbn_object *, struct mbn_interface *);
void MBN_IMPORT mbnFree(struct mbn_handler *);
struct mbn_interface * MBN_IMPORT mbnEthernetOpen(char *interface);

void MBN_IMPORT mbnProcessRawMessage(struct mbn_interface *, unsigned char *, int, void *);
void MBN_IMPORT mbnSendMessage(struct mbn_handler *, struct mbn_message *, int);

void MBN_IMPORT mbnSendPingRequest(struct mbn_handler *, unsigned long);
struct mbn_address_node * MBN_IMPORT mbnNodeStatus(struct mbn_handler *, unsigned long);
struct mbn_address_node * MBN_IMPORT mbnNextNode(struct mbn_handler *, struct mbn_address_node *);

void MBN_IMPORT mbnUpdateNodeName(struct mbn_handler *, char *);
void MBN_IMPORT mbnUpdateEngineAddr(struct mbn_handler *, unsigned long);
void MBN_IMPORT mbnUpdateServiceRequest(struct mbn_handler *, char);
void MBN_IMPORT mbnUpdateSensorData(struct mbn_handler *, unsigned short, union mbn_data);
void MBN_IMPORT mbnUpdateActuatorData(struct mbn_handler *, unsigned short, union mbn_data);
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
#define mbnSetErrorCallback(mbn, func)                     (mbn->cb_Error = func)
#define mbnUnsetErrorCallback(mbn)                         (mbn->cb_Error = NULL)
#define mbnSetAcknowledgeTimeoutCallback(mbn, func)        (mbn->cb_AcknowledgeTimeout = func)
#define mbnUnsetAcknowledgeTimeoutCallback(mbn)            (mbn->cb_AcknowledgeTimeout = NULL)
#define mbnSetAcknowledgeReplyCallback(mbn, func)          (mbn->cb_AcknowledgeReply = func)
#define mbnUnsetAcknowledgeReplyCallback(mbn)              (mbn->cb_AcknowledgeReply = NULL)



/* One VA_ARG functions embedded in the .h, as exporting them
 *  from .dlls be a bit problematic on some systems */
#ifdef MBN_VARARG
#include <stdarg.h>
#include <string.h>

struct mbn_object MBN_OBJ(char *desc, unsigned char freq, ...) {
  struct mbn_object obj;
  va_list va;
  va_start(va, freq);

  /* basic info */
  memcpy((void *)obj.Description, (void *)desc, strlen((char *)desc));
  obj.EngineAddr = 0;
  obj.UpdateFrequency = freq;

  /* sensor */
  obj.SensorType = (unsigned char) va_arg(va, int);
  switch(obj.SensorType) {
    case MBN_DATATYPE_UINT:
    case MBN_DATATYPE_STATE:
      obj.SensorSize = (unsigned char) va_arg(va, int);
      obj.SensorMin.UInt  = (unsigned long) va_arg(va, unsigned long);
      obj.SensorMax.UInt  = (unsigned long) va_arg(va, unsigned long);
      obj.SensorData.UInt = (unsigned long) va_arg(va, unsigned long);
      break;
    case MBN_DATATYPE_SINT:
      obj.SensorSize = (unsigned char) va_arg(va, int);
      obj.SensorMin.SInt  = (long) va_arg(va, long);
      obj.SensorMax.SInt  = (long) va_arg(va, long);
      obj.SensorData.SInt = (long) va_arg(va, long);
      break;
    case MBN_DATATYPE_FLOAT:
      obj.SensorSize = (unsigned char) va_arg(va, int);
      obj.SensorMin.Float  = (float) va_arg(va, double);
      obj.SensorMax.Float  = (float) va_arg(va, double);
      obj.SensorData.Float = (float) va_arg(va, double);
      break;
    case MBN_DATATYPE_OCTETS:
      obj.SensorSize = (unsigned char) va_arg(va, int);
      obj.SensorMin.Octets  = (unsigned char *) va_arg(va, char *);
      obj.SensorMax.Octets  = (unsigned char *) va_arg(va, char *);
      obj.SensorData.Octets = (unsigned char *) va_arg(va, char *);
      break;
    case MBN_DATATYPE_BITS:
      obj.SensorSize = (unsigned char) va_arg(va, int);
      memcpy((void *)obj.SensorMin.Bits,  (void *)va_arg(va, unsigned char *), obj.SensorSize);
      memcpy((void *)obj.SensorMax.Bits,  (void *)va_arg(va, unsigned char *), obj.SensorSize);
      memcpy((void *)obj.SensorData.Bits, (void *)va_arg(va, unsigned char *), obj.SensorSize);
      break;
    default:
      obj.SensorType = MBN_DATATYPE_NODATA;
      obj.SensorSize = 0;
      break;
  }

  /* actuator */
  obj.ActuatorType = (unsigned char) va_arg(va, int);
  switch(obj.ActuatorType) {
    case MBN_DATATYPE_UINT:
    case MBN_DATATYPE_STATE:
      obj.ActuatorSize = (unsigned char) va_arg(va, int);
      obj.ActuatorMin.UInt  = (unsigned long) va_arg(va, unsigned long);
      obj.ActuatorMax.UInt  = (unsigned long) va_arg(va, unsigned long);
      obj.ActuatorDefault.UInt  = (unsigned long) va_arg(va, unsigned long);
      obj.ActuatorData.UInt = (unsigned long) va_arg(va, unsigned long);
      break;
    case MBN_DATATYPE_SINT:
      obj.ActuatorSize = (unsigned char) va_arg(va, int);
      obj.ActuatorMin.SInt  = (long) va_arg(va, long);
      obj.ActuatorMax.SInt  = (long) va_arg(va, long);
      obj.ActuatorDefault.SInt  = (long) va_arg(va, long);
      obj.ActuatorData.SInt = (long) va_arg(va, long);
      break;
    case MBN_DATATYPE_FLOAT:
      obj.ActuatorSize = (unsigned char) va_arg(va, int);
      obj.ActuatorMin.Float  = (float) va_arg(va, double);
      obj.ActuatorMax.Float  = (float) va_arg(va, double);
      obj.ActuatorDefault.Float  = (float) va_arg(va, double);
      obj.ActuatorData.Float = (float) va_arg(va, double);
      break;
    case MBN_DATATYPE_OCTETS:
      obj.ActuatorSize = (unsigned char) va_arg(va, int);
      obj.ActuatorMin.Octets  = (unsigned char *) va_arg(va, char *);
      obj.ActuatorMax.Octets  = (unsigned char *) va_arg(va, char *);
      obj.ActuatorDefault.Octets  = (unsigned char *) va_arg(va, char *);
      obj.ActuatorData.Octets = (unsigned char *) va_arg(va, char *);
      break;
    case MBN_DATATYPE_BITS:
      obj.ActuatorSize = (unsigned char) va_arg(va, int);
      memcpy((void *)obj.ActuatorMin.Bits,  (void *)va_arg(va, unsigned char *), obj.ActuatorSize);
      memcpy((void *)obj.ActuatorMax.Bits,  (void *)va_arg(va, unsigned char *), obj.ActuatorSize);
      memcpy((void *)obj.ActuatorDefault.Bits,  (void *)va_arg(va, unsigned char *), obj.ActuatorSize);
      memcpy((void *)obj.ActuatorData.Bits, (void *)va_arg(va, unsigned char *), obj.ActuatorSize);
      break;
    default:
      obj.ActuatorType = MBN_DATATYPE_NODATA;
      obj.ActuatorSize = 0;
      break;
  }
  va_end(va);
  return obj;
}


#endif



#endif


