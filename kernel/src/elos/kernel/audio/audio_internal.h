#pragma once

#include "elos/audio.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/audio/hda.h"


typedef enum {
    AUDIO_TYPE_NONE = 0,
    AUDIO_TYPE_HDA,
} AudioDeviceType;

#define AUDIO_DEVICE_NULL NULL

typedef struct ScanInfo ScanInfo;
struct ScanInfo {
    AudioDevice* devices;
    int maxCount;
    int count;
};

typedef struct AudioDevice_impl AudioDevice_impl;
struct AudioDevice_impl {
    AudioDeviceType type;
    ELOS_AudioDeviceInfo audioInfo;

    ELOS_AudioBuffer* audioBuffer;
    // Owned by the kernel and should not be modified by user so we keep these here.
    u32               size;
    u32               tail;

    union {
        struct {
            HDA_Controller* controller;
            int streamNumber;
            int streamIndex;
            u32 prev_lpib;

            // @TODO buffer
        } hda;
    };

};


#define MAX_AUDIO_DEVICES 8
extern AudioDevice_impl impl_audioDevices[MAX_AUDIO_DEVICES];

AudioDevice_impl* AUDIO_reserve_device();

