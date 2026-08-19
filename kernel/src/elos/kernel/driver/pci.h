
#pragma once

#include "elos/common/types.h"

//##########################
//       TYPES
//##########################



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
        u16  interrupt_disable                   : 1;
    } command;
    struct {
        u16 _reserved0                : 3;
        u16 interrupt_status          : 1;
        u16 capabilities_list         : 1;
        u16 mhz66_capable             : 1;
        u16 _reserved1                : 1;
        u16 fast_back_to_back_capable : 1;
        u16 master_data_parity_error  : 1;
        u16 devsel_timing             : 2;
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

    int pci_bus;
    int pci_device;
    int pci_function;
} PCI_ConfigSpace;

// @IMPORTANT DO not sizeof(PCI_ConfigSpace) and read a bunch of words from PCI port.
//   PCI Config space contains some extra fields (bus, device, function)
#define CONFIG_SPACE_SIZE 64

typedef struct PCI_Scanner PCI_Scanner;

typedef bool(*FN_foreach_device)(PCI_Scanner* scanner, PCI_ConfigSpace* config);

struct PCI_Scanner {
    FN_foreach_device func;
    void* user_data;
};






//###########################
//        FUNCTIONS
//###########################


void pci_scan_buses(PCI_Scanner* scanner);

u16 pciConfig_readw(u8 bus, u8 slot, u8 func, u8 offset);
u32 pciConfig_readl(u8 bus, u8 slot, u8 func, u8 offset);
void pciConfig_writew(u8 bus, u8 slot, u8 func, u8 offset, u16 data);
void pciConfig_writel(u8 bus, u8 slot, u8 func, u8 offset, u32 data);


u16 pci_config_readw(PCI_ConfigSpace* config, u8 offset);
u32 pci_config_readl(PCI_ConfigSpace* config, u8 offset);
void pci_config_writew(PCI_ConfigSpace* config, u8 offset, u16 data);
void pci_config_writel(PCI_ConfigSpace* config, u8 offset, u32 data);


void decode_bar_size(PCI_ConfigSpace* config, int bar_index, u64* bar_size);
void decode_bar(PCI_ConfigSpace* config, u64* first_ioaddr, u64* out_first_ioaddr_size, u64* first_maddr, u64* out_first_maddr_size);
