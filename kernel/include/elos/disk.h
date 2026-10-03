#pragma once

#include "elos/common/types.h"
#include "elos/boot_api.h"
#include "elos/elos.h"

//###########################
//      TYPES
//###########################

typedef enum {
    DISK_TYPE_NONE = 0,
    DISK_TYPE_RAM,
    DISK_TYPE_SATA,
    DISK_TYPE_NVME,
} DiskDeviceType;

typedef enum {
    DISK_ACCESS_READ      = 0x1,
    DISK_ACCESS_WRITE     = 0x2,
    DISK_ACCESS_EXCLUSIVE = 0x4,
} DiskAccess;

typedef struct DiskDevice DiskDevice;
 
struct DiskDevice {
    ELOS_DiskID    diskID;
    DiskDeviceType type;
    ELOS_DiskInfo  diskInfo;

    void* userData;
};

//###########################
//       FUNCTIONS
//###########################

void DISK_init(BootAPI* boot_api);

void DISK_scan_devices(DiskDevice** devices, int* count);

void DISK_enumerate(u64* cookie, u64* entryCount, ELOS_DiskEntry* buffer);

void DISK_get_info(DiskDevice* device, ELOS_DiskInfo* info);

ELOS_Error DISK_write(DiskDevice* device, u64 offset, u64 size, void* buffer);

ELOS_Error DISK_read(DiskDevice* device, u64 offset, u64 size, void* buffer);

// void DISK_flush(DiskDevice* device);
