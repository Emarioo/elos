#pragma once

#include "elos/disk.h"

#include "elos/kernel/driver/pci.h"


typedef volatile struct tagHBA_MEM HBA_MEM;
typedef volatile struct tagHBA_PORT HBA_PORT;

typedef enum {
    DEVICE_TYPE_NONE,
    DEVICE_TYPE_SATA,
    DEVICE_TYPE_NVME,
} DeviceType;

typedef struct {
    DiskDevice* devices;
    int maxCount;
    int count;
} ScanInfo;

typedef struct {
    bool active;

    PCI_ConfigSpace configSpace;
    DeviceType type;
    DiskInfo diskInfo;

    HBA_MEM*  abar;
    HBA_PORT* port;
    int       portNo;

} DiskDevice_impl;


#define MAX_DISK_DEVICES 8
extern DiskDevice_impl impl_diskDevices[MAX_DISK_DEVICES];