#include "elos/disk.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel/disk/disk_internal.h"
#include "elos/kernel/disk/ahci.h"


DiskDevice_impl impl_diskDevices[MAX_DISK_DEVICES];

#define printf(...) KCON_printf(__VA_ARGS__)


static void* g_initrd;
static u64   g_initrd_size;

void DISK_init(BootAPI* boot_api) {
    g_initrd = boot_api->initrd;
    g_initrd_size = boot_api->initrd_size;
}


bool DISK_find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    if (config->classCode != PCI_CLASSCODE__MASS_STORAGE_CONTROLLER) {
        return false;
    }
    ScanInfo* scanInfo = (ScanInfo*)scanner->user_data;
    
    // printf("Vendor=%x Device=%x Subclass=%d progif=%d\n", config->vendorID, config->deviceID, config->subclass, config->progIF);

    if (config->subclass == PCI_SUBCLASS__SERIAL_ATA_CONTROLLER) {
        
        return ahci_scan(scanInfo, config);

    // } else if (config->subclass == PCI_SUBCLASS__NON_VOLATILE_MEMORY_CONTROLLER && config->progIF == 0x2) { // NVM Express
   
    //     if (scanInfo->count >= scanInfo->maxCount) {
    //         // Stop searching, no more room.
    //         return true;
    //     }

    //     DiskDevice_impl* device = NULL;
    //     for (int i=0;i<MAX_DISK_DEVICES;i++) {
    //         if (impl_diskDevices[i].type == DISK_TYPE_NONE) {
    //             device = &impl_diskDevices[i];
    //             break;
    //         }
    //     }
    //     if (!device) {
    //         printf("[WARNING] Reached Disk Device limit (%d).\n", MAX_DISK_DEVICES);
    //         // Stop searching, no more room.
    //         return true;
    //     }


    //     device->type = DISK_TYPE_NVME;
    //     device->nvme.configSpace = *config;

    //     scanInfo->devices[scanInfo->count] = (DiskDevice)device;
    //     scanInfo->count++;

        // nvme_init(device);
   
    } else {
        return false;
    }

    
    // False because we want to keep searching.
    return false;
}
  
DiskDevice_impl* DISK_reserve_device() {
    DiskDevice_impl* device = NULL;
    for (int i=0;i<MAX_DISK_DEVICES;i++) {
        if (impl_diskDevices[i].type == DISK_TYPE_NONE) {
            device = &impl_diskDevices[i];
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

void DISK_scan_devices(DiskDevice* devices, int* count) {
    ScanInfo scanInfo = {
        .devices = devices,
        .maxCount = *count,
        .count = 0,
    };
    PCI_Scanner scanner = { .func = DISK_find_device };
    scanner.user_data = (void*)&scanInfo;

    if (g_initrd) {
        DiskDevice_impl* dev = DISK_reserve_device();

        dev->type = DISK_TYPE_RAM;
        dev->ram.data = g_initrd;
        dev->ram.size = g_initrd_size;
        snprintf(dev->diskInfo.name, sizeof(dev->diskInfo.name), "initrd");
        dev->diskInfo.diskSize = g_initrd_size;
        dev->diskInfo.blockSize = 512; // Hardcoding this might cause problems.

        scanInfo.devices[scanInfo.count] = (DiskDevice)dev;
        scanInfo.count++;
    }

    pci_scan_buses(&scanner);

    *count = scanInfo.count;
}

void DISK_get_info(DiskDevice _device, DiskInfo* info) {
    DiskDevice_impl* device = (DiskDevice_impl*)_device;
    memcpy(info, &device->diskInfo, sizeof(*info));
}

bool DISK_write(DiskDevice _device, u64 offset, u64 size, void* buffer) {
    DiskDevice_impl* device = (DiskDevice_impl*)_device;

    switch (device->type) {
        case DISK_TYPE_RAM: {
            if (size == 0) {
                return false;
            }
            if (offset + size > device->ram.size) {
                printf("DISK_write: out of bounds read on (0x%zx:0x%zx, 0x%zx) from %s\n", offset, size, device->ram.size, device->diskInfo.name);
                return false;
            }
            memcpy((char*)device->ram.data + offset, buffer, size);
        } break;
        case DISK_TYPE_SATA: {
            bool res = ahci_write(device, offset, size, buffer);
            if (!res) {
                printf("DISK_write: Could not read (%x, %d) from %s\n", offset, size, device->diskInfo.name);
                return false;
            }
        } break;
        default: {
            printf("DISK_write: type %d not implemented\n", device->type);
            return false;
        } break;
    }
    return true;
}

bool DISK_read(DiskDevice _device, u64 offset, u64 size, void* buffer) {
    DiskDevice_impl* device = (DiskDevice_impl*)_device;

    switch (device->type) {
        case DISK_TYPE_RAM: {
            if (size == 0) {
                return false;
            }
            if (offset + size > device->ram.size) {
                printf("DISK_read: out of bounds read on (off=0x%zx sz=0x%zx diskSize=0x%zx) from %s\n", offset, size, device->ram.size, device->diskInfo.name);
                return false;
            }
            memcpy(buffer, (char*)device->ram.data + offset, size);
        } break;
        case DISK_TYPE_SATA: {
            bool res = ahci_read(device, offset, size, buffer);
            if (!res) {
                printf("DISK_read: Could not read (%x, %d) from %s\n", offset, size, device->diskInfo.name);
                return false;
            }
        } break;
        default: {
            printf("DISK_read: type %d not implemented\n", device->type);
            return false;
        } break;
    }
    return true;
}

void DISK_flush(DiskDevice device) {
    // @TODO Implement
}

