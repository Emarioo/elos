#pragma once

#include "elos/util.h"


// #define UNUSED_CLUSTER 0x123
#define UNUSED_CLUSTER 0x000
#define RESERVED_CLUSTER_MIN 0xFF0
#define RESERVED_CLUSTER_MAX 0xFF6
#define BAD_CLUSTER 0xFF7
#define LAST_CLUSTER_MIN 0xFF8
#define LAST_CLUSTER_MAX 0xFFF

typedef void fat12;

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
    u8 attributes;
    u8 _reserved;
    u8 creation_time_ms; // not actually ms, more like 100 hundreths of a second
    u16 creation_time;
    u16 creation_date;
    u16 accessed_date;
    u16 zero; // zero for FAT12, high 16 bits of first cluster number otherwise
    u16 modified_time;
    u16 modified_date;
    u16 first_cluster_number;
    u32 file_size;
} DirectoryEntry;
typedef struct {
    u8 order;
    u16 file_name5[5];
    u8 attributes;
    u8 zero;
    u8 checksum;
    u16 file_name6[6];
    u16 zero2;
    u16 file_name2[6];
} LongNameEntry;
#pragma pack(pop)

//###############################
//  PUBLIC FUNCTIONS
//###############################

void fat12_init_table(fat12* data);
void fat12_init_bootsector(fat12* data, u8* buffer, int size);

void fat12_write_file(fat12* data, int entry_index, int offset, char* buffer, int size);

int fat12_create_entry(fat12* data, char* name, bool is_dir);
void fat12_write_table_entry(char* entries, int entry_index, int value);
int fat12_read_table_entry(char* entries, int entry_index);
char* fat12_get_cluster_memory(fat12* data, int cluster);
// returns logical cluster number
int fat12_alloc_cluster(fat12* data);

//##############################
//   PRIVATE FUNCTIONS
//##############################