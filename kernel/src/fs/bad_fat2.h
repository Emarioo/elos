/*
    Component for reading and writing to FAT in memory.

    @TODO THIS IS OLD DELETE THIS
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>


//###############################
//  PUBLIC TYPES
//###############################


#pragma pack(push, 1)

// FAT BIOS Parameter Block
typedef struct {
    uint8_t  jmp_short[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t dir_entries_count;
    uint16_t total_sectors;
    uint8_t  media_descriptor_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t large_sector_count;
} FAT_bpb;

// FAT 12/16 Extended boot parameter block
typedef struct {
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  signature; // 0x28 or 0x29
    uint32_t volume_id;
    char     volume_label[11]; // space padding
    char     system_id[8];

    // uint8_t  code[448];

    // uint16_t boot_signature[2]; // 0xAA55
} FAT12_ebpb, FAT16_ebpb;

// FAT 32 Extended boot parameter block
typedef struct {
    uint32_t sectors_per_fat;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t cluster_number_of_root_directory; // often 2
    uint16_t sector_number_of_fsinfo;
    uint16_t sector_number_of_backup_boot_sector;
    uint8_t  reserved[12]; // should be zero on creation
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  signature;
    uint32_t volume_id;
    char     volume_label[11]; // "FAT32", space padding
    char     system_id[8];

    // uint8_t  code[420];

    // uint16_t boot_signature[2]; // 0xAA55
} FAT32_ebpb;

typedef struct {
    uint32_t signature; // always 0x41615252
    uint8_t  reserved[480];
    uint32_t signature2; // always 0x61417272
    uint32_t last_known_free_cluster_count; // -1 means it should be computed
    uint32_t next_maybe_free_cluster_number; // -1 means start searching from 2 (the start)
    uint8_t  reserved2[12];
    uint32_t boot_signature; // 0xAA550000
} FAT32_fsinfo;

typedef struct {
    FAT_bpb    standard_ebpb;
    FAT12_ebpb extended_ebpb;
} FAT12_boot_record, FAT16_boot_record;

typedef struct {
    FAT_bpb    standard_ebpb;
    FAT32_ebpb extended_ebpb;
} FAT32_boot_record;

#pragma pack(pop)


void FAT32_init_boot_record(FAT32_boot_record* boot_record);



// #define UNUSED_CLUSTER 0x123
#define UNUSED_CLUSTER 0x000
#define RESERVED_CLUSTER_MIN 0xFF0
#define RESERVED_CLUSTER_MAX 0xFF6
#define BAD_CLUSTER 0xFF7
#define LAST_CLUSTER_MIN 0xFF8
#define LAST_CLUSTER_MAX 0xFFF

#define FAT16_UNUSED_CLUSTER 0x0000
#define FAT16_RESERVED_CLUSTER_MIN 0xFFF0
#define FAT16_RESERVED_CLUSTER_MAX 0xFFF6
#define FAT16_BAD_CLUSTER 0xFFF7
#define FAT16_LAST_CLUSTER_MIN 0xFFF8
#define FAT16_LAST_CLUSTER_MAX 0xFFFF

typedef void fat12;
typedef void fat16;

enum DirectoryAttributes {
    READ_ONLY=0x01,
    HIDDEN=0x02,
    SYSTEM=0x04,
    VOLUME_ID=0x08,
    DIRECTORY=0x10,
    ARCHIVE=0x20,
    LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID
};

#pragma pack(push,1)
typedef struct {
    char file_name[11];
    uint8_t attributes;
    uint8_t _reserved;
    uint8_t creation_time_ms; // not actually ms, more like 100 hundreths of a second
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t accessed_date;
    uint16_t zero; // zero for FAT12, high 16 bits of first cluster number otherwise
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t first_cluster_number;
    uint32_t file_size;
} DirectoryEntry;
typedef struct {
    uint8_t order;
    uint16_t file_name5[5];
    uint8_t attributes;
    uint8_t zero;
    uint8_t checksum;
    uint16_t file_name6[6];
    uint16_t zero2;
    uint16_t file_name2[6];
} LongNameEntry;
#pragma pack(pop)


//###############################
//  PUBLIC FUNCTIONS
//###############################


void fat12_init_table(fat12* data);

void fat12_write_file(fat12* data, int entry_index, int offset, char* buffer, int size);

int fat12_create_entry(fat12* data, char* name, bool is_dir);
void fat12_write_table_entry(char* entries, int entry_index, int value);
int fat12_read_table_entry(char* entries, int entry_index);
char* fat12_get_cluster_memory(fat12* data, int cluster);
// returns logical cluster number
int fat12_alloc_cluster(fat12* data);



void fat16_init_table(fat16* data);
void fat16_mirror_fat(fat16* data);

void fat16_write_file(fat16* data, int entry_index, int offset, char* buffer, int size);

int fat16_create_entry(fat16* data, char* name, bool is_dir);
void fat16_write_table_entry(char* entries, int entry_index, int value);
int fat16_read_table_entry(char* entries, int entry_index);
char* fat16_get_cluster_memory(fat16* data, int cluster);
// returns logical cluster number
int fat16_alloc_cluster(fat16* data);

