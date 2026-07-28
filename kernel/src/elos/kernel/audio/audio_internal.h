#pragma once

#include "elos/audio.h"

#include "elos/kernel/driver/pci.h"


typedef enum {
    AUDIO_TYPE_NONE = 0,
    AUDIO_TYPE_HDA,
} AudioDeviceType;

typedef struct {
    AudioDevice* devices;
    int maxCount;
    int count;
} ScanInfo;

typedef struct {
    AudioDeviceType type;
    AudioInfo audioInfo;

    union {
        struct {
            PCI_ConfigSpace configSpace;
        } hda;
    };

} AudioDevice_impl;


#define MAX_DISK_DEVICES 8
extern AudioDevice_impl impl_audioDevices[MAX_DISK_DEVICES];
