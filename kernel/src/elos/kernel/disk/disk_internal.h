#pragma once

#include "elos/disk.h"

#include "elos/kernel/driver/pci.h"

typedef struct {
    bool used;

    PCI_ConfigSpace configSpace;
    // u16 pci_vendorID;
    // u16 pci_deviceID;
    // int pci_bus;
    // int pci_device;
    // int pci_function;
    DiskInfo diskInfo;

} DiskDevice_impl;

