
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

/* Debugging */
#define MBN_TRACE(x) if(1) { printf("%s:%d: ", __FILE__, __LINE__); x; printf("\n"); }


#define MBN_MAX_MESSAGE_SIZE 128
#define MBN_MIN_MESSAGE_SIZE 16


/* All information required for the default objects of a node */
struct mbn_node_info {
  unsigned int MambaNetAddr;      /* Variable */
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
  /* unfinished, needs callbacks (among other things...) */
};


/* Packet information structs */
struct mbn_message {
  unsigned char ControlByte;
  unsigned long AddressTo, AddressFrom;
  unsigned long MessageID;
  unsigned short MessageType;
  unsigned char DataLength;
  unsigned char *buffer;
  int bufferlength;
  /* TODO: union with parsed data */
};


/* All internal data should go into this struct */
struct mbn_handler {
  struct mbn_node_info node;
  struct mbn_interface interface;
};


/* Function prototypes */
#ifdef __cplusplus
extern "C" {
#endif

struct mbn_handler * MBN_IMPORT mbnInit(struct mbn_node_info, struct mbn_interface);

void MBN_IMPORT mbnProcessRawMambaNetMessage(struct mbn_handler *, unsigned char *, int);
int MBN_IMPORT mbnEthernetInit(struct mbn_handler *, char *interface);

#ifdef __cplusplus
}
#endif


#endif

