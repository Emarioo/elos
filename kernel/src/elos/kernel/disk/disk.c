#include "elos/disk.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel/disk/disk_internal.h"
#include "elos/kernel/disk/ahci.h"



typedef struct {
    DiskDevice* devices;
    int maxCount;
    int count;
} ScanInfo;

#define MAX_DISK_DEVICES 8
DiskDevice_impl impl_diskDevices[MAX_DISK_DEVICES];

#define printf(...) KCON_printf(__VA_ARGS__)

bool DISK_find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    // printf("Vendor=%x Device=%x Subclass=%d\n", config->vendorID, config->deviceID, config->subclass);
    if (config->classCode != PCI_CLASSCODE__MASS_STORAGE_CONTROLLER) {
        return false;
    }
    if (config->subclass != PCI_SUBCLASS__ATA_CONTROLLER && config->subclass != PCI_SUBCLASS__IDE_CONTROLLER && config->subclass != PCI_SUBCLASS__SERIAL_ATA_CONTROLLER) {
        return false;
    }

    if (config->vendorID == VENDOR_ID__INTEL && config->deviceID == DEVICE_ID__ICH9R) {
        ScanInfo* scanInfo = (ScanInfo*)scanner->user_data;
        if (scanInfo->count >= scanInfo->maxCount)
            return true; // stop searching

        DiskDevice_impl* device = NULL;
        for (int i=0;i<MAX_DISK_DEVICES;i++) {
            if (!impl_diskDevices[i].used) {
                device = &impl_diskDevices[i];
                break;
            }
        }
        if (!device) {
            printf("[WARNING] Reached Disk Device limit (%d).\n", MAX_DISK_DEVICES);
            return true; // stop searching
        }


        device->configSpace = *config;

        scanInfo->devices[scanInfo->count] = (DiskDevice)device;
        scanInfo->count++;
        
        return false; // Keep searching
    }

    return false;
}


void DISK_scan_devices(DiskDevice* devices, int* count) {
    ScanInfo scanInfo = {
        .devices = devices,
        .maxCount = *count,
        .count = 0,
    };
    PCI_Scanner scanner = { .func = DISK_find_device };
    scanner.user_data = (void*)&scanInfo;

    pci_scan_buses(&scanner);

    if (scanInfo.count > 0) {
        ahci_init(scanInfo.devices[0]);
    }

    *count = scanInfo.count;
}

void DISK_get_info(DiskDevice _device, DiskInfo* info) {
    DiskDevice_impl* device = (DiskDevice_impl*)_device;

    memset(info, 0, sizeof(*info));

    // device->configSpace.
}

void DISK_write(DiskDevice device, u64 offset, void* buffer, u64 size);

void DISK_read(DiskDevice device, u64 offset ,void* buffer, u64 size);

void DISK_flush(DiskDevice device) {
    // @TODO Implement
}

