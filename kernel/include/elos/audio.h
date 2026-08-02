#pragma once

#include "elos/common/types.h"

#include "elos/boot_api.h"

#include "elos/syscalls.h"

//###########################
//      TYPES
//###########################

#define AUDIO_NULL_DEVICE (NULL)

typedef void* AudioDevice;



//###########################
//       FUNCTIONS
//###########################

void AUDIO_init(BootAPI* boot_api);

void AUDIO_scan_devices(AudioDevice* devices, int* count);

void AUDIO_get_info(AudioDevice device, ELOS_AudioDeviceInfo* info);

bool AUDIO_create_buffer(AudioDevice device, u32 maxBytes, ELOS_AudioBuffer** buffer);


