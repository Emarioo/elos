#pragma once

#include "elos/common/types.h"

#include "elos/boot_api.h"

//###########################
//      TYPES
//###########################

#define DISK_NULL_DEVICE (NULL)

typedef void* DiskDevice;

typedef struct DiskInfo {
    char name[32];
    u64 diskSize;
    u32 blockSize;
} DiskInfo;

//###########################
//       FUNCTIONS
//###########################

void DISK_init(BootAPI* boot_api);

void DISK_scan_devices(DiskDevice* devices, int* count);

void DISK_get_info(DiskDevice device, DiskInfo* info);

bool DISK_write(DiskDevice device, u64 offset, u64 size, void* buffer);

bool DISK_read(DiskDevice device, u64 offset, u64 size, void* buffer);

void DISK_flush(DiskDevice device);
