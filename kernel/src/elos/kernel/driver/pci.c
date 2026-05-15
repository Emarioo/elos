
#include "elos/kernel/driver/pci.h"
#include "elos/common/string.h"
#include "elos/kernel_console.h"
#include "elos/common/intrinsics.h"

#include "elos/kernel/driver/pata.h"



#define printf(...) KCON_printf(__VA_ARGS__)


u16 pciConfig_readw(u8 bus, u8 slot, u8 func, u8 offset) {
    // TODO: Handle errors?
    if (slot >= 1<<6)
        kernel_bug();
    if (slot >= 1<<6)
        kernel_bug();
    if ((offset & 1) != 0)
        kernel_bug();
    u32 address = (1 << 31) // enable bit
        | ((u32) bus << 16)
        | ((u32) slot << 11)
        | ((u32) func << 8)
        | ((u32) offset & 0xFC); // low 2 bits should be zero for DWORD alignment

    // CONFIG_ADDRESS
    outl(0xCF8, address);

    // CONFIG_DATA
    u32 value = inl(0xCFC);

    return (value >> ((offset&2) * 8)) & 0xFFFF;
}

u32 pciConfig_readl(u8 bus, u8 slot, u8 func, u8 offset) {
    // TODO: Handle errors?
    if (slot >= (1<<5))
        kernel_bug();
    if (func >= (1<<3))
        kernel_bug();
    if ((offset & 3) != 0)
        kernel_bug();
    u32 address = (1 << 31) // enable bit
        | ((u32) bus << 16)
        | ((u32) slot << 11)
        | ((u32) func << 8)
        | ((u32) offset & 0xFC); // low 2 bits should be zero for DWORD alignment

    // CONFIG_ADDRESS
    outl(0xCF8, address);

    // CONFIG_DATA
    u32 value = inl(0xCFC);
    return value;
}


void pciConfig_writew(u8 bus, u8 slot, u8 func, u8 offset, u16 data) {
    // TODO: Handle errors?
    if (slot >= 1<<6)
        kernel_bug();
    if (slot >= 1<<6)
        kernel_bug();
    if ((offset & 1) != 0)
        kernel_bug();
    u32 address = (1 << 31) // enable bit
        | ((u32) bus << 16)
        | ((u32) slot << 11)
        | ((u32) func << 8)
        | ((u32) offset & 0xFC); // low 2 bits should be zero for DWORD alignment

    // CONFIG_ADDRESS
    outl(0xCF8, address);

    // CONFIG_DATA
    u32 value = inl(0xCFC);
    if (offset & 2) {
        value &= 0xFFFF;
        value |= (u32)data << 16;
    } else {
        value &= 0xFFFF0000;
        value |= (u32)data;
    }
    outl(0xCFC, value);
}

void pciConfig_writel(u8 bus, u8 slot, u8 func, u8 offset, u32 data) {
    // TODO: Handle errors?
    if (slot >= (1<<5))
        kernel_bug();
    if (func >= (1<<3))
        kernel_bug();
    if ((offset & 3) != 0)
        kernel_bug();
    u32 address = (1 << 31) // enable bit
        | ((u32) bus << 16)
        | ((u32) slot << 11)
        | ((u32) func << 8)
        | ((u32) offset & 0xFC); // low 2 bits should be zero for DWORD alignment

    // CONFIG_ADDRESS
    outl(0xCF8, address);

    // CONFIG_DATA
    outl(0xCFC, data);
}

void pci_read_config_space(PCI_ConfigSpace* config, u8 bus, u8 slot, u8 function) {
    u32* dwords = (u32*)config;

    // Read first 4 DWORDs, they are normal and common
    for (int i = 0; i < CONFIG_SPACE_SIZE/sizeof(u32); i++)
        dwords[i] = pciConfig_readl(bus, slot, function, i * sizeof(u32));
}


u8 pci_readHeaderType(int bus, int device, int function) {
    return 0xFF & pciConfig_readw(bus, device, function, 14);
}
u16 pci_readVendorID(int bus, int device, int function) {
    return pciConfig_readw(bus, device, function, 2);
}

bool pci_scan_bus(PCI_Scanner* scanner, int bus);
bool pci_scan_device(PCI_Scanner* scanner, int bus, int device);
bool pci_scan_function(PCI_Scanner* scanner, int bus, int device, int function);

void trace_config_space(PCI_ConfigSpace* config) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "Device %d, Vendor %d, Class %d, Subclass %d, ProgIF %d\n", config->deviceID, config->vendorID, config->classCode, config->subclass, config->progIF);
    printf("%s", buffer);
}

bool pci_scan_function(PCI_Scanner* scanner, int bus, int device, int function) {
    PCI_ConfigSpace config = {0};
    config.pci_bus = bus;
    config.pci_device = device;
    config.pci_function = function;
    pci_read_config_space(&config, bus, device, function);

    // trace_config_space(&config);

    if ((config.headerType & 0x7F) == 1 && config.classCode == PCI_CLASSCODE__BRIDGE_CONTROLLER && config.subclass == PCI_SUBCLASS__PCI_TO_PCI_BRIDGE) {
        pci_scan_bus(scanner, config.header1.secondary_bus_number);
    } else {
        if (scanner->func) {
            bool finished = scanner->func(scanner, &config);
            if (finished) {
                return true;
            }
        }

        // switch (config.classCode) {
        //     case PCI_CLASSCODE__MASS_STORAGE_CONTROLLER: {
        //         if (config.subclass == PCI_SUBCLASS__IDE_CONTROLLER) {
        //             // for each drive and bus we create a device if we can communicate with it

        //             // If we did a scan previously then we want to update the devices we already made instead of
        //             // overwriting or creating new ones.
        //         }
        //     } break;
        //     default: {

        //     } break;
        // }
    }
    return false;
}

bool pci_scan_device(PCI_Scanner* scanner, int bus, int device) {
    int function = 0;
    int vendor, headerType;

    vendor = pciConfig_readw(bus, device, function, 2);
    if (vendor == 0xFFFF)
        return false;
    
    bool finished = pci_scan_function(scanner, bus, device, function);
    if (finished)
        return true;
    
    headerType = 0xFF & pciConfig_readw(bus, device, function, 14);
    
    // Device has multiple functions when bit 7 is set
    if((headerType & 0x80) == 0)
        return false;
    
    for (function = 1; function < 8; function++) {
        vendor = pciConfig_readw(bus, device, function, 2);
        if (vendor == 0xFFFF)
            continue;
        
        bool finished = pci_scan_function(scanner, bus, device, function);
        if (finished)
            return true;
    }
    return false;
}

bool pci_scan_bus(PCI_Scanner* scanner, int bus) {
    for (int dev=0;dev<32;dev++) {
        bool finished = pci_scan_device(scanner, bus, dev);
        if (finished)
            return true;
    }
    return false;
}

void pci_scan_buses(PCI_Scanner* scanner) {
    int header = pci_readHeaderType(0, 0, 0);
    if ((header & 80) == 0) {
        pci_scan_bus(scanner, 0);
    } else {
        for (int function=0;function<8;function++) {
            int vendor = pci_readVendorID(0, 0, function);
            if (vendor == 0xFFFF) continue;
            pci_scan_bus(scanner, function);
        }
    }
}


void decode_bar_size(PCI_ConfigSpace* config, int bar_index, u64* out_bar_size) {
    // There text at https://wiki.osdev.org/PCI "Address and size of the BAR" which says:
    // "Before attempting to read the information about the BAR, make sure to disable both I/O and memory decode in the command byte"
    // I am not doiung this which might be a problem.

    int pci_bus = config->pci_bus;
    int pci_device = config->pci_device;
    int pci_function = config->pci_function;

    // Disable memory + IO decode
    u16 cmd = pciConfig_readw(pci_bus, pci_device, pci_function, 0x6);
    pciConfig_writew(pci_bus, pci_device, pci_function, 0x6, cmd & ~0x3);

    int bar_offset = 0x10 + bar_index * 4;

    // @TODO This may not work for 64-bit BARS. Doing two separate writes of 1s 
    //   to lower and upper bar seems strange though?

    // Get current bar
    int prev_bar = pciConfig_readl(pci_bus, pci_device, pci_function, bar_offset);

    // Write ones to bar to tell PCI to give back bar size.
    pciConfig_writel(pci_bus, pci_device, pci_function, bar_offset, 0xFFFFFFFF);
    
    // Retrieve and decode size
    int bar_size = pciConfig_readl(pci_bus, pci_device, pci_function, bar_offset);
    bar_size = ~bar_size + 1;

    // Restore bar
    pciConfig_writel(pci_bus, pci_device, pci_function, bar_offset, prev_bar);

    // Enable memory + IO decode
    cmd = pciConfig_readw(pci_bus, pci_device, pci_function, 0x6);
    pciConfig_writew(pci_bus, pci_device, pci_function, 0x6, cmd | 0x3);

    *out_bar_size = bar_size;
}
void decode_bar(PCI_ConfigSpace* config, u64* first_ioaddr, u64* out_first_ioaddr_size, u64* first_maddr, u64* out_first_maddr_size) {
    
    *first_ioaddr = 0;
    *out_first_ioaddr_size = 0;
    *first_maddr = 0;
    *out_first_maddr_size = 0;

    u32* bars = &config->header0.bar0;

    // #define debug(...) printf(__VA_ARGS__)
    #define debug(...) 

    int head = 0;
    while (head < 6) {
        u32 bar = bars[head];
        head++;
        u64 bar_size = 0;
        decode_bar_size(config, head-1, &bar_size);

        if (bar & 0x1) {
            u32 addr = bar & ~0x3;
            debug("[INFO] bar[%d] IO-mapped addr=%x size=%d KB\n", head, addr, bar_size/1024);
            if (*first_ioaddr == 0) {
                *first_ioaddr = addr;
                *out_first_ioaddr_size = bar_size;
            }
        } else if (((bar >> 1) & 0x6) == 0) {
            u32 addr = bar & ~0xf;
            if (bar & 0x8) {
                debug("[INFO] bar[%d] 32-bit prefetchable addr=%x size=%d KB\n", head, addr, bar_size/1024);
            } else {
                debug("[INFO] bar[%d] 32-bit addr=%x size=%d KB\n", head,addr, bar_size/1024);
            }
            if (*first_maddr == 0) {
                *first_maddr = addr;
                *out_first_maddr_size = bar_size;
            }
        } else if (((bar >> 1) & 0x3) == 2) {
            u32 bar_ext = bars[head];
            u64 addr = ((u64)bar_ext << 32) | ((u64)bar & ~0xfLLU);
            head++;
            if (bar & 0x8) {
                debug("[INFO] bar[%d] 64-bit prefetchable addr=%x\n", head, addr, bar_size/1024);
            } else {
                debug("[INFO] bar[%d] 64-bit addr=%x size=%d KB\n", head, addr, bar_size/1024);
            }
            if (*first_maddr == 0) {
                *first_maddr = addr;
                *out_first_maddr_size = bar_size;
            }
        }
    }
}