#include "elos/audio.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel/audio/audio_internal.h"

#include "elos/kernel/audio/hda.h"


AudioDevice_impl impl_audioDevices[MAX_DISK_DEVICES];

#define printf(...) KCON_printf(__VA_ARGS__)



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
    for (int i=0;i<MAX_DISK_DEVICES;i++) {
        if (impl_audioDevices[i].type == AUDIO_TYPE_NONE) {
            device = &impl_audioDevices[i];
            break;
        }
    }
    if (!device) {
        printf("[WARNING] Reached Disk Device limit (%d).\n", MAX_DISK_DEVICES);
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

void AUDIO_get_info(AudioDevice _device, AudioInfo* info) {
    AudioDevice_impl* device = (AudioDevice_impl*)_device;
    memcpy(info, &device->audioInfo, sizeof(*info));
}
