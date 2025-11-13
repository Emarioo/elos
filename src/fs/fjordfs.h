/*
    Experimental simple file system

    Support:
        Paths, files, directories  (duuh)
        
    Extra?
        Symlinks
        Permissions (owner, group, )
        Shrink file system (partition it's on)
        

    All fields are little endian.
    All fields follow standard C struct field alignment
*/

#pragma once

#include <stdint.h>

/*
    Start of the header
*/
typedef struct fjord__Header {
    uint8_t  _reserved0[4];         // Not used, reserved for short jump
    uint32_t  magic;                // FJRD, 0x44524a46 (little endian)
    uint16_t fs_version;            // 0
    uint16_t flags;                 // not used
    
    uint64_t total_bytes;           // Total size of the file system in byte (size of the partition the FS is on, sector_count * sector_size)
    

    // fill out sector with zeros.
    // last 2 bytes may be 0x55 0xAA to indicate boot sector
} fjord__Header;

typedef struct fjord__NodeMetadata {
    uint32_t byte_size;
    uint8_t flags; // IS_DIRECTORY
    uint8_t flags; // PERMISIONS
    uint8_t reserved[4]; // optional short jump
    uint8_t magic[4];
    uint16_t fs_version;
    uint16_t sector_per_cluster;

    // fill out sector with zeros.
    // last 2 bytes may be 0x55 0xAA to indicate boot sector
} fjord__NodeMetadata;


typedef struct fjord__FileInfo {
    uint64_t size;
    int flags;

    uint32_t _reserved0[12 + 3 + 1]; // 12 for creation, last access, last write time, 3+1 for owner,group,other and permission flags.
} fjord__FileInfo;


typedef int (*gpt__read_sectors_fn) (const void* buffer, uint64_t lba, uint64_t count, void* user_data);
typedef int (*gpt__write_sectors_fn)(const void* buffer, uint64_t lba, uint64_t count, void* user_data);

typedef struct fjord__Context {
    gpt__read_sectors_fn read_sectors;
    gpt__write_sectors_fn write_sectors;
    void* user_data;

    char* working_memory;
    int working_memory_len;
    int working_memory_head; // Should be initialized to zero 

    uint64_t lba_start;
    uint64_t lba_end;
    uint32_t sector_size;
} fjord__Context;

int fjord__overwrite_header(fjord__Context* context, uint64_t total_bytes);

int fjord__create_file(fjord__Context* context, const char* path, int flags);

int fjord__remove_file(fjord__Context* context, const char* path);

int fjord__info_file(fjord__Context* context, const char* path, fjord__FileInfo* out_info);

int fjord__read_file(fjord__Context* context, const char* path, uint64_t offset, void* buffer, uint64_t size);

int fjord__write_file(fjord__Context* context, const char* path, uint64_t offset, void* buffer, uint64_t size);

int fjord__list_files(fjord__Context* context, const char* path, void* buffer, uint64_t size);


#ifdef FJORD_IMPL

static void fjord__memzero(void* addr, int size) {
    for(int i=0;i<size;i++) {
        ((char*)addr)[i] = 0;
    }
}
static void fjord__memcpy(void* dst, void* src, int size) {
    for(int i=0;i<size;i++) {
        ((char*)dst)[i] = ((char*)src)[i];
    }
}
static int fjord__strlen(const char* str) {
    int len = 0;
    while(str[len]) {
        len++;
    }
    return len;
}
static int fjord__strcmp(const char* a, const char* b) {
    int head = 0;
    while (a[head] || b[head]) {
        if (a[head] != b[head]) {
            return (a[head] < b[head]) + (a[head] > b[head]);
        }
        head++;
    }
    return 0;
}

void* fjord__alloc(fjord__Context* context, int size) {
    if (context->working_memory_head + size >= context->working_memory_len)
        return NULL;

    // ensure 8-byte alignment
    if (context->working_memory_head % 8 != 0)
        context->working_memory_head += 8 - size % 8;

    void* ptr = context->working_memory + context->working_memory_head;
    context->working_memory_head += size;

    return ptr;
}

int fjord__overwrite_header(fjord__Context* context, uint64_t total_bytes) {
    int res;

    fjord__Header* header = fjord__alloc(context, context->sector_size);
    fjord__memzero(header, context->sector_size);

    header->magic = 0x44524a46;
    header->fs_version = 0;
    header->flags = 0;

    header->total_bytes = total_bytes;

    res = context->write_sectors(header, context->lba_start, 1, context->user_data);
    if (res) return res;

    return 0;
}


int fjord__create_file(fjord__Context* context, const char* path, int flags) {
    int res;

    // Split path

    // Find directory to put file in
    // khkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkfnljljljljkjkjkjkjkjddd__khkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkhkfnljljljljkjkjkjkjkjddd__

    // RootDir* dir = disc + 1 // second sector

    // int index = 0
    // while index < split_path.len - 1 {
    //     dir = find_name(dir, split_path[index])
    // }

    


}

#endif
