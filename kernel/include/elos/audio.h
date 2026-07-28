#pragma once

#include "elos/common/types.h"

#include "elos/boot_api.h"

//###########################
//      TYPES
//###########################

#define DISK_NULL_DEVICE (NULL)

typedef void* AudioDevice;

typedef struct AudioInfo {
    char name[32];
} AudioInfo;

//###########################
//       FUNCTIONS
//###########################

void AUDIO_init(BootAPI* boot_api);

void AUDIO_scan_devices(AudioDevice* devices, int* count);

void AUDIO_get_info(AudioDevice device, AudioInfo* info);
