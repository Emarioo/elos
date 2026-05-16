#pragma once

#include "elos/boot_api.h"

#include "elos/common/types.h"

// structure for revision 0 (version 1.0)
#pragma pack(push, 1)
typedef struct RSDP {
    char Signature[8];
    u8   Checksum;
    char OEMID[6];
    u8   Revision;
    u32  RsdtAddress;
} RSDP;
#pragma pack(pop)

// structure for revision 2 (version 2.0+)
#pragma pack(push, 1)
typedef struct XSDP {
 char Signature[8];
 u8   Checksum;
 char OEMID[6];
 u8   Revision;
 u32  RsdtAddress;      // deprecated since version 2.0

 u32  Length;
 u64  XsdtAddress;
 u8   ExtendedChecksum;
 u8   reserved[3];
} XSDP;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct ACPI_SDTHeader {
  char Signature[4];
  u32  Length;
  u8   Revision;
  u8   Checksum;
  char OEMID[6];
  char OEMTableID[8];
  u32  OEMRevision;
  u32  CreatorID;
  u32  CreatorRevision;
} ACPI_SDTHeader;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
  u32 lapic_address;
  u32 flags;
} MADT_header;
#pragma pack(pop)

#define MADT_ENTRY_LAPIC 0
#define MADT_ENTRY_IOAPIC 1
#define MADT_ENTRY_IOAPIC_INTERRUPT_SRC_OVERRIDE 2
#define MADT_ENTRY_IOAPIC_NON_MASKABLE_INTERRUPT_SRC 3
#define MADT_ENTRY_LAPIC_NON_MASKABLE_INTERRUPT 4
#define MADT_ENTRY_LAPIC_ADDRESS_OVERRIDE 5
#define MADT_ENTRY_LOCAL_X2APIC 9

#pragma pack(push, 1)
typedef struct {
    u8 entryType;
    u8 entryLength;
} MADT_entry_header;
#pragma pack(pop)

#define MADT_entry_header_fields \
    u8 entryType; \
    u8 entryLength;

#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u8  acpiProcessorID;
    u8  apicID;
    u32 flags;
} MADT_lapic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u8  ioapicID;
    u8  reserved;
    u32 ioapicAddress;
    u32 globalSystemInterruptBase;
} MADT_ioapic;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u8  busSource;
    u8  irqSource;
    u32 globalSystemInterrupt;
    u16 flags;
} MADT_ioapic_interrupt_source_override;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u8  nmiSource;
    u8  reserved;
    u16 flags;
    u32 globalSysteminterrupt;
} MADT_ioapic_non_maskable_interrupt_source;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u8  acpiProcessorID; // 0xff means all processors
    u16 flags;
    u8  lint; // 0 or 1
} MADT_lapic_non_maskable_interrupt;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u16 reserved;
    u64 address64;
} MADT_lapic_address_override;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    MADT_entry_header_fields
    u16 reserved;
    u32 x2apicID;
    u32 flags;
    u32 acpiID;
} MADT_local_x2apic;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} GenericAddressStructure;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  Reserved;

    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t BootArchitectureFlags;

    uint8_t  Reserved2;
    uint32_t Flags;

    // 12 byte structure; see below for details
    GenericAddressStructure ResetReg;

    uint8_t  ResetValue;
    uint8_t  Reserved3[3];
  
    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                X_FirmwareControl;
    uint64_t                X_Dsdt;

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} FADT;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    GenericAddressStructure address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} HPET;
#pragma pack(pop)

/*
    These structs are here for reference.
    The bit fields are conceptually correct but does not compile
    to correct layout.

#pragma pack(push, 1)
typedef struct {
    uint8_t  rev_id;
    uint8_t  num_time_cap: 5;
    uint8_t  count_size_cap : 1;
    uint8_t  reserved : 1;
    uint8_t  leg_rt_cap : 1;
    uint16_t vendor_id;
    uint32_t counter_clk_period;
} HPET_Capability_Register;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t  reserved2: 1;
    uint8_t  int_type_cnf: 1;
    uint8_t  int_enb_cnf: 1;
    uint8_t  type_cnf: 1;
    uint8_t  per_int_cap: 1;
    uint8_t  size_cap: 1;
    uint8_t  val_set_cnf: 1;
    uint8_t  reserved1: 1;
    uint8_t  int_route_cnf : 6;
    uint8_t  fsb_en_cnf : 1;
    uint8_t  fsb_int_del_cap : 1;
    uint16_t reserved;
    uint32_t int_route_cap;
} HPET_Timer_Register;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint64_t  enable_cnf: 1;
    uint64_t  leg_rt_cnf: 1;
    uint64_t  reserved: 62;
} HPET_Configuration_Register;
#pragma pack(pop)
*/

typedef struct {
    u64 address; // mapped when parsing ACPI tables
    u64 interruptBaseNumber;
} IOAPIC_Info;

extern u64 acpi_lapic_address;
extern IOAPIC_Info acpi_ioapic_array[4];
extern int acpi_ioapic_array_len;
extern u64 acpi_hpet_address;

void acpi_init(BootAPI* boot_api);


void acpi_system_reset();

