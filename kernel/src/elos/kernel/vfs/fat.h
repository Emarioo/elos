#pragma once

#include "elos/common/types.h"

#include "elos/vfs.h"

#include "elos/common/string.h"
#include "elos/common/types.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"
#include "elos/kernel_console.h"

typedef struct VFS_FileObject VFS_FileObject; // @NOCHECKIN Delete

typedef struct VFS_Mount VFS_Mount;

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

#define FAT_ENTRY_UNUSED ((char)(unsigned char)0xE5)

#define FAT_CLUSTER_UNUSED 0
#define FAT_CLUSTER_EOF 0xFFFFFFF
#define FAT_CLUSTER_RESERVED 0xFFFFFF8
#define FAT_CLUSTER_INVALID ((u32)-1)
#define VALID_CLUSTER(CLUSTER) ( (CLUSTER) != FAT_CLUSTER_EOF && (CLUSTER) != FAT_CLUSTER_INVALID && (CLUSTER) != FAT_CLUSTER_RESERVED)


typedef u64 FAT_ID;

#define FAT_ID_NULL (0)
#define FAT_ID_ROOT_DIR (1)

bool fat_is_fat(VFS_Mount* mount);

FAT_ID fat_get_root(VFS_Mount* mount);
FAT_ID fat_lookup(VFS_Mount* mount, FAT_ID directory, const cstring subname);
// Caller needs to make sure the entry doesn't already exist.
FAT_ID fat_make_entry(VFS_Mount* mount, FAT_ID directory, const cstring subname, u32 attributes);
bool fat_remove_entry(VFS_Mount* mount, FAT_ID id);
bool fat_iterate(VFS_Mount* mount, FAT_ID directory, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* entries);

u64 fat_read(VFS_Mount* mount, FAT_ID file, u64 offset, u64 size, void* buffer);
u64 fat_write(VFS_Mount* mount, FAT_ID file, u64 offset, u64 size, const void* buffer);

bool fat_info(VFS_Mount* mount, FAT_ID file, VFS_HandleInfo* info);


u64 fat__sane_mtime(const fat__DirectoryEntry* entry);












// // IMPLEMENTATION SPECIFIC

// VFS_FileObject* fat_mkfile(VFS_Mount* mount, const cstring path);
// // bool fat_rename(VFS_Mount* mount, const cstring old_path, const cstring new_path);



// bool fat_mkdir(VFS_Mount* mount, const cstring path);

// VFS_FileObject* find_file_object(VFS_Mount* mount, u32 clusterIndex);
// extern VFS_FileObject fileObjects[1000];


// VFS_FileObject* search_fat(VFS_Mount* mount, const cstring path);
// bool iter_fat(VFS_Mount* mount, VFS_FileObject* obj, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* entries);
// u64 read_fat(VFS_Handle_impl* handle, u64 offset, u64 size, void* buffer);
// u64 write_fat(VFS_Handle_impl* handle, u64 offset, u64 size, const void* buffer);

// bool fat_remove(VFS_Mount* mount, const cstring path);

