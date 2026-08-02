#include "elos/audio.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel/audio/audio_internal.h"

#include "elos/kernel/audio/hda.h"


#define printf(...) KCON_printf(__VA_ARGS__)


AudioDevice_impl impl_audioDevices[MAX_AUDIO_DEVICES];



#define MAX_AUDIO_BUFFERS 50

typedef struct {
    ELOS_AudioBuffer* buffer;
    u32 tail;
    u32 ringMask;
    bool used;
    // EXEC_Thread* thread; // refer to ID instead?
} KernelAudioBuffer;


volatile u32 g_audio_lock;

KernelAudioBuffer audioBuffers[MAX_AUDIO_BUFFERS];


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

void AUDIO_get_info(AudioDevice _device, ELOS_AudioDeviceInfo* info) {
    AudioDevice_impl* device = (AudioDevice_impl*)_device;
    memcpy(info, &device->audioInfo, sizeof(*info));
}

bool AUDIO_create_buffer(AudioDevice device, ELOS_AudioBuffer** buffer) {
    bool returnValue = false;

    LOCK_INT(&g_audio_lock);

    
    


    // @TODO Does process have enough memory for maxEntries?

    u32 ringMask = ringMaskFromEntryCount(maxEntries);

    AsyncRing* ring = makeAsyncRing(ringMask);
    if (!ring) {
        goto exit;
    }
    
    ring->flags = flags;

    int coreIndex = CPU_get_core_index();
    EXEC_Core* core = &cores[coreIndex];
    EXEC_Thread* activeThread = &core->threads[core->active_thread];
    ring->thread = activeThread;

    *requestRing = ring->requestRing;
    *completionRing = ring->completionRing;

    returnValue = ASYNC_OK;
exit:
    UNLOCK_INT(&g_audio_lock);
    return returnValue;