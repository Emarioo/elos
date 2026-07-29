#pragma once

#include "elos/common/types.h"

#include "elos/boot_api.h"

//###########################
//      TYPES
//###########################

#define AUDIO_NULL_DEVICE (NULL)

typedef void* AudioDevice;


typedef enum {
    AUDIO_8BIT_PCM,
    AUDIO_16BIT_PCM,
    AUDIO_32BIT_PCM,
    AUDIO_32BIT_FLOAT,
} _AudioSampleFormat;
typedef u8 AudioSampleFormat;

typedef struct {
    AudioSampleFormat bitFormat;
    u8  channels;
    u32 sampleRate;
} AudioFormat;

typedef struct AudioDeviceInfo {
    char        name[32];
    AudioFormat format;
} AudioDeviceInfo;


typedef volatile struct {
    u64 head;
    u64 tail;
    u64 sizeMask;
    u8  samples[];
} AudioDeviceBuffer;

//###########################
//       FUNCTIONS
//###########################

void AUDIO_init(BootAPI* boot_api);

void AUDIO_scan_devices(AudioDevice* devices, int* count);

void AUDIO_get_info(AudioDevice device, AudioDeviceInfo* info);



