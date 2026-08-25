#include "elos/audio.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel/audio/audio_internal.h"
#include "elos/physical_memory.h"

#include "elos/kernel/audio/hda.h"
#include "elos/execution.h"


#define printf(...) KCON_printf(__VA_ARGS__)


AudioDevice_impl impl_audioDevices[MAX_AUDIO_DEVICES];



#define MAX_AUDIO_BUFFERS 50

// typedef struct {
//     ELOS_AudioBuffer* buffer;
//     u32 tail;
//     u32 sizeMask;
//     bool used;
//     EXEC_Thread* thread; // refer to ID instead?
// } KernelAudioBuffer;


volatile u32 g_audio_lock;

// KernelAudioBuffer audioBuffers[MAX_AUDIO_BUFFERS];


void AUDIO_init(BootAPI* boot_api) {
    
}


bool AUDIO_find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    if (config->classCode != PCI_CLASSCODE__MULTIMEDIA_CONTROLLER) {
        return false;
    }
    ScanInfo* scanInfo = (ScanInfo*)scanner->user_data;
    
    // printf("Vendor=%x Device=%x Subclass=%d progif=%d\n", config->vendorID, config->deviceID, config->subclass, config->progIF);

    if (config->subclass != PCI_SUBCLASS__AUDIO_DEVICE) {
        return false;
    }
    
    if (config->vendorID == VENDOR_ID__INTEL && config->deviceID == DEVICE_ID__82801FB) {
        hda_scan(scanInfo, config);
    } else {
        printf("Unknown PCI audio device vendorID=0x%x deviceID\n", config->vendorID, config->deviceID);
    }
    
    // False because we want to keep searching.
    return false;
}
  
AudioDevice_impl* AUDIO_reserve_device() {
    AudioDevice_impl* device = NULL;
    for (int i=0;i<MAX_AUDIO_DEVICES;i++) {
        if (impl_audioDevices[i].type == AUDIO_TYPE_NONE) {
            device = &impl_audioDevices[i];
            break;
        }
    }
    if (!device) {
        printf("[WARNING] Reached Disk Device limit (%d).\n", MAX_AUDIO_DEVICES);
        return NULL;
    }
    memset(device, 0, sizeof(*device));
    return device;
}

void AUDIO_scan_devices(AudioDevice* devices, int* count) {

    // @TODO Rescan in case new devices were added. I don't remember
    //    If PCI allows it, probably not, BUT HDA might but probably not.
    //    Rescanning will be much more relevant for USB.

    if (impl_audioDevices->type != AUDIO_TYPE_NONE) {
        int maxCount = *count;
        int index = 0;
        while (index<maxCount) {
            AudioDevice_impl* dev = &impl_audioDevices[index];
            if (dev->type == AUDIO_TYPE_NONE) {
                break;
            }
            devices[index] = dev;
            index++;
        }
        *count = index;
        return;
    }

    ScanInfo scanInfo = {
        .devices = devices,
        .maxCount = *count,
        .count = 0,
    };
    PCI_Scanner scanner = { .func = AUDIO_find_device };
    scanner.user_data = (void*)&scanInfo;


    pci_scan_buses(&scanner);

    *count = scanInfo.count;
}


AudioDevice AUDIO_default_device() {
    for (int i=0;i<ARRAY_LENGTH(impl_audioDevices);i++) {
        AudioDevice_impl* dev = &impl_audioDevices[i];
        if (dev->type != AUDIO_TYPE_NONE) {
            return (AudioDevice)dev;
        }
    }
    return AUDIO_DEVICE_NULL;
}

bool AUDIO_get_info(AudioDevice _device, ELOS_AudioDeviceInfo* info) {
    AudioDevice_impl* device = (AudioDevice_impl*)_device;
    memcpy(info, &device->audioInfo, sizeof(*info));
    return true;
}

// u32 sizeMaskFromBufferSize(u32 size) {
//     if (size == 1)
//         return 1;

//     u32 bit = 31;
//     while (bit >= 0) {
//         u32 shifted_bit = 1 << bit;
//         if (shifted_bit & size)  {
//             if (size-1 && (shifted_bit & size) == 0) {
//                 return size-1;
//             } else if(bit == 31) {
//                 return -1;
//             } else {
//                 return (1 << (bit + 1)) - 1;
//             }
//         }
//         bit--;
//     }
//     return 0;
// }



// KernelAudioBuffer* makeAudioBuffer(u32 bufferSize) {
//     u32 sizeMask = sizeMaskFromBufferSize(bufferSize);
//     if (sizeMask+1 != sizeMask) {
//         return NULL;
//     }

//     KernelAudioBuffer* newBuffer = NULL;
//     for (int i=0;i<ARRAY_LENGTH(audioBuffers);i++) {
//         KernelAudioBuffer* buf = &audioBuffers[i];
//         if (!buf->used) {
//             newBuffer = buf;
//             break;
//         }
//     }
//     if (!newBuffer) {
//         return NULL;
//     }

//     u64 totalBufferSize = sizeof(ELOS_AudioBuffer) + sizeMask + 1;

//     void* bufferAddress = PMEM_alloc_phys(totalBufferSize, PMEM_FLAG_IDENTITY_MAPPED);
//     if (!bufferAddress) {
//         return NULL;
//     }

//     memset(bufferAddress, 0, totalBufferSize);

//     newBuffer->buffer = bufferAddress;
//     newBuffer->sizeMask = sizeMask;
//     *(u32*)&newBuffer->buffer->sizeMask = sizeMask;

//     newBuffer->used = true;
//     return newBuffer;
// }

ELOS_Error AUDIO_create_buffer(AudioDevice _device, ELOS_AudioFormat* format, u32 bufferSize, ELOS_AudioBuffer** buffer) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;
    LOCK_INT(&g_audio_lock);

    AudioDevice_impl* device = (AudioDevice_impl*)_device;
    switch (device->type) {
        case AUDIO_TYPE_NONE: {

        } break;
        case AUDIO_TYPE_HDA: {
            returnValue = hda_create_buffer(device, format, bufferSize, buffer);
        } break;
    }

exit:
    UNLOCK_INT(&g_audio_lock);
    return returnValue;
}
