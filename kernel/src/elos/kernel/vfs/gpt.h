#pragma once

#include "elos/common/types.h"

#include "elos/disk.h"

typedef struct gpt__GUID {
    uint32_t data0;
    uint16_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[6];
} gpt__GUID;



typedef struct gpt__Header {
    char      signature[8];
    uint32_t  revision;
    uint32_t  header_size;
    uint32_t  header_crc32;
    uint32_t  reserved0;
    uint64_t  current_lba;
    uint64_t  backup_lba;
    uint64_t  first_lba; // for partitions
    uint64_t  last_lba;
    gpt__GUID disc_guid;
    uint64_t  start_lba_entries;
    uint32_t  num_entries;
    uint32_t  entry_size;
    uint32_t  entries_crc32;
} gpt__Header;




typedef enum gpt__PartitionFlags {
    gpt_FLAG_REQUIRED_PARTITION   = 0x1, // Can indicate important boot/recovery partition
    gpt_FLAG_NO_BLOCK_IO_PROTOCOL = 0x2, // EFI firmware should ignore this partition
    gpt_FLAG_LEGACY_BIOS_BOOTABLE = 0x3, // Not important, read UEFI GPT spec for info
} gpt__PartitionFlags;



typedef struct gpt__Partition {
    gpt__GUID type;
    gpt__GUID unique;
    uint64_t start_lba;
    uint64_t end_lba; // inclusive
    uint64_t attributes;
    uint16_t partition_name[36]; // UTF-16
} gpt__Partition;



bool gpt_find_partition(DiskDevice device, int partitionIndex, u64* start_lba, u64* end_lba);

