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

void acpi_init(BootAPI* boot_api);
