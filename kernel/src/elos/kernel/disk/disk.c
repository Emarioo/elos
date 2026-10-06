#include "elos/disk.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel_console.h"

#include "elos/kernel/driver/pci.h"
#include "elos/kernel/driver/pci_list.h"

#include "elos/kernel/disk/disk_private.h"
#include "elos/kernel/disk/ahci.h"
#include "elos/kernel/disk/nvme.h"


#define printf(...) KCON_printf(__VA_ARGS__)


DiskDevice g_diskDevices[MAX_DISK_DEVICES];

static Disk_RAM_Context g_initrd;


void DISK_init(BootAPI* boot_api) {
    g_initrd.data = PMEM_phys_to_kernel(boot_api->initrd_data);
    g_initrd.size = boot_api->initrd_size;
}


bool disk_find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    if (config->classCode != PCI_CLASSCODE__MASS_STORAGE_CONTROLLER) {
        return false;
    }
    Disk_ScanInfo* scanInfo = (Disk_ScanInfo*)scanner->user_data;
    
    // printf("Vendor=%x Device=%x Subclass=%d progif=%d\n", config->vendorID, config->deviceID, config->subclass, config->progIF);

    if (config->subclass == PCI_SUBCLASS__SERIAL_ATA_CONTROLLER) {
        return ahci_pci_scan(scanInfo, config);
    } else if (config->subclass == PCI_SUBCLASS__NON_VOLATILE_MEMORY_CONTROLLER) {
        return nvme_pci_scan(scanInfo, config);
    } else {
        return false;
    }

    // False because we want to keep searching.
    return false;
}
  
DiskDevice* disk_reserve_device() {
    DiskDevice* device = NULL;
    for (int i=0;i<MAX_DISK_DEVICES;i++) {
        if (g_diskDevices[i].type == DISK_TYPE_NONE) {
            device = &g_diskDevices[i];
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

void DISK_scan_devices(DiskDevice** devices, int* count) {
    Disk_ScanInfo scanInfo = {
        .devices = devices,
        .maxCount = *count,
        .count = 0,
    };
    PCI_Scanner scanner = { .func = disk_find_device };
    scanner.user_data = (void*)&scanInfo;

    if (g_initrd.data) {
        DiskDevice* dev = disk_reserve_device();

        dev->type = DISK_TYPE_RAM;
        dev->userData = &g_initrd;
        dev->diskInfo.name_len = snprintf(dev->diskInfo.name, sizeof(dev->diskInfo.name), "initrd");
        dev->diskInfo.diskSize = g_initrd.size;
        dev->diskInfo.sectorSize = 512; // Hardcoding this might cause problems.

        scanInfo.devices[scanInfo.count] = dev;
        scanInfo.count++;
    }

    pci_scan_buses(&scanner);

    *count = scanInfo.count;
}


void DISK_enumerate(u64* cookie, u64* entryCount, ELOS_DiskEntry* buffer) {
    u64 maxCount = *entryCount;
    u32 index = *cookie;
    u32 retrievedEntries = 0;

    // @TODO Lock g_diskDevices array.

    while (index < MAX_DISK_DEVICES && retrievedEntries < maxCount) {
        u32 entryIndex = index;
        index++;
        DiskDevice* device = &g_diskDevices[entryIndex];
        if (device->type == DISK_TYPE_NONE) {
            continue;
        }

        ELOS_DiskEntry* entry = &buffer[retrievedEntries];
        retrievedEntries++;

        entry->diskID = entryIndex;
        entry->diskSize = device->diskInfo.diskSize;
        entry->sectorSize = device->diskInfo.sectorSize;

        u32 name_len = strlen(device->diskInfo.name);
        if (name_len > sizeof(entry->name)-1) {
            entry->name_len = sizeof(entry->name)-1;
        } else {
            entry->name_len = name_len;
        }
        memcpy(entry->name, device->diskInfo.name, name_len);
        entry->name[name_len] = '\0';
    }

    *cookie = index;
    *entryCount = retrievedEntries;
}

void DISK_get_info(DiskDevice* device, ELOS_DiskInfo* info) {
    memcpy(info, &device->diskInfo, sizeof(*info));
}

ELOS_Error DISK_write(DiskDevice* device, u64 offset, u64 size, void* buffer) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;

    switch (device->type) {
        case DISK_TYPE_RAM: {
            Disk_RAM_Context* ram = (Disk_RAM_Context*)device->userData;
            if (size == 0) {
                returnValue = ELOS_ERR_INVALID_PARAM;
            } else if (size > ram->size) {
                returnValue = ELOS_ERR_OUT_OF_BOUNDS;
            } else if (offset > ram->size - size) {
                returnValue = ELOS_ERR_OUT_OF_BOUNDS;
                printf("DISK_write: out of bounds read on (0x%zx:0x%zx, 0x%zx) from %s\n", offset, size, ram->size, device->diskInfo.name);
            } else {
                memcpy(ram->data + offset, buffer, size);
                returnValue = ELOS_OK;
            }
        } break;
        case DISK_TYPE_SATA: {
            returnValue = ahci_write(device, offset, size, buffer);
            if (returnValue != ELOS_OK) {
                printf("DISK_write: Could not write (0x%zx, %d) from %s\n", offset, size, device->diskInfo.name);
            }
        } break;
        case DISK_TYPE_NVME: {
            returnValue = nvme_write(device, offset, size, buffer);
            if (returnValue != ELOS_OK) {
                printf("DISK_write: Could not write (0x%zx, %d) from %s\n", offset, size, device->diskInfo.name);
            }
        } break;
        default: {
            printf("DISK_write: type %d not implemented\n", device->type);
        } break;
    }

    return returnValue;
}

ELOS_Error DISK_read(DiskDevice* device, u64 offset, u64 size, void* buffer) {
    ELOS_Error returnValue = ELOS_ERR_UNKNOWN;

    switch (device->type) {
        case DISK_TYPE_RAM: {
            Disk_RAM_Context* ram = (Disk_RAM_Context*)device->userData;
            if (size == 0) {
                returnValue = ELOS_ERR_INVALID_PARAM;
            } else if (size > ram->size) {
                returnValue = ELOS_ERR_OUT_OF_BOUNDS;
            } else if (offset > ram->size - size) {
                returnValue = ELOS_ERR_OUT_OF_BOUNDS;
                printf("DISK_read: out of bounds read on (off=0x%zx sz=0x%zx diskSize=0x%zx) from %s\n", offset, size, ram->size, device->diskInfo.name);
            } else {
                memcpy(buffer, (char*)ram->data + offset, size);
                returnValue = ELOS_OK;
            }
        } break;
        case DISK_TYPE_SATA: {
            returnValue = ahci_read(device, offset, size, buffer);
            if (returnValue != ELOS_OK) {
                printf("DISK_read: Could not read (0x%zx, %d) from %s\n", offset, size, device->diskInfo.name);
            }
        } break;
        case DISK_TYPE_NVME: {
            returnValue = nvme_read(device, offset, size, buffer);
            if (returnValue != ELOS_OK) {
                printf("DISK_read: Could not read (0x%zx, %d) from %s\n", offset, size, device->diskInfo.name);
            }
        } break;
        default: {
            printf("DISK_read: type %d not implemented\n", device->type);
        } break;
    }

    return returnValue;
}
