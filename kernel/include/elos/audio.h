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

AudioDevice AUDIO_default_device();

bool AUDIO_get_info(AudioDevice device, ELOS_AudioDeviceInfo* info);

bool AUDIO_create_buffer(AudioDevice device, ELOS_AudioFormat* format, u32 bufferSize, ELOS_AudioBuffer** buffer);

bool AUDIO_destroy_buffer(AudioDevice device,ELOS_AudioBuffer* buffer);

bool AUDIO_control(AudioDevice device, ELOS_AudioOperation operation, size_t value);


