#pragma once

#include "elos/common/types.h"

#define MBR_BOOT_SIGNATURE (0xAA55)

#pragma pack(push, 1)
typedef struct mbr__PartitionRecord {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_lba;
} mbr__PartitionRecord;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct mbr__Header {
    u8 bootCode[440];
    u8 diskSignature[4];
    u8 zero[2];
    mbr__PartitionRecord partitionRecords[4];
    u16 bootSignature; // 0xAA55
} mbr__Header;
#pragma pack(pop)

