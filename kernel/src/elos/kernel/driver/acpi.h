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



void acpi_init(BootAPI* boot_api);
