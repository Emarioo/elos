#pragma once

#include "elos/disk.h"

#include "elos/kernel/driver/pci.h"


typedef volatile struct tagHBA_MEM HBA_MEM;
typedef volatile struct tagHBA_PORT HBA_PORT;

typedef enum {
    DISK_TYPE_NONE = 0,
    DISK_TYPE_RAM,
    DISK_TYPE_SATA,
    DISK_TYPE_NVME,
} DiskDeviceType;

typedef struct NVME_Context NVME_Context;

struct DiskDevice {
    DiskDeviceType type;
    DiskInfo diskInfo;

    union {
        struct {
            void* data;
            u64   size;
        } ram;
        struct {
            PCI_ConfigSpace configSpace;
            HBA_MEM*  abar;
            HBA_PORT* port;
            int       portNo;
        } sata;
        struct {
            PCI_ConfigSpace configSpace;
            NVME_Context*   nvme_context;
        } nvme;
    };
};

typedef struct {
    DiskDevice** devices;
    int maxCount;
    int count;
} ScanInfo;


#define MAX_DISK_DEVICES 8
extern DiskDevice g_diskDevices[MAX_DISK_DEVICES];