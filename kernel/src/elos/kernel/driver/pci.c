
#include "elos/kernel/driver/pci.h"
#include "elos/kernel/common/string.h"
#include "elos/kernel/log/print.h"
#include "elos/kernel/common/intrinsics.h"
#include "elos/kernel/debug/debug.h"

#include "elos/kernel/driver/pata.h"

typedef enum PCI_ClassCode {
    PCI_CLASSCODE__UNCLASSIFIED = 0x0,
    PCI_CLASSCODE__MASS_STORAGE_CONTROLLER = 0x1,
    PCI_CLASSCODE__NETWORK_CONTROLLER = 0x2,
    PCI_CLASSCODE__DISPLAY_CONTROLLER = 0x3,
    PCI_CLASSCODE__MULTIMEDIA_CONTROLLER = 0x4,
    PCI_CLASSCODE__MEMORY_CONTROLLER = 0x5,
    PCI_CLASSCODE__BRIDGE_CONTROLLER = 0x6,
    PCI_CLASSCODE__SIMPLE_COMMUNICATION_CONTROLLER = 0x7,
    PCI_CLASSCODE__BASE_SYSTEM_PERIPHERAL = 0x8,
    PCI_CLASSCODE__INPUT_DEVICE_CONTROLLER = 0x9,
    PCI_CLASSCODE__DOCKING_STATION = 0xA,
    PCI_CLASSCODE__PROCESSOR = 0xB,
    PCI_CLASSCODE__SERIAL_BUS_CONTROLLER = 0xC,
    PCI_CLASSCODE__WIRELESS_CONTROLLER = 0xD,
    PCI_CLASSCODE__INTELLIGENT_CONTROLLER = 0xE,
    PCI_CLASSCODE__SATELLITE_COMMUNICATION_CONTROLLER = 0xF,
    PCI_CLASSCODE__ENCRYPTION_CONTROLLER = 0x10,
    PCI_CLASSCODE__SIGNAL_PROCESSING_CONTROLLER = 0x11,
    PCI_CLASSCODE__PROCESSING_ACCELERATOR = 0x12,
    PCI_CLASSCODE__NON_ESSENTIAL_INSTRUMENTATION = 0x13,
    // reserved 0x14 - 0x3f
    PCI_CLASSCODE__CO_PROCESSOR = 0x40,
    // reserved 0x41 - 0xFE
    PCI_CLASSCODE__UNASSIGNED_CLASS = 0xFF, // vendor specific
} PCI_ClassCode;


typedef enum PCI_Subclass {
    PCI_SUBCLASS__OTHER = 0x80,

    // PCI_CLASSCODE__UNCLASSIFIED
    PCI_SUBCLASS__NON_VGA_COMPATIBLE_UNCLASSIFIED_DEVICE = 0x0,
    PCI_SUBCLASS__VGA_COMPATIBLE_UNCLASSIFIED_DEVICE = 0x1,
    
    // PCI_CLASSCODE__MASS_STORAGE_CONTROLLER
    PCI_SUBCLASS__SCSI_BUS_CONTROLLER = 0x0,
    PCI_SUBCLASS__IDE_CONTROLLER = 0x1,
    PCI_SUBCLASS__FLOPPY_DISK_CONTROLLER = 0x2,
    PCI_SUBCLASS__IPI_BUS_CONTROLLER = 0x3,
    PCI_SUBCLASS__RAID_CONTROLLER = 0x4,
    PCI_SUBCLASS__ATA_CONTROLLER = 0x5,
    PCI_SUBCLASS__SERIAL_ATA_CONTROLLER = 0x6,
    PCI_SUBCLASS__SERIAL_ATTACHED_SCSI_CONTROLLER = 0x7,
    PCI_SUBCLASS__NON_VOLATILE_MEMORY_CONTROLLER = 0x8,

    // PCI_CLASSCODE__NETWORK_CONTROLLER
    PCI_SUBCLASS__ETHERNET_CONTROLLER = 0x0,
    PCI_SUBCLASS__TOKEN_RING_CONTROLLER = 0x1,
    PCI_SUBCLASS__FDDI_CONTROLLER = 0x2,
    PCI_SUBCLASS__ATM_CONTROLLER = 0x3,
    PCI_SUBCLASS__ISDN_CONTROLLER = 0x4,
    PCI_SUBCLASS__WORLD_FIP_CONTROLLER = 0x5,
    PCI_SUBCLASS__PICMG_CONTROLLER = 0x6,
    PCI_SUBCLASS__NETWORK_INFINIBAND_CONTROLLER = 0x7,
    PCI_SUBCLASS__FABRIC_CONTROLLER = 0x8,

    // PCI_CLASSCODE__DISPLAY_CONTROLLER
    PCI_SUBCLASS__VGA_CONTROLLER = 0x0,
    PCI_SUBCLASS__XGA_CONTROLLER = 0x1,
    PCI_SUBCLASS__3D_CONTROLLER = 0x2,

    // PCI_CLASSCODE__MULTIMEDIA_CONTROLLER
    PCI_SUBCLASS__VIDEO_CONTROLLER = 0x0,
    PCI_SUBCLASS__AUDIO_CONTROLLER = 0x1,
    PCI_SUBCLASS__TELEPHONY_DEVICE = 0x2,
    PCI_SUBCLASS__AUDIO_DEVICE     = 0x3,

    // PCI_CLASSCODE__MEMORY_CONTROLLER
    PCI_SUBCLASS__RAM_CONTROLLER = 0x0,
    PCI_SUBCLASS__FLASH_CONTROLLER = 0x1,

    // PCI_CLASSCODE__BRIDGE_CONTROLLER
    PCI_SUBCLASS__HOST_BRIDGE = 0x0,
    PCI_SUBCLASS__ISA_BRIDGE = 0x1,
    PCI_SUBCLASS__EISA_BRIDGE = 0x2,
    PCI_SUBCLASS__MCA_BRIDGE = 0x3,
    PCI_SUBCLASS__PCI_TO_PCI_BRIDGE = 0x4,
    PCI_SUBCLASS__PCMCIA_BRIDGE = 0x5,
    PCI_SUBCLASS__NUBUS_BRIDGE = 0x6,
    PCI_SUBCLASS__CARDBUS_BRIDGE = 0x7,
    PCI_SUBCLASS__RACEWAY_BRIDGE = 0x8,
    PCI_SUBCLASS__PCI_TO_PCI_SEMI_TRANSPARENT_BRIDGE = 0x9,
    PCI_SUBCLASS__INFINIBAND_TO_PCI_BRIDGE = 0xA,

    // PCI_CLASSCODE__SIMPLE_COMMUNICATION_CONTROLLER
    PCI_SUBCLASS__SERIAL_CONTROLLER = 0x0,
    PCI_SUBCLASS__PARALLEL_CONTROLLER = 0x1,
    PCI_SUBCLASS__MULTIPORT_SERIAL_CONTROLLER = 0x2,
    PCI_SUBCLASS__MODEM = 0x3,
    PCI_SUBCLASS__IEEE_488_GPIB_CONTROLLER = 0x4,
    PCI_SUBCLASS__SMART_CARD_CONTROLLER = 0x5,

    // PCI_CLASSCODE__BASE_SYSTEM_PERIPHERAL
    PCI_SUBCLASS__PIC = 0x0,
    PCI_SUBCLASS__DMA_CONTROLLER = 0x1,
    PCI_SUBCLASS__TIMER = 0x2,
    PCI_SUBCLASS__RTC_CONTROLLER = 0x3,
    PCI_SUBCLASS__PCI_HOT_PLUG_CONTROLLER = 0x4,
    PCI_SUBCLASS__SD_HOST_CONTROLLER = 0x5,
    PCI_SUBCLASS__IOMMU = 0x6,

    // PCI_CLASSCODE__INPUT_DEVICE_CONTROLLER
    PCI_SUBCLASS__KEYBOARD_CONTROLLER = 0x0,
    PCI_SUBCLASS__PEN_CONTROLLER = 0x1,
    PCI_SUBCLASS__MOUSE_CONTROLLER = 0x2,
    PCI_SUBCLASS__SCANNER_CONTROLLER = 0x3,
    PCI_SUBCLASS__GAMEPORT_CONTROLLER = 0x4,

    // PCI_CLASSCODE__DOCKING_STATION
    PCI_SUBCLASS__GENERIC_DOCKING_STATION = 0x0,

    // PCI_CLASSCODE__PROCESSOR
    PCI_SUBCLASS__386 = 0x0,
    PCI_SUBCLASS__486 = 0x1,
    PCI_SUBCLASS__PENTIUM = 0x2,
    PCI_SUBCLASS__PENTIUM_PRO = 0x3,
    PCI_SUBCLASS__ALPHA = 0x10,
    PCI_SUBCLASS__POWERPC = 0x20,
    PCI_SUBCLASS__MIPS = 0x30,
    PCI_SUBCLASS__CO_PROCESSOR = 0x40,

    // PCI_CLASSCODE__SERIAL_BUS_CONTROLLER
    PCI_SUBCLASS__FIREWIRE_CONTROLLER = 0x0,
    PCI_SUBCLASS__ACCESS_BUS_CONTROLLER = 0x1,
    PCI_SUBCLASS__SSA = 0x2,
    PCI_SUBCLASS__USB_CONTROLLER = 0x3,
    PCI_SUBCLASS__FIBRE_CHANNEL = 0x4,
    PCI_SUBCLASS__SMBUS_CONTROLLER = 0x5,
    PCI_SUBCLASS__SERIAL_BUS_INFINIBAND_CONTROLLER = 0x6,
    PCI_SUBCLASS__IPMI_INTERFACE = 0x7,
    PCI_SUBCLASS__SERCOS_INTERFACE = 0x8,
    PCI_SUBCLASS__CANBUS_CONTROLLER = 0x9,

    // PCI_CLASSCODE__WIRELESS_CONTROLLER
    PCI_SUBCLASS__IRDA_CONTROLLER = 0x0,
    PCI_SUBCLASS__CONSUMER_IR_CONTROLLER = 0x1,
    PCI_SUBCLASS__RF_CONTROLLER = 0x10,
    PCI_SUBCLASS__BLUETOOTH_CONTROLLER = 0x11,
    PCI_SUBCLASS__BROADBAND_CONTROLLER = 0x12,
    PCI_SUBCLASS__ETHERNET_CONTROLLER_802_1A = 0x20,
    PCI_SUBCLASS__ETHERNET_CONTROLLER_802_1B = 0x21,

    // PCI_CLASSCODE__INTELLIGENT_CONTROLLER
    PCI_SUBCLASS__I2O = 0x0,

    // PCI_CLASSCODE__SATELLITE_COMMUNICATION_CONTROLLER
    PCI_SUBCLASS__SATELLITE_TV_CONTROLLER = 0x0,
    PCI_SUBCLASS__SATELLITE_AUDIO_CONTROLLER = 0x1,
    PCI_SUBCLASS__SATELLITE_VOICE_CONTROLLER = 0x2,
    PCI_SUBCLASS__SATELLITE_DATA_CONTROLLER = 0x3,

    // PCI_CLASSCODE__ENCRYPTION_CONTROLLER
    PCI_SUBCLASS__NETWORK_AND_COMPUTING_ENCRYPTION = 0x0,
    PCI_SUBCLASS__ENTERTAINMENT_ENCRYPTION = 0x10,

    // PCI_CLASSCODE__SIGNAL_PROCESSING_CONTROLLER
    PCI_SUBCLASS__DPIO_MODULES = 0x0,
    PCI_SUBCLASS__PERFORMANCE_COUNTERS = 0x1,
    PCI_SUBCLASS__COMMUNICATION_SYNCHRONIZER = 0x10,
    PCI_SUBCLASS__SIGNAL_PROCESSING_MANAGEMENT = 0x20,
} PCI_Subclass;

// TODO: prog IF

typedef struct PCI_ConfigSpace {
    u16 vendorID;
    u16 deviceID;
    struct {
        u16  io_space                           : 1;
        u16  memory_space                       : 1;
        u16  bus_master                         : 1;
        u16  special_cycles                     : 1;
        u16  memory_write_and_invalidate_enable : 1;
        u16  vga_palette_snoop                  : 1;
        u16  parity_error_response              : 1;
        u16  _reserved0                         : 1;
        u16  serr_enable                        : 1;
        u16  fast_back_to_back_enable           : 1;
        u16  interupt_disable                   : 1;
    } command;
    struct {
        u16 _reserved0                : 1;
        u16 interrupt_status          : 1;
        u16 capabilities_list         : 1;
        u16 mhz66_capable             : 1;
        u16 _reserved1                : 1;
        u16 fast_back_to_back_capable : 1;
        u16 master_data_parity_error  : 1;
        u16 devsel_timing             : 1;
        u16 signaled_target_abort     : 1;
        u16 received_target_abort     : 1;
        u16 received_master_abort     : 1;
        u16 signaled_system_error     : 1;
        u16 detected_parity_error     : 1;
    } status;
    u8  revisionID;
    u8  progIF;
    u8  subclass;
    u8  classCode;
    u8  cacheLineSize;
    u8  latencyTimer;
    u8  headerType;
    struct {
        u8 completion_code : 4;
        u8 _reserved0      : 2;
        u8 start_bist      : 1;
        u8 bist_capable    : 1;
    } bist;

    // Header type specific
    union {

        // Header Type 0x0
        struct {
            u32 bar0;
            u32 bar1;
            u32 bar2;
            u32 bar3;
            u32 bar4;
            u32 bar5;
            u32 cardbus_cis_pointer;
            u16 subsystem_vendor_id;
            u16 subsystem_id;
            u32 expansion_rom_base_address;
            u8  capabilities_pointer;
            u8  _reserved0[3];
            u32 _reserved1;
            u8  interrupt_line;
            u8  interrupt_pin;
            u8  min_grant;
            u8  max_latency;
        } header0;
        
        // Header Type 0x1 (PCI-to-PCI bridge)
        struct {
            u32 bar0;
            u32 bar1;
            u8  primary_bus_number;
            u8  secondary_bus_number;
            u8  subordinate_bus_number;
            u8  secondary_latency_timer;
            u8  io_base;
            u8  io_limit;
            u16 secondary_status;
            u16 memory_base;
            u16 memory_limit;
            u16 prefetchable_memory_base;
            u16 prefetchable_memory_limit;
            u32 prefetchable_base_upper_32_bits;
            u32 prefetchable_limit_upper_32_bits;
            u16 io_base_upper_16_bits;
            u16 io_limit_upper_16_bits;
            u8  capabilities_pointer;
            u8  _reserved0[3];
            u32 expansion_rom_base_address;
            u16 bridge_control;
            u8  interrupt_pin;
            u8  interrupt_line;
        } header1;

          // Header Type 0x2 (PCI-to-CardBus bridge)
        //   struct {
        //     u32 cardbus_socket_exca_base_address;
        // } header2;
    };
} PCI_ConfigSpace;

static u16 pciConfig_readw(u8 bus, u8 slot, u8 func, u8 offset) {
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

static u32 pciConfig_readl(u8 bus, u8 slot, u8 func, u8 offset) {
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

void pci_read_config_space(PCI_ConfigSpace* config, u8 bus, u8 slot, u8 function) {
    u32* dwords = (u32*)config;

    // Read first 4 DWORDs, they are normal and common
    for (int i = 0; i < sizeof(PCI_ConfigSpace)/sizeof(u32); i++)
        dwords[i] = pciConfig_readl(bus, slot, function, i * sizeof(u32));
}

void trace_config_space(PCI_ConfigSpace* config) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "Device %d, Vendor %d, Class %d, Subclass %d, ProgIF %d\n", config->deviceID, config->vendorID, config->classCode, config->subclass, config->progIF);
    printf("%s", buffer);
    serial_printf("%s", buffer);
}

u8 pci_readHeaderType(int bus, int device, int function) {
    return 0xFF & pciConfig_readw(bus, device, function, 14);
}
u16 pci_readVendorID(int bus, int device, int function) {
    return pciConfig_readw(bus, device, function, 2);
}

void pci_scan_bus(int bus);

void pci_scan_function(int bus, int device, int function) {
    PCI_ConfigSpace config = {};
    pci_read_config_space(&config, bus, device, function);
    trace_config_space(&config);

    if ((config.headerType & 0x7F) == 1 && config.classCode == PCI_CLASSCODE__BRIDGE_CONTROLLER && config.subclass == PCI_SUBCLASS__PCI_TO_PCI_BRIDGE) {
        pci_scan_bus(config.header1.secondary_bus_number);
    } else {
        switch (config.classCode) {
            case PCI_CLASSCODE__MASS_STORAGE_CONTROLLER: {
                if (config.subclass == PCI_SUBCLASS__IDE_CONTROLLER) {
                    // for each drive and bus we create a device if we can communicate with it

                    // If we did a scan previously then we want to update the devices we already made instead of
                    // overwriting or creating new ones.
                }
            } break;
            default: {

            } break;
        }
    }
}

void pci_scan_device(int bus, int device) {
    int function = 0;
    int vendor, headerType;

    vendor = pciConfig_readw(bus, device, function, 2);
    if (vendor == 0xFFFF)
        return;
    
    pci_scan_function(bus, device, function);
    
    headerType = 0xFF & pciConfig_readw(bus, device, function, 14);
    
    // Device has multiple functions when bit 7 is set
    if((headerType & 0x80) == 0)
        return;
    
    for (function = 1; function < 8; function++) {
        vendor = pciConfig_readw(bus, device, function, 2);
        if (vendor == 0xFFFF)
            continue;
        
        pci_scan_function(bus, device, function);
    }
}

void pci_scan_bus(int bus) {
    for (int dev=0;dev<32;dev++) {
        pci_scan_device(bus, dev);
    }
}

void pci_scan_buses() {

    int header = pci_readHeaderType(0, 0, 0);
    if ((header & 80) == 0) {
        pci_scan_bus(0);
    } else {
        for (int function=0;function<8;function++) {
            int vendor = pci_readVendorID(0, 0, function);
            if (vendor == 0xFFFF) continue;
            pci_scan_bus(function);
        }
    }
}
