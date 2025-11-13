#pragma once


#include <stdint.h>
#include <stdbool.h>

#ifdef ELOS_DEBUG
    #include <stdio.h>

    #define log(...) printf(__VA_ARGS__)

    int _hits;
    void trigger_break() {
        _hits++;
    }
#else
    #define log(...)
#endif

typedef int (*fat__read_sectors_fn) (void* buffer, uint64_t lba, uint64_t count, void* user_data);
typedef int (*fat__write_sectors_fn)(const void* buffer, uint64_t lba, uint64_t count, void* user_data);


typedef struct fat__Context {
    // Input parameters
    fat__read_sectors_fn read_sectors;
    fat__write_sectors_fn write_sectors;
    void* user_data;
    char* working_memory;
    int working_memory_len;
    uint32_t sector_size;
    // The fat32 API needs you to specify which partition to 
    // create/read fat32 file system to/from.
    // Rather than providing index to GPT entry or GUID we 
    // give LBA range.
    uint64_t lba_start;
    uint64_t lba_end;

    // Modified by API functions
    int working_memory_head; // Should be initialized to zero 
} fat__Context;

typedef enum fat__Error {
    fat__ERROR_SUCCESS,
    fat__ERROR_UNKNOWN,
    fat__ERROR_OUT_OF_MEMORY,
} fat__Error;


typedef enum fat__DirectoryAttributes {
    fat__READ_ONLY=0x01,
    fat__HIDDEN=0x02,
    fat__SYSTEM=0x04,
    fat__VOLUME_ID=0x08,
    fat__DIRECTORY=0x10,
    fat__ARCHIVE=0x20,
    fat__LFN=fat__READ_ONLY|fat__HIDDEN|fat__SYSTEM|fat__VOLUME_ID,
} fat__DirectoryAttributes;


// @TODO Add bootable parameter?
int fat__overwrite_header(fat__Context* context);

int fat__create_entry(fat__Context* context, const char* path, fat__DirectoryAttributes attributes);

int fat__write_data(fat__Context* context, const char* path, uint64_t offset, void* buffer, uint64_t size);

int fat__read_data(fat__Context* context, const char* path, uint64_t offset, void* buffer, uint64_t size);

#ifdef ELOS_DEBUG

int fat__print_header(fat__Context* context);

#endif // ELOS_DEBUG

#ifdef FAT_IMPL

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




typedef struct fat__InternalContext {
    fat__Context* context;

    int fat_version;
    int sectors_per_fat;
    int fat_entries_per_sector; // may not align to a whole sector (because of FAT12)

    fat__BPB* bpb;
    fat16__EBPB* ebpb16;
    fat32__EBPB* ebpb32;

    char* split_path[20];
    int split_count;
    char path_data[256];

    char* loaded_fat_sector;
    int loaded_fat_sector_index; // relative to start of FAT file system (BPB)

} fat__InternalContext;


#define fat__END_OF_FILE 0xFFFFFFF
#define fat__RESERVED 0xFFFFFF8


static void fat__memzero(void* addr, int size) {
    // for(int i=0;i<size;i++)
    //     ((char*)addr)[i] = 0;

    uintptr_t ptr = (uintptr_t)addr;
    // Align to 8 bytes if possible
    while (size >= 8 && (ptr & 7) != 0) {
        *(uint8_t*)ptr = 0;
        ptr++;
        size--;
    }
    // Zero 64 bits at a time
    while (size >= 8) {
        *(uint64_t*)ptr = 0;
        ptr += 8;
        size -= 8;
    }
    // Zero remaining bytes
    while (size > 0) {
        *(uint8_t*)ptr = 0;
        ptr++;
        size--;
    }
}
static void fat__memset(void* addr, int val, int size) {
    // for(int i=0;i<size;i++)
    //     ((char*)addr)[i] = val;

    uintptr_t ptr = (uintptr_t)addr;
    uint8_t byte_val = (uint8_t)val;
    uint64_t word_val = (uint64_t)byte_val;
    word_val |= word_val << 8;
    word_val |= word_val << 16;
    word_val |= word_val << 32;

    // Align to 8 bytes if possible
    while (size >= 8 && (ptr & 7) != 0) {
        *(uint8_t*)ptr = byte_val;
        ptr++;
        size--;
    }
    // Set 64 bits at a time
    while (size >= 8) {
        *(uint64_t*)ptr = word_val;
        ptr += 8;
        size -= 8;
    }
    // Set remaining bytes
    while (size > 0) {
        *(uint8_t*)ptr = byte_val;
        ptr++;
        size--;
    }
}
static void fat__memcpy(void* dst, const void* src, int size) {
    // for(int i=0;i<size;i++)
    //     ((char*)dst)[i] = ((char*)src)[i];

    uintptr_t dst_ptr = (uintptr_t)dst;
    uintptr_t src_ptr = (uintptr_t)src;

    // Align to 8 bytes if possible
    while (size >= 8 && ((dst_ptr & 7) != 0 || (src_ptr & 7) != 0)) {
        *(uint8_t*)dst_ptr = *(uint8_t*)src_ptr;
        dst_ptr++;
        src_ptr++;
        size--;
    }
    // Copy 64 bits at a time
    while (size >= 8) {
        *(uint64_t*)dst_ptr = *(uint64_t*)src_ptr;
        dst_ptr += 8;
        src_ptr += 8;
        size -= 8;
    }
    // Copy remaining bytes
    while (size > 0) {
        *(uint8_t*)dst_ptr = *(uint8_t*)src_ptr;
        dst_ptr++;
        src_ptr++;
        size--;
    }
}
static int fat__strlen(const char* str) {
    int len = 0;
    while(str[len])
        len++;
    return len;
}
static int fat__strcmp(const char* a, const char* b) {
    int head = 0;
    while (a[head] || b[head]) {
        if (a[head] != b[head]) {
            return (a[head] < b[head]) + (a[head] > b[head]);
        }
        head++;
    }
    return 0;
}

void* fat__alloc(fat__Context* context, int size) {
    if (context->working_memory_head + size >= context->working_memory_len)
        return NULL;

    // ensure 8-byte alignment
    if (context->working_memory_head % 8 != 0)
        context->working_memory_head += 8 - size % 8;

    void* ptr = context->working_memory + context->working_memory_head;
    context->working_memory_head += size;

    return ptr;
}

#define fat__FAT12 1
#define fat__FAT16 2
#define fat__FAT32 3
int fat__init_internal(fat__InternalContext* internal) {
    fat__Context* context = internal->context;
    if (internal->fat_version == fat__FAT32) {
        internal->sectors_per_fat = internal->ebpb32->sectors_per_fat_32;
        internal->fat_entries_per_sector = context->sector_size / 4;
    } else if (internal->fat_version == fat__FAT16) {
        internal->sectors_per_fat = internal->bpb->sectors_per_fat_16;
        internal->fat_entries_per_sector = context->sector_size / 2;
    } else if (internal->fat_version == fat__FAT12) {
        internal->sectors_per_fat = internal->bpb->sectors_per_fat_16;
        internal->fat_entries_per_sector = 0; // Can't use this for fat12, doesn't divide cleanly
    } else {
        return 1;
    }

    internal->loaded_fat_sector = fat__alloc(context, context->sector_size * 2);
    if (!internal->loaded_fat_sector) return fat__ERROR_OUT_OF_MEMORY;
    internal->loaded_fat_sector_index = -1;

    return 0;
}

int fat__detect_type(fat__BPB* bpb) {
    if (bpb->sectors_per_fat_16 == 0)
        return fat__FAT32;

    // Otherwise FAT12 or FAT16
    uint32_t root_dir_sectors = ((bpb->root_dir_entries * sizeof(fat__DirectoryEntry))
                                + (bpb->bytes_per_sector - 1))
                                / bpb->bytes_per_sector;

    uint32_t total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    uint32_t fat_size = bpb->sectors_per_fat_16;
    uint32_t data_sectors = total_sectors
        - (bpb->reserved_sectors + (bpb->fat_count * fat_size) + root_dir_sectors);

    uint32_t cluster_count = data_sectors / bpb->sectors_per_cluster;

    if (cluster_count < 4085)
        return fat__FAT12;
    else
        return fat__FAT16;
}

int fat__copy_sectors(fat__InternalContext* internal, uint64_t dst_lba, uint64_t src_lba, uint64_t count) {
    int res;
    // @TODO Check overlap
    fat__Context* context = internal->context;
    void* temp_sector = fat__alloc(internal->context, internal->context->sector_size);

    for (int i=0;i<count;i++) {
        res = context->read_sectors(temp_sector, src_lba + i, 1, context->user_data);
        if (res) return res;
        res = context->write_sectors(temp_sector, dst_lba + i, 1, context->user_data);
        if (res) return res;
    }
    return 0;
}

int fat__copy_fat(fat__InternalContext* internal) {
    int res;
    fat__Context* context = internal->context;
    if (internal->bpb->fat_count == 1) {
        return 0;
    }
    if (internal->bpb->fat_count > 2) {
        return 1;
    }

    uint64_t src_lba = internal->bpb->reserved_sectors;
    uint64_t dst_lba = internal->bpb->reserved_sectors + internal->sectors_per_fat;

    res = fat__copy_sectors(internal, dst_lba, src_lba, internal->sectors_per_fat);
    if (res) return res;

    return 0;
}

int fat__overwrite_header(fat__Context* context) {
    int res;
    // @TODO Check errors

    int fat_version;

    uint64_t total_sectors = context->lba_end - context->lba_start;
    if (total_sectors * context->sector_size <= 0x400000) { // 4MB
        fat_version = fat__FAT12;
    } else if (total_sectors * context->sector_size <= 0x20000000) { // 512MB
        fat_version = fat__FAT16;
    } else {
        fat_version = fat__FAT32;
    }

    fat__BPB* bpb = (fat__BPB*) fat__alloc(context, context->sector_size);
    if (!bpb) return fat__ERROR_OUT_OF_MEMORY;
    fat__memzero((char*)bpb, context->sector_size);
    
    bpb->jmp_short[0] = 0xEB;
    bpb->jmp_short[1] = 0x3C;
    bpb->jmp_short[2] = 0x90;

    *(uint16_t*)((char*)bpb + 510) = 0xAA55;


    fat__memcpy(bpb->oem, "MTOO4043", 8);

    if (!(context->sector_size == 512 || context->sector_size == 1024 || context->sector_size == 2048 || context->sector_size == 4096))
        return fat__ERROR_UNKNOWN;

        
    bpb->bytes_per_sector = context->sector_size;
    bpb->sectors_per_cluster = 2;
        
    if (context->sector_size != 512)
        return fat__ERROR_UNKNOWN;


    bpb->fat_count = 2;

    if (fat_version == fat__FAT32) {
        bpb->reserved_sectors = 32;
        bpb->root_dir_entries = 0;
        bpb->total_sectors_16 = 0;
        bpb->total_sectors_32 = context->lba_end - context->lba_start;
    } else {
        bpb->reserved_sectors = 1;
        // FAT16 has a special root directory at cluster 1.
        // In FAT32 root dir is just a normal directory which
        // is referred to in the header.
        bpb->root_dir_entries = 512;
        if (context->lba_end - context->lba_start < 0x10000) {
            bpb->total_sectors_16 = context->lba_end - context->lba_start;
            bpb->total_sectors_32 = 0;
        } else {
            bpb->total_sectors_16 = 0;
            bpb->total_sectors_32 = context->lba_end - context->lba_start;
        }
    }

    bpb->media_descriptor_type = 0xf8; // fixes, non removable media

    int sectors_per_fat = 0;

    if (fat_version == fat__FAT32) {
        bpb->sectors_per_fat_16 = 0;
    } else if (fat_version == fat__FAT16) {
        // @TODO Scale with total sectors, sectors per cluster, sector size?
        bpb->sectors_per_fat_16 = sectors_per_fat = 16;
    } else if (fat_version == fat__FAT12) {
        // @TODO Scale with total sectors, sectors per cluster, sector size?
        bpb->sectors_per_fat_16 = sectors_per_fat = 12;
    }

    bpb->sectors_per_track = 63;
    bpb->num_heads = 16;
    bpb->hidden_sectors = 0;

    fat32__EBPB* ebpb32 = (fat32__EBPB*)((char*)bpb + 36);
    fat16__EBPB* ebpb16 = (fat16__EBPB*)((char*)bpb + 36);

    if (fat_version == fat__FAT32) {
        fat32__EBPB* ebpb = (fat32__EBPB*)((char*)bpb + 36);

        ebpb->sectors_per_fat_32 = 0;
        while (1) {
            uint64_t data_sectors = bpb->total_sectors_32 - (bpb->reserved_sectors + bpb->fat_count * ebpb->sectors_per_fat_32);
            uint64_t count_of_clusters = data_sectors / bpb->sectors_per_cluster;
            uint64_t new_spf = (count_of_clusters * 4 + bpb->bytes_per_sector-1) / bpb->bytes_per_sector;
            if (new_spf == ebpb->sectors_per_fat_32)
                break;
            ebpb->sectors_per_fat_32 = new_spf;
        }
        sectors_per_fat = ebpb->sectors_per_fat_32;

        ebpb->flags = 1 << 7; // one active fat at index 0. no mirroring
        ebpb->fat_version = 0;
        ebpb->root_cluster = 2;
        ebpb->fsinfo_cluster = 1;
        ebpb->backup_boot_sector = 6;
        fat__memzero(ebpb->_reserved0, 12);
        ebpb->drive_number = 0x80;
        ebpb->_reserved1 = 0;
        ebpb->signature = 0x29;
        ebpb->volume_id = 0x3590; // should be chosen based on date and time, altough this is number probably fine.
        fat__memcpy(ebpb->volume_label, "NO NAME    ", 11);
        fat__memcpy(ebpb->system_id, "FAT32   ", 8);

        fat__FSInfo* fsinfo = (fat__FSInfo*)fat__alloc(context, context->sector_size);
        if (!fsinfo) return fat__ERROR_OUT_OF_MEMORY;

        fsinfo->signature = 0x41615252;
        fsinfo->signature2 = 0x61417272;
        fsinfo->last_known_free_cluster_count = 0xFFFFFFFF;
        fsinfo->next_maybe_free_cluster_number = 0xFFFFFFFF;
        fat__memzero(&fsinfo->_reserved0, 480);
        fat__memzero(&fsinfo->_reserved1, 12);

        // @TODO What is sector size isn't 512. We need to zero rest of BPB and FSINFO

        // Backup BPB
        res = context->write_sectors(bpb, context->lba_start + 6, 1, context->user_data);
        if (res) return res;

        res = context->write_sectors(fsinfo, context->lba_start + 1, 1, context->user_data);
        if (res) return res;
    } else {
        fat16__EBPB* ebpb = (fat16__EBPB*)((char*)bpb + 36);
        ebpb->drive_number = 0x80;
        ebpb->_reserved0 = 0;
        ebpb->signature = 0x29;
        ebpb->volume_id = 0x3DD00AB0; // Should be date and time combined
        fat__memcpy(ebpb->volume_label, "NO NAME    ", 11);
        if (fat_version == fat__FAT12)
            memcpy(ebpb->system_id, "FAT12   ", 8);
        else
            memcpy(ebpb->system_id, "FAT16   ", 8);
    }
    
    res = context->write_sectors(bpb, context->lba_start, 1, context->user_data);
    if (res) return res;

    
    void* temp_sector = fat__alloc(context, context->sector_size);
    if (!temp_sector) return fat__ERROR_OUT_OF_MEMORY;
    fat__memzero(temp_sector, context->sector_size);

    // Init File Allocation Table, with some values, rest are zeros.
    if (fat_version == fat__FAT32) {
        ((uint32_t*)temp_sector)[0] = 0xFFFFFF8;  // reserved
        ((uint32_t*)temp_sector)[1] = 0xFFFFFF8;  // reserved
        ((uint32_t*)temp_sector)[2] = 0xFFFFFFF; // root directory
    } else if(fat_version == fat__FAT16) {
        ((uint16_t*)temp_sector)[0] = 0xFFF8; // reserved
        ((uint16_t*)temp_sector)[1] = 0xFFF8; // reserved
    } else {
        ((uint16_t*)temp_sector)[0] = 0x8FF8; // reserved (12 bits per cluster)
        ((uint16_t*)temp_sector)[1] = 0xFF;   // reserved
    }

    int fat_sector_index = bpb->reserved_sectors;
    res = context->write_sectors(temp_sector, context->lba_start + fat_sector_index, 1, context->user_data);
    if (res) return res;
    
    fat__memzero(temp_sector, context->sector_size);
    
    for (int i = 1; i < sectors_per_fat; i++) {
        int fat_sector_index = bpb->reserved_sectors + i;
        res = context->write_sectors(temp_sector, context->lba_start + fat_sector_index, 1, context->user_data);
        if (res) return res;
    }
    if (fat_version != fat__FAT32) {
        for (int i = 0; i < (bpb->root_dir_entries * sizeof(fat__DirectoryEntry)) / context->sector_size; i++) {
            int sector_index = bpb->reserved_sectors + sectors_per_fat * bpb->fat_count + i;
            res = context->write_sectors(temp_sector, context->lba_start + sector_index, 1, context->user_data);
            if (res) return res;
        }
    }

    
    uint64_t data_region_start = bpb->reserved_sectors + sectors_per_fat * bpb->fat_count + ( fat_version != fat__FAT32 ? (bpb->root_dir_entries * sizeof(fat__DirectoryEntry)) / context->sector_size : 0);
    for (int i = 0; i < context->lba_end - data_region_start; i++) {
        int sector_index = data_region_start + i;
        res = context->write_sectors(temp_sector, context->lba_start + sector_index, 1, context->user_data);
        if (res) return res;
    }

    fat__InternalContext _internal = {0};
    fat__InternalContext* internal = &_internal;
    internal->context = context;
    internal->bpb = bpb;
    internal->ebpb16 = ebpb16;
    internal->ebpb32 = ebpb32;
    internal->fat_version = fat__detect_type(bpb);

    res = fat__init_internal(internal);
    if (res) return res;

    res = fat__copy_fat(internal);
    if (res) return res;

    context->working_memory_head = 0;
    return fat__ERROR_SUCCESS;
}


int fat__write_fat(fat__InternalContext* internal, int cluster, uint32_t value);
uint32_t fat__read_fat(fat__InternalContext* internal, int cluster);

int fat__sane_cstring(fat__InternalContext* internal, const fat__DirectoryEntry* entry, char* out_path) {
    int res;

    if (entry->attributes == fat__LFN) {
        return 1;
    }

    int head = 7;
    while (head >= 0 && (entry->file_name[head] == 0 || entry->file_name[head] == ' ')) {
        head--;
    }
    if (head < 0)
        return 1;

    fat__memcpy(out_path, entry->file_name, head + 1);

    int ext_head = 2;
    while (ext_head >= 0 && (entry->file_name[ext_head + 8] == 0 || entry->file_name[ext_head + 8] == ' ')) {
        ext_head--;
    }

    if (ext_head != -1) {
        out_path[head+1] = '.';
        fat__memcpy(out_path + head+2, entry->file_name + 8, ext_head + 1);
        out_path[head+1 + 1 + ext_head+1] = '\0';
    } else {
        out_path[head+1] = '\0';
    }


    return 0;
}

int fat__cluster_to_sector_offset(fat__InternalContext* internal, int cluster) {
    fat__Context* context = internal->context;

    return internal->bpb->reserved_sectors + internal->bpb->fat_count * internal->sectors_per_fat + (internal->bpb->root_dir_entries * sizeof(fat__DirectoryEntry) + context->sector_size-1)/context->sector_size + (cluster - 2) * internal->bpb->sectors_per_cluster;
}

// Returns -1 if long name is required
int fat__insert_name(fat__InternalContext* internal, fat__DirectoryEntry* entry, const char* name) {
    int res;
    
    int name_len = fat__strlen(name);

    int dot_index = name_len-1;
    while(dot_index >= 0) {
        if (name[dot_index] == '.') {
            break;
        }
        dot_index--;
    }

    if (name_len > 12 || (dot_index == -1 && name_len > 8) || (dot_index != -1 && (dot_index > 8 || name_len - dot_index - 1 > 3))) {
        // long name
        return 1;
    } else {
        for (int i=0;i<11;i++)
            entry->file_name[i] = ' ';
        if (dot_index == -1) {
            fat__memcpy(entry->file_name, name, name_len);
        } else {
            fat__memcpy(entry->file_name, name, dot_index);
            fat__memcpy(entry->file_name + 8, name + dot_index + 1, name_len - dot_index - 1);
        }
    }
    return 0;
}

int fat__find_free_cluster(fat__InternalContext* internal, int* cluster);


// Find directory containing the specified path
int fat__find_dir_entry(fat__InternalContext* internal, int* cluster, int create_missing) {
    int res;

    fat__Context* context = internal->context;


    int sector_start = 0;
    int sector_end = 0;
    int sector_index = 0;
    int current_cluster = 0;

    if (internal->fat_version == fat__FAT32) {
        current_cluster = internal->ebpb32->root_cluster;
        sector_start = fat__cluster_to_sector_offset(internal, current_cluster);
        sector_end = sector_start + internal->bpb->sectors_per_cluster;
        sector_index = 0;
    } else {
        current_cluster = -1;
        sector_index = 0;
        sector_start = internal->bpb->reserved_sectors + internal->sectors_per_fat * internal->bpb->sectors_per_cluster;
        sector_end = sector_start + internal->bpb->root_dir_entries * sizeof(fat__DirectoryEntry) / context->sector_size;
    }

    if (internal->split_count == 0) {
        *cluster = current_cluster;
        return 0;
    }

    int split_index = 0;

    // sectors per cluster
    char* temp_sector = fat__alloc(context, context->sector_size);
    if (!temp_sector) return fat__ERROR_OUT_OF_MEMORY;
    
    char* empty_sector = fat__alloc(context, context->sector_size);
    if (!empty_sector) return fat__ERROR_OUT_OF_MEMORY;
    fat__memzero(empty_sector, context->sector_size);

    int entry_number = 0;
    int avail_entry_number = -1;

    while (split_index < internal->split_count-1) {

        if (sector_start + sector_index >= sector_end) {
            // @TODO We need to check if directory entry has more clusters.
            //   If it's fat16 and root directory entry we need special case because cluster/sector/entry count is determined bpb->root_dir_entries
            // could not find entry in cluster
            if (avail_entry_number == -1) {
                // Could not find available entry
                return 1;
            }

            if (!create_missing) {
                return 1;
            }

            // Create new entry
            sector_index = (avail_entry_number * sizeof(fat__DirectoryEntry)) / context->sector_size;
            res = context->read_sectors(temp_sector, context->lba_start + sector_start + sector_index, 1, context->user_data);
            if (res) return res;
            
            fat__DirectoryEntry* entries = (fat__DirectoryEntry*)temp_sector;
            int entries_len = context->sector_size / sizeof(fat__DirectoryEntry);
            fat__DirectoryEntry* entry = &entries[avail_entry_number % entries_len];

            entry->attributes = fat__DIRECTORY;

            res = fat__insert_name(internal, entry, internal->split_path[split_index]);
            if (res) return res;
            entry->file_size = 0;

            int new_cluster;
            res = fat__find_free_cluster(internal, &new_cluster);
            if (res) return res;
            entry->cluster_low = new_cluster & 0xFFFF;
            entry->cluster_high = new_cluster >> 16;
            
            res = context->write_sectors(temp_sector, context->lba_start + sector_start + sector_index, 1, context->user_data);
            if (res) return res;

            res = fat__write_fat(internal, new_cluster, fat__END_OF_FILE);
            if (res) return res;

            int sector = fat__cluster_to_sector_offset(internal, new_cluster);
            res = context->write_sectors(empty_sector, context->lba_start + sector, 1, context->user_data);
            if (res) return res;

            int parent_cluster = current_cluster;
            current_cluster = new_cluster;
            sector_index = 0;
            sector_start = fat__cluster_to_sector_offset(internal, current_cluster);
            sector_end = sector_start + internal->bpb->sectors_per_cluster;

            {
                res = context->read_sectors(temp_sector, context->lba_start + sector_start + sector_index, 1, context->user_data);
                if (res) return res;

                fat__DirectoryEntry* entry_dot = (fat__DirectoryEntry*)temp_sector;
                fat__DirectoryEntry* entry_dot2 = (fat__DirectoryEntry*)temp_sector + 1;

                fat__memset(entry_dot->file_name, ' ', 11);
                entry_dot->file_name[0] = '.';
                entry_dot->attributes   = fat__DIRECTORY;
                entry_dot->cluster_low  = current_cluster & 0xFFFF;
                entry_dot->cluster_high = current_cluster >> 16;

                
                fat__memset(entry_dot2->file_name, ' ', 11);
                entry_dot2->file_name[0] = '.';
                entry_dot2->file_name[1] = '.';
                entry_dot2->attributes   = fat__DIRECTORY;
                entry_dot2->cluster_low  = parent_cluster & 0xFFFF;
                entry_dot2->cluster_high = parent_cluster >> 16;

                
                res = context->write_sectors(temp_sector, context->lba_start + sector_start + sector_index, 1, context->user_data);
                if (res) return res;
            }

            split_index++;
            avail_entry_number = -1;
            entry_number = 0;
            continue;
        }

        res = context->read_sectors(temp_sector, context->lba_start + sector_start + sector_index, 1, context->user_data);
        if (res) return res;

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)temp_sector;
        int entries_len = context->sector_size / sizeof(fat__DirectoryEntry);

        for (int i=0;i<entries_len;i++) {
            fat__DirectoryEntry* entry = &entries[i];
            entry_number++;

            if (entry->attributes == fat__LFN) {
                // Can't handle
                continue;
            }
            if (entry->file_name[0] == 0xE5) {
                // Not present
                if (avail_entry_number == -1)
                    avail_entry_number = entry_number-1;
                continue;
            }
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                if (avail_entry_number == -1)
                    avail_entry_number = entry_number-1;
                sector_index = sector_end - sector_start - 1;
                break;
            }
            if ((entry->attributes & fat__DIRECTORY) == 0) {
                // Not a directory
                return 1;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ')
                continue;
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ')
                continue;

            const char* name = internal->split_path[split_index];
            char file_name[256];
            res = fat__sane_cstring(internal, entry, file_name);
            if (res) return 1;

            if (fat__strcmp(name, file_name)) {
                // not equal
                continue;
            }

            // Found it
            current_cluster = (int)entry->cluster_low | ((int)entry->cluster_high << 16);
            sector_index = -1; // incremented later
            sector_start = fat__cluster_to_sector_offset(internal, current_cluster);
            sector_end = sector_start + internal->bpb->sectors_per_cluster;

            split_index++;
            avail_entry_number = -1;
            break;
        }
        sector_index++;
    }

    *cluster = current_cluster;

    return 0;
}


uint32_t fat__read_fat(fat__InternalContext* internal, int cluster) {
    int res;
    fat__Context* context = internal->context;
    
    uint32_t value = -1;
    if (cluster <= 1) {
        log("Cluster is invalid/reserved value: %d\n", cluster);
        trigger_break();
    }

    if (internal->fat_version == fat__FAT16 || internal->fat_version == fat__FAT32) {
        // Clean cluster number divide, FAT entry can't span across sectors.
        int sector_index = internal->bpb->reserved_sectors + cluster / internal->fat_entries_per_sector;
        if (sector_index != internal->loaded_fat_sector_index) {
            res = context->read_sectors(internal->loaded_fat_sector, context->lba_start + sector_index, 1, context->user_data);
            if (res) return res;
            internal->loaded_fat_sector_index = sector_index;
        }

        void* sector_buffer = internal->loaded_fat_sector;
        int entry_index = cluster % internal->fat_entries_per_sector;

        if (internal->fat_version == fat__FAT32) {
            value =((uint32_t*)sector_buffer)[entry_index];
        } else if (internal->fat_version == fat__FAT16) {
            value = ((uint16_t*)sector_buffer)[entry_index];
            if (value == 0xFFFF)
                value = fat__END_OF_FILE;
            if (value == 0xFFF8)
                value = fat__RESERVED;
        }
    } else {
        int fat_offset = cluster + cluster / 2;
        
        int sector_index = internal->bpb->reserved_sectors + fat_offset / context->sector_size;

        if (sector_index != internal->loaded_fat_sector_index) {
            res = context->read_sectors(internal->loaded_fat_sector, context->lba_start + sector_index, 2, context->user_data);
            if (res) return res;
        }

        char* sector_buffer = internal->loaded_fat_sector;

        if (cluster & 1) {
            // check odd or even
            value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) >> 4;
        } else {
            value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) & 0x0FFF;
        }
      
        if (value == 0xFFF)
            value = fat__END_OF_FILE;
        if (value == 0xFF8)
            value = fat__RESERVED;
    }

    return value;
}
int fat__write_fat(fat__InternalContext* internal, int cluster, uint32_t value) {
    int res;
    fat__Context* context = internal->context;

    if (internal->fat_version == fat__FAT32 || internal->fat_version == fat__FAT16) {
        int sector_index =  internal->bpb->reserved_sectors + cluster / internal->fat_entries_per_sector;

        if (sector_index != internal->loaded_fat_sector_index) {
            res = context->read_sectors(internal->loaded_fat_sector, context->lba_start + sector_index, 1, context->user_data);
            if (res) return res;
            internal->loaded_fat_sector_index = sector_index;
        }

        int entry_index = cluster % internal->fat_entries_per_sector;

        if (internal->fat_version == fat__FAT32) {
            // NOTE: We must preserve upper 4 bits
            ((uint32_t*)internal->loaded_fat_sector)[entry_index] =
                (value & 0x0FFFFFFF) | (((uint32_t*)internal->loaded_fat_sector)[entry_index] & 0xF0000000);
        } else if (internal->fat_version == fat__FAT16) {
            if (value == fat__END_OF_FILE)
                value = 0xFFFF;
            ((uint16_t*)internal->loaded_fat_sector)[entry_index] = value;
        }
        res = context->write_sectors(internal->loaded_fat_sector, context->lba_start + sector_index, 1, context->user_data);
        if (res) return res;
    } else {
        int fat_offset = cluster + cluster / 2;
        int sector_index = internal->bpb->reserved_sectors + fat_offset / context->sector_size;

        if (sector_index != internal->loaded_fat_sector_index) {
            res = context->read_sectors(internal->loaded_fat_sector, context->lba_start + sector_index, 2, context->user_data);
            if (res) return res;
            internal->loaded_fat_sector_index = sector_index;
        }

        char* sector_buffer = internal->loaded_fat_sector;

        if (value == fat__END_OF_FILE)
            value = 0xFFF;
        
        uint16_t prev_value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]);
        if (cluster & 1) {
            // check odd or even
            *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) = (value << 4) | (prev_value & 0xF);
        } else {
            *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) = (value & 0xFFF) | (prev_value & 0xF000);
        }

        int wrapped_boundary = (fat_offset % context->sector_size) == (context->sector_size - 1);

        res = context->write_sectors(internal->loaded_fat_sector, context->lba_start + sector_index, 1 + wrapped_boundary, context->user_data);
        if (res) return res;

        uint32_t written_value = fat__read_fat(internal, cluster);
        if (written_value != value && !(written_value == fat__END_OF_FILE && value == 0xFFF)) {
            log("Read did not return written FAT, %u != %u\n", value, written_value);
            trigger_break();
        }
    }

    return 0;
}

int fat__find_free_cluster(fat__InternalContext* internal, int* cluster) {
    int res;

    // log("Find free cluster\n");

    fat__Context* context = internal->context;

    uint32_t total_sectors = internal->bpb->total_sectors_16;
    if (internal->bpb->total_sectors_16 == 0)
        total_sectors = internal->bpb->total_sectors_32;
    uint32_t total_clusters = (total_sectors - internal->bpb->reserved_sectors - internal->sectors_per_fat * internal->bpb->fat_count) / internal->bpb->sectors_per_cluster;

    for (int i = 2; i < total_clusters; i++) {
        uint32_t value = fat__read_fat(internal, i);
        if (value == 0) {
            // log("  found %d\n", i);
            *cluster = i;
            return 0;
        }
    }

    return 1;
}

int fat__split_path(fat__InternalContext* internal, const char* path) {
    int res;

    // Double slash interpreted as single slash
    // Initial slash required
    // "//hello/okay dude//sure.that_IS-a/cake"

    internal->split_count = 0;

    int len = strlen(path);
    if (len == 0)
        return 1;
    int path_data_head = 0;
    int head = 0;
    int prev_start = 0; // points at start of next name
    while (head < len) {
        char chr = path[head];
        head++;

        if (chr == '/' || head >= len) {
            int end;
            if (chr == '/')
                end = head-1;
            else
                end = head;
                
            if (end - prev_start == 0) {
                // double slash
                // skip
                prev_start = head;
                continue;
            }

            int name_len = end - prev_start;
            fat__memcpy(internal->path_data + path_data_head, path + prev_start, name_len);
            internal->path_data[path_data_head + name_len] = '\0';
            internal->split_path[internal->split_count] = internal->path_data + path_data_head;
            internal->split_count++;
            path_data_head += name_len + 1;

            prev_start = head;
        }
    }
    return 0;
}



int fat__create_entry(fat__Context* context, const char* path, fat__DirectoryAttributes attributes) {
    int res;

    int len = fat__strlen(path);
    // Internal structures use 256-byte arrays to keep paths.
    if (len > 255)
        return 1;

    // We assume FAT is valid, i guess we could?

    fat__BPB* bpb = (fat__BPB*) fat__alloc(context, context->sector_size);
    if (!bpb) return fat__ERROR_OUT_OF_MEMORY;
    fat16__EBPB* ebpb16 = (fat16__EBPB*)((char*)bpb + 36);
    fat32__EBPB* ebpb32 = (fat32__EBPB*)((char*)bpb + 36);

    res = context->read_sectors(bpb, context->lba_start, 1, context->user_data);
    if (res) return res;


    fat__InternalContext _internal = {0};
    fat__InternalContext* internal = &_internal;
    internal->context = context;
    internal->bpb = bpb;
    internal->ebpb16 = ebpb16;
    internal->ebpb32 = ebpb32;
    internal->fat_version = fat__detect_type(bpb);

    res = fat__init_internal(internal);
    if (res) return res;

    res = fat__split_path(internal, path);
    if (res) return res;

    
    int dir_cluster;
    res = fat__find_dir_entry(internal, &dir_cluster, 1);
    if (res) return res;

    // log("Create entry in cluster: %d\n", dir_cluster);

    // find free slot
    char* temp_sector = fat__alloc(context, context->sector_size);
    if (!temp_sector) return fat__ERROR_OUT_OF_MEMORY;

    int sector_index = 0; // in cluster

    fat__DirectoryEntry* found_entry = NULL;

    while (!found_entry) {

        if (sector_index >= internal->bpb->sectors_per_cluster) {
            // could not find entry in cluster
            break;
        }

        int sector_offset = fat__cluster_to_sector_offset(internal, dir_cluster) + sector_index;

        res = context->read_sectors(temp_sector, context->lba_start + sector_offset, 1, context->user_data);
        if (res) return res;

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)temp_sector;
        int entries_len = context->sector_size / sizeof(fat__DirectoryEntry);

        for (int i=0;i<entries_len;i++) {
            fat__DirectoryEntry* entry = &entries[i];

            // @TODO Check if we are readding the entry

            if (entry->file_name[0] != 0 && entry->file_name[0] != 0xE5) {
                // in use
                continue;
            }

            sector_index--; // we need to preserve, so we decrement since we decrement after the break
            found_entry = entry;
            break;
        }
        sector_index++;
    }
    
    int free_cluster;
    res = fat__find_free_cluster(internal, &free_cluster);
    if (res) return res;

    res = fat__write_fat(internal, free_cluster, fat__END_OF_FILE);
    if (res) return res;


    found_entry->file_size = 0;
    found_entry->cluster_low = free_cluster & 0xFFFF;
    found_entry->cluster_high = free_cluster >> 16;

    // @TODO Set time from somewhere
    // found_entry->creation_time = 0x3beb;
    // found_entry->creation_date = 0x5b65;
    // found_entry->accessed_date = 0x5b65;
    // found_entry->modified_time = 0x3beb;
    // found_entry->modified_date = 0x5b65;

    
    const char* name = internal->split_path[internal->split_count-1];

    res = fat__insert_name(internal, found_entry, name);
    if (res) return res;

    int sector_offset = fat__cluster_to_sector_offset(internal, dir_cluster) + sector_index;
    res = context->write_sectors(temp_sector, context->lba_start + sector_offset, 1, context->user_data);
    if (res) return res;

    context->working_memory_head = 0;
    return fat__ERROR_SUCCESS;
}


int fat__write_data(fat__Context* context, const char* path, uint64_t offset, void* buffer, uint64_t size) {
    int res;

    if (offset + size >= 0xFFFFFFFFLLU) // 4 GB is max file size
        return 1;

    int len = fat__strlen(path);
    // Internal structures use 256-byte arrays to keep paths.
    if (len > 255)
        return 1;

    // We assume FAT is valid, i guess we could?

    fat__BPB* bpb = (fat__BPB*) fat__alloc(context, context->sector_size);
    if (!bpb) return fat__ERROR_OUT_OF_MEMORY;
    fat16__EBPB* ebpb16 = (fat16__EBPB*)((char*)bpb + 36);
    fat32__EBPB* ebpb32 = (fat32__EBPB*)((char*)bpb + 36);

    res = context->read_sectors(bpb, context->lba_start, 1, context->user_data);
    if (res) return res;

    fat__InternalContext _internal = {0};
    fat__InternalContext* internal = &_internal;
    internal->context = context;
    internal->bpb = bpb;
    internal->ebpb16 = ebpb16;
    internal->ebpb32 = ebpb32;
    internal->fat_version = fat__detect_type(bpb);

    res = fat__init_internal(internal);
    if (res) return res;

    res = fat__split_path(internal, path);
    if (res) return res;

    
    int dir_cluster;
    res = fat__find_dir_entry(internal, &dir_cluster, 0);
    if (res) return res;

    char* temp_sector = fat__alloc(context, context->sector_size);
    if (!temp_sector) return fat__ERROR_OUT_OF_MEMORY;

    int sector_index = 0; // in cluster
    int entries_sector_index = -1;

    fat__DirectoryEntry* found_entry = NULL;

    while (!found_entry) {

        if (sector_index >= internal->bpb->sectors_per_cluster) {
            // could not find entry in cluster
            break;
        }

        int sector_offset = fat__cluster_to_sector_offset(internal, dir_cluster) + sector_index;

        res = context->read_sectors(temp_sector, context->lba_start + sector_offset, 1, context->user_data);
        if (res) return res;

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)temp_sector;
        int entries_len = context->sector_size / sizeof(fat__DirectoryEntry);

        for (int i=0;i<entries_len;i++) {
            fat__DirectoryEntry* entry = &entries[i];
            
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                return 1;
            }
            if (entry->file_name[0] == 0xE5) {
                // empty entry
                continue;
            }

            const char* name = internal->split_path[internal->split_count-1];
            char file_name[256];
            res = fat__sane_cstring(internal, entry, file_name);
            if (res) return 1;

            if (fat__strcmp(name, file_name)) {
                // not equal
                continue;
            }

            entries_sector_index = sector_index; // save it because we want to modify file size of entry later and write the entry to disc
            found_entry = entry;
            break;
        }
        sector_index++;
    }

    if (!found_entry) {
        return 1;
    }

    // Finally time to write data
    uint32_t cluster = ((int)found_entry->cluster_high << 16) | (int)found_entry->cluster_low;
    int advance_clusters = offset / (bpb->bytes_per_sector * bpb->sectors_per_cluster);

    while (advance_clusters) {
        advance_clusters--;
        uint32_t next_cluster = fat__read_fat(internal, cluster);
        if (next_cluster == fat__END_OF_FILE) {
            // Allocate new cluster
            int new_cluster;
            res = fat__find_free_cluster(internal, &new_cluster);
            if (res) return res;

            // @TODO Write zeros to new cluster

            res = fat__write_fat(internal, cluster, new_cluster);
            if (res) return res;
            res = fat__write_fat(internal, new_cluster, fat__END_OF_FILE);
            if (res) return res;

            cluster = new_cluster;
        } else {
            cluster = next_cluster;
        }
    }

    // @TODO Write multiple sectors at once, one whole cluster since it's contiguous. 

    char* data_sector = fat__alloc(context, context->sector_size);
    if (!data_sector) return fat__ERROR_OUT_OF_MEMORY;

    sector_index = 0;
    uint64_t buffer_offset = 0;
    while (buffer_offset < size) {

        if (sector_index >= bpb->sectors_per_cluster) {
            uint32_t next_cluster = fat__read_fat(internal, cluster);
            if (next_cluster == fat__END_OF_FILE) {
                // Allocate new cluster
                int new_cluster;
                res = fat__find_free_cluster(internal, &new_cluster);
                if (res) return res;

                // @TODO Write zeros to new cluster

                res = fat__write_fat(internal, cluster, new_cluster);
                if (res) return res;
                res = fat__write_fat(internal, new_cluster, fat__END_OF_FILE);
                if (res) return res;

                cluster = new_cluster;
                sector_index = 0;
            } else {
                cluster = next_cluster;
                sector_index = 0;
            }
        }

        int sector_offset = fat__cluster_to_sector_offset(internal, cluster) + sector_index;

        int alignment = (context->sector_size + (offset - buffer_offset) % context->sector_size ) % context->sector_size;

        if ((alignment != 0) || (buffer_offset + context->sector_size > size)) {
            // writing a partial sector.
            // We must read the sector
            // memcpy in our partial data to write then
            // do a full sector write
            res = context->read_sectors(data_sector, context->lba_start + sector_offset, 1, context->user_data);
            if (res) return res;

            int part_size = context->sector_size - alignment;
            if (part_size > size - buffer_offset) {
                part_size = size - buffer_offset;
            }
            fat__memcpy(data_sector + alignment, buffer + buffer_offset, part_size);

            res = context->write_sectors(data_sector, context->lba_start + sector_offset, 1, context->user_data);
            if (res) return res;

            buffer_offset += part_size;
            sector_index++;
        } else {
            res = context->write_sectors(buffer + buffer_offset, context->lba_start + sector_offset, 1, context->user_data);
            if (res) return res;

            buffer_offset += context->sector_size;
            sector_index++;
        }
    }

    if (offset + size > found_entry->file_size)
        found_entry->file_size = offset + size;

    int sector_offset = fat__cluster_to_sector_offset(internal, dir_cluster) + entries_sector_index;
    res = context->write_sectors(temp_sector, context->lba_start + sector_offset, 1, context->user_data);
    if (res) return res;

    res = fat__copy_fat(internal);
    if (res) return res;

    return 0;
}

// int fat__read_data(fat__Context* context, const char* path, uint64_t offset, void* buffer, uint64_t size) {

// }


#ifdef ELOS_DEBUG

int fat__print_entry(fat__InternalContext* internal, int sector_start, int sector_end, int* entry_number);

int fat__print_header(fat__Context* context) {
    int res;


    fat__BPB* bpb = (fat__BPB*) fat__alloc(context, context->sector_size);
    if (!bpb) return fat__ERROR_OUT_OF_MEMORY;
    fat16__EBPB* ebpb16 = (fat16__EBPB*)((char*) bpb + 36);

    res = context->read_sectors(bpb, context->lba_start, 1, context->user_data);
    if (res) return res;
    
    fat__InternalContext _internal = { 0 };
    fat__InternalContext* internal = &_internal;

    internal->context = context;
    internal->bpb = bpb;
    internal->ebpb16 = ebpb16;
    internal->fat_version = fat__detect_type(bpb);

    fat__init_internal(internal);

    #define field(X) printf("  " # X ": %d (%x)\n", (int)(X), (int)(X));

    printf("BPB:\n");
    field((*(uint32_t*)&bpb->jmp_short) & 0xFFFFFF)
    printf("  bpb->oem: %.*s\n", 8, bpb->oem);
    field(bpb->bytes_per_sector);
    field(bpb->sectors_per_cluster);
    field(bpb->reserved_sectors);
    field(bpb->fat_count);
    field(bpb->root_dir_entries);
    field(bpb->total_sectors_16);
    field(bpb->media_descriptor_type);
    field(bpb->sectors_per_fat_16);
    field(bpb->sectors_per_track);
    field(bpb->num_heads);
    field(bpb->hidden_sectors);
    field(bpb->total_sectors_32);

    printf("EBPB:\n");
    field(ebpb16->drive_number);
    field(ebpb16->signature);
    field(ebpb16->volume_id);
    printf("  volume_label: %.*s\n", 11, ebpb16->volume_label);
    printf("  system_label: %.*s\n", 8, ebpb16->system_id);

    char* temp_sector = fat__alloc(context, context->sector_size);
    if (!temp_sector) return fat__ERROR_OUT_OF_MEMORY;
    
    int root_dir_lba = internal->bpb->reserved_sectors + internal->bpb->fat_count * internal->bpb->sectors_per_fat_16;

    int entry_number = -1;
    
    res = fat__print_entry(internal, root_dir_lba, root_dir_lba + bpb->root_dir_entries*sizeof(fat__DirectoryEntry)/context->sector_size, &entry_number);
    if (res) return res;

    return 0;
}

int fat__print_entry(fat__InternalContext* internal, int sector_start, int sector_end, int* entry_number) {
    int res;
    fat__Context* context = internal->context;
    int prev_head = context->working_memory_head;

    char* temp_sector = fat__alloc(context, context->sector_size);
    if (!temp_sector) return fat__ERROR_OUT_OF_MEMORY;

    for (int i = sector_start; i < sector_end; i++) {
        res = context->read_sectors(temp_sector, context->lba_start + i, 1, context->user_data);
        if (res) return res;

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)temp_sector;
        for(int j=0;j<512/32;j++) {
            fat__DirectoryEntry* entry = &entries[j];
            (*entry_number)++;

            if (entry->file_name[0] == 0) {
                i = 0x7FFFFFF0;
                break;
            }

            if (entry->file_name[0] == 0 || entry->file_name[0] == 0xE5) {
                printf("entry %d: null\n", (int)(*entry_number));
                continue;
            }

            printf("entry %d\n", (int)(*entry_number));
            printf("  name: %.*s\n", 11, entry->file_name);
            field(entry->attributes)
            field(entry->creation_time_ms)
            field(entry->creation_time)
            field(entry->creation_date)
            field(entry->accessed_date)
            field(entry->cluster_high)
            field(entry->modified_time)
            field(entry->modified_date)
            field(entry->cluster_low)
            field(entry->file_size)
            
            
            if (entry->file_name[0] == '.' && (entry->file_name[1] == ' ' || (entry->file_name[1] == '.' && entry->file_name[2] == ' '))) {
                // Don't follow these recursively
                continue;
            }

            if (entry->attributes != fat__LFN && (entry->attributes & fat__DIRECTORY)) {

                int cluster = (int)entry->cluster_low | ((int)entry->cluster_high << 16);

                int start = internal->bpb->reserved_sectors + internal->bpb->fat_count * internal->bpb->sectors_per_fat_16 + (internal->bpb->root_dir_entries * 32 + internal->bpb->bytes_per_sector - 1 ) /internal->bpb->bytes_per_sector + internal->bpb->sectors_per_cluster * (cluster - 2);
                int end = start + internal->bpb->sectors_per_cluster;

                // @TODO Look up cluster in fat table, the directory entries may continue there

                res = fat__print_entry(internal, start, end, entry_number);
                if (res) return res;
            }
        }
    }
    
    context->working_memory_head = prev_head;
    return 0;
}

#endif // ELOS_DEBUG

#endif // FAT_IMPL
