

typedef int(*mbn_callback)(int*);


// All information required for the default objects of a node
struct mbn_node_info {
  unsigned int MambaNetAddr;      // Variable
  unsigned short ManufacturerID, ProductID, UniqueIDPerProduct;
  char Description[64];
  char Name[32];                  // Variable
  unsigned char HardwareMajorRevision, HardwareMinorRevision;
  unsigned char FirmwareMajorRevision, FirmwareMinorRevision;
  unsigned short NumberOfObjects;
  unsigned int DefaultEngineAddr; // Variable
  unsigned char HardwareParent[6];
};


// All internal data should go into this struct
struct mbn_handler {
  mbn_callback cb;
  struct mbn_node_info node;
};


struct mbn_handler *mbnInit(struct mbn_node_info, mbn_callback);


