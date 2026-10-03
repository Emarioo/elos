#pragma once

#include "elos/disk.h"

#include "elos/kernel/driver/pci.h"

//#############################
//           TYPES
//#############################

typedef volatile struct tagHBA_MEM HBA_MEM;
typedef volatile struct tagHBA_PORT HBA_PORT;


typedef struct {
    u8*   data;
    u64   size;
} Disk_RAM_Context;

typedef struct {
    PCI_ConfigSpace configSpace;
    HBA_MEM*  abar;
    HBA_PORT* port;
    int       portNo;
} Disk_SATA_Context;


typedef struct {
    DiskDevice** devices;
    int maxCount;
    int count;
} Disk_ScanInfo;


#define MAX_DISK_DEVICES 8

extern DiskDevice g_diskDevices[MAX_DISK_DEVICES];


//#############################
//          FUNCTIONS
//#############################

DiskDevice* disk_reserve_device();

bool disk_find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config);
