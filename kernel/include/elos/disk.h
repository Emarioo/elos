#pragma once

#include "elos/common/types.h"

//###########################
//      TYPES
//###########################

typedef void* DiskDevice;

typedef struct DiskInfo {
    char name[32];
    u64 diskSize;
    u32 blockSize;
} DiskInfo;

//###########################
//       FUNCTIONS
//###########################


void DISK_scan_devices(DiskDevice* devices, int* count);

void DISK_get_info(DiskDevice device, DiskInfo* info);

bool DISK_write(DiskDevice device, u64 offset, u64 size, void* buffer);

bool DISK_read(DiskDevice device, u64 offset, u64 size, void* buffer);

void DISK_flush(DiskDevice device);
