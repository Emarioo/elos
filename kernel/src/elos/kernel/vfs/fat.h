#pragma once

#include "elos/common/types.h"


typedef enum fat__DirectoryAttributes {
    fat__READ_ONLY  = 0x01,
    fat__HIDDEN     = 0x02,
    fat__SYSTEM     = 0x04,
    fat__VOLUME_ID  = 0x08,
    fat__DIRECTORY  = 0x10,
    fat__ARCHIVE    = 0x20,
    fat__LFN        = fat__READ_ONLY | fat__HIDDEN | fat__SYSTEM | fat__VOLUME_ID,
    fat__LFN_MASK   = fat__READ_ONLY | fat__HIDDEN | fat__SYSTEM | fat__VOLUME_ID | fat__DIRECTORY | fat__ARCHIVE,
} fat__DirectoryAttributes;

#pragma pack(push, 1)
typedef struct fat__BPB {
    uint8_t  jmp_short[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_dir_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} fat__BPB;
#pragma pack(pop)

// FAT 12/16 Extended boot parameter block
#pragma pack(push, 1)
typedef struct fat16__EBPB {
    uint8_t  drive_number;
    uint8_t  _reserved0;
    uint8_t  signature; // 0x28 or 0x29
    uint32_t volume_id;
    char     volume_label[11]; // space padding
    char     system_id[8];

    // uint8_t  code[448];

    // uint16_t boot_signature[2]; // 0xAA55
} fat16__EBPB;
#pragma pack(pop)

// FAT 32 Extended boot parameter block
#pragma pack(push, 1)
typedef struct fat32__EBPB {
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    
    uint16_t fsinfo_cluster;
    uint16_t backup_boot_sector;
    uint8_t  _reserved0[12]; // should be zero on creation
    uint8_t  drive_number;
    uint8_t  _reserved1;
    uint8_t  signature;
    uint32_t volume_id;
    char     volume_label[11]; // "FAT32", space padding
    char     system_id[8];
    
    // uint8_t  code[420];
    
    // uint16_t boot_signature[2]; // 0xAA55
} fat32__EBPB;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct fat__FSInfo {
    uint32_t signature; // always 0x41615252
    uint8_t  _reserved0[480];
    uint32_t signature2; // always 0x61417272
    uint32_t last_known_free_cluster_count; // -1 means it should be computed
    uint32_t next_maybe_free_cluster_number; // -1 means start searching from 2 (the start)
    uint8_t  _reserved1[12];
    uint32_t boot_signature; // 0xAA550000
} fat__FSInfo;
#pragma pack(pop)



#pragma pack(push,1)
typedef struct fat__DirectoryEntry {
    char file_name[11];
    uint8_t attributes;
    uint8_t _reserved0;
    uint8_t creation_time_ms; // not actually ms, more like 100 hundreths of a second
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t accessed_date;
    uint16_t cluster_high;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t cluster_low;
    uint32_t file_size;
} fat__DirectoryEntry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint8_t  order;
    uint16_t file_name0[5];
    uint8_t  attributes;
    uint8_t  _zero0;
    uint8_t  checksum;
    uint16_t file_name1[6];
    uint16_t _zero1;
    uint16_t file_name2[2];
} fat__LongNameEntry;
#pragma pack(pop)

#define fat__LFN_MAX_CHARS 13
#define fat__LAST_LONG_ENTRY 0x40


#define fat__END_OF_FILE 0xFFFFFFF
#define fat__RESERVED 0xFFFFFF8


static bool IsLeapYear(int year);
static int DaysBeforeMonth(int month, bool leap);
u64 FAT_ToUnixMicroseconds(u16 fatDate, u16 fatTime, u8 creationTenths);
u64 fat__sane_mtime(const fat__DirectoryEntry* entry);
bool fat__string_equal(const cstring name, const cstring entryName);
