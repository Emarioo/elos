/*

    Functions for creating and reading GUID Partition Table

    When creating file with a raw GPT image you can implement methods as file operations.

    All functions return an integer where ZERO means success.
    Non-zero is an error see gpt__Error and implementation for details.

    The 'context' parameter on some functions specifiy:
        - A 'read_sectors' and 'write_sectors' method. You need to provide the implementation for this. If you want to operate on a file then use OS write/read file functions.
           If you are embedding this header in an OS, kernel or if you want to operate on a storage device directly then you can implement
           what you need to read and write sectors to it. You can use 'user_data' for your own context, eg. the file path to perform write/read on.
        - A 'sector_size' field which will be 512 most of the time. It depends on your storage device.
        - 'working_memory' with head and length. No more than 64 KiB is expected. If you embedd this where OS malloc isn't available then providing some
          memory the API can use to store sectors is very necessary. Thread local storage isn't an option. Global variables is problematic in multi-thread contexts.
          You can allocate working_memory using malloc if available or if in OS or kernel just take some memory from somewhere.

    A good read:
    https://uefi.org/specs/UEFI/2.10/05_GUID_Partition_Table_Format.html

*/

#pragma once

#include <stdint.h>
#include <stdbool.h>


typedef int (*read_sectors_fn) (const void* buffer, uint64_t lba, uint64_t count, void* user_data);
typedef int (*write_sectors_fn)(const void* buffer, uint64_t lba, uint64_t count, void* user_data);


typedef struct gpt__GUID {
    uint32_t data0;
    uint16_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[6];
} gpt__GUID;

typedef struct gpt__Context {
    // Input parameters
    read_sectors_fn read_sectors;
    write_sectors_fn write_sectors;
    void* user_data;
    char* working_memory;
    int working_memory_len;
    uint32_t sector_size;

    // Modified by API functions
    int working_memory_head; // Should be initialized to zero 
} gpt__Context;

// Look up implementation for what the errors mean.
typedef enum gpt__Error {
    gpt__ERROR_SUCCESS = 0,
    gpt__ERROR_UNKNOWN_ERROR,
    gpt__ERROR_NULL_PARAMETER,
    gpt__ERROR_MISSING_METHOD,
    gpt__ERROR_MISSING_SECTOR_SIZE,
    gpt__ERROR_INVALD_SECTOR_SIZE,
    gpt__ERROR_MISSING_WORKING_MEMORY,
    gpt__ERROR_TOO_SMALL_WORKING_MEMORY,
    gpt__ERROR_WORKING_MEMORY_HEAD_IS_NON_ZERO,
    gpt__ERROR_CORRUPT_HEADER,
    gpt__ERROR_MISSING_NAME,
    gpt__ERROR_TOO_LONG_NAME,
    gpt__ERROR_PARTITION_ARRAY_FULL,
    gpt__ERROR_LBA_RANGE_COLLIDES_WITH_ANOTHER_PARTITION,
    gpt__ERROR_LBA_RANGE_OUTSIDE_VALID_PARTITION_SPACE,
    gpt__ERROR_MAX,
} gpt__Error;

typedef enum gpt__PartitionFlags {
    gpt_FLAG_REQUIRED_PARTITION   = 0x1, // Can indicate important boot/recovery partition
    gpt_FLAG_NO_BLOCK_IO_PROTOCOL = 0x2, // EFI firmware should ignore this partition
    gpt_FLAG_LEGACY_BIOS_BOOTABLE = 0x3, // Not important, read UEFI GPT spec for info
} gpt__PartitionFlags;

/*
    Creates an empty valid GUID Partition Table.
    
    @param total_sectors Is the size of the GUID Partition. It should be the size of the storage device or the max size you need for your raw image

    @param max_partition_entries A suggestion is 16384/sector_size. It is the minimum amount specified by UEFI

    @param disc_guid Call gpt__generate_unique_guid and pass GUID to this function.
*/
int gpt__overwrite_header(gpt__Context* context, uint64_t total_sectors, int max_partition_entries, gpt__GUID* disc_guid);

/*
    Create a partition.

    @param type_guid Type of partition, eg. gpt__GUID_EFI_SYSTEM_PARTITION
    
    @param unique_guid A unique GUID for this partition. Use gpt__generate_unique_guid.

    @param start_lba Start of partition.

    @param end_lba End of partition (inclusive)

    @param name Name of the partition. Limited to 35 characters.
    
    @param attributes Set based on gpt__PartitionFlags
*/
int gpt__create_partition(gpt__Context* context, gpt__GUID* type_guid, gpt__GUID* unique_guid, uint64_t start_lba, uint64_t end_lba, const char* name, uint64_t attributes);

/*
    The uniqueness is questionable.
*/
void gpt__generate_unique_guid(gpt__GUID* out_guid);


extern const gpt__GUID gpt__GUID_EMPTY;
extern const gpt__GUID gpt__GUID_EFI_SYSTEM_PARTITION;

#ifdef GPT_IMPL

const gpt__GUID gpt__GUID_EMPTY = { 0 };
const gpt__GUID gpt__GUID_EFI_SYSTEM_PARTITION = { 0x28732AC1, 0x1FF8, 0xD211, 0x4BBA, { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } }; // C12A7328-F81F-11D2-BA4B-00A0C93EC93B


static void gpt__memzero(uint64_t* addr, int size) {
    for(int i=0;i<size/8;i++) {
        addr[i] = 0;
    }
}
static void gpt__memcpy(uint32_t* dst, uint32_t* src, int size) {
    for(int i=0;i<size/4;i++) {
        dst[i] = src[i];
    }
}
static int gpt__strlen(const char* str) {
    int len = 0;
    while(str[len]) {
        len++;
    }
    return len;
}

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

typedef struct gpt__Partition {
    gpt__GUID type;
    gpt__GUID unique;
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attributes;
    uint16_t partition_name[36]; // UTF-16
} gpt__Partition;

int gpt__compute_crc32(gpt__Context* context);

int gpt__overwrite_header(gpt__Context* context, uint64_t total_sectors, int max_partition_entries, gpt__GUID* disc_guid) {
    int res;
    if (!context)
        return gpt__ERROR_NULL_PARAMETER;
    if (!context->read_sectors)
        return gpt__ERROR_MISSING_METHOD;
    if (!context->write_sectors)
        return gpt__ERROR_MISSING_METHOD;
    if (!context->sector_size)
        return gpt__ERROR_MISSING_SECTOR_SIZE;
    if (!context->working_memory)
        return gpt__ERROR_MISSING_WORKING_MEMORY;
    if (context->working_memory_len < 0x10000) // 64 KiB
        return gpt__ERROR_TOO_SMALL_WORKING_MEMORY;
    if (context->working_memory_head)
        return gpt__ERROR_WORKING_MEMORY_HEAD_IS_NON_ZERO;
    
    int num_ones=0;
    for(int i=0;i<32;i++)
        num_ones += (context->sector_size>>i) & 1;

    if (context->sector_size < 512 || num_ones != 1)
        return gpt__ERROR_INVALD_SECTOR_SIZE;

    const int MIN_BYTES_PARTITION_ARRAY = 16384; // Specified by UEFI

    int entry_size = 128;

    static gpt__GUID default_disc_guid = { 0 };

    int actual_partition_entries = max_partition_entries;
    // Ensure minimum entries specified by UEFI is used
    if(actual_partition_entries < (MIN_BYTES_PARTITION_ARRAY + context->sector_size-1) / context->sector_size)
        actual_partition_entries = (MIN_BYTES_PARTITION_ARRAY + context->sector_size-1) / context->sector_size;
    // Sector align entry count (partition entry array)
    if((actual_partition_entries * entry_size) % context->sector_size != 0)
        actual_partition_entries = ((actual_partition_entries * entry_size + context->sector_size-1) / context->sector_size) / entry_size;

    int num_sectors_for_array = (actual_partition_entries * entry_size + context->sector_size - 1) / context->sector_size;
    int header_size = 92;

    gpt__Header* header = (gpt__Header*)context->working_memory;
    gpt__memzero(context->working_memory, context->sector_size);
    context->working_memory_head += context->sector_size;
    
    int prime_start = 1;
    int alt_start = total_sectors - 1;

    int prime_start_array = 2;
    int alt_start_array = total_sectors - 2 - num_sectors_for_array;

    int prime_start_alt = alt_start;
    int alt_start_prime = prime_start;

    gpt__memcpy(header->signature, "EFI PART", 8);
    header->revision = 0x10000;        // 1.0
    header->header_size = header_size; // 92 bytes
    header->header_crc32 = 0;          // computed later
    header->reserved0 = 0;
    header->current_lba = 0;           // Set later
    header->backup_lba = 0;            // Set later
    header->first_lba = 2 + num_sectors_for_array;
    header->last_lba = (total_sectors-1) - 2 - num_sectors_for_array;
    header->disc_guid = *disc_guid;
    header->start_lba_entries = 0;     // Set later
    header->num_entries = actual_partition_entries;
    header->entry_size = entry_size;
    header->entries_crc32 = 0;         // computed later
    
    header->current_lba = alt_start;
    header->backup_lba = alt_start_prime;
    header->start_lba_entries = alt_start_array;
    res = context->write_sectors(header, header->current_lba, 1, context->user_data);
    if (res)
        return res;
    
    header->current_lba = prime_start;
    header->backup_lba = prime_start_alt;
    header->start_lba_entries = prime_start_array;
    res = context->write_sectors(header, header->current_lba, 1, context->user_data);
    if (res)
        return res;

    void* empty_sector = ((char*)context->working_memory + context->working_memory_head);
    gpt__memzero(empty_sector, context->sector_size);
    context->working_memory_head += context->sector_size;

    for (int i=0;i<header->num_entries * entry_size / context->sector_size;i++) {
        res = context->write_sectors(empty_sector, prime_start_array + i, 1, context->user_data);
        if (res)
            return res;
        res = context->write_sectors(empty_sector, alt_start_array   + i, 1, context->user_data);
        if (res)
            return res;
    }

    res = gpt__compute_crc32(context);
    if (res)
        return res;

    return gpt__ERROR_SUCCESS;
}


int gpt__create_partition(gpt__Context* context, gpt__GUID* type_guid, gpt__GUID* unique_guid, uint64_t start_lba, uint64_t end_lba, const char* name, uint64_t flags) {
    int res;

    int name_len = gpt__strlen(name);

    if(name)
        return gpt__ERROR_MISSING_NAME;
    if(name_len >= 36)
        return gpt__ERROR_TOO_LONG_NAME;
    

    gpt__Header* header = (gpt__Header*)((char*)context->working_memory + context->working_memory_head);
    context->working_memory_head += context->sector_size;
    res = context->read_sectors(header, 1, 1, context->user_data);
    if (res) return res;

    if (header->first_lba <= start_lba)
        return gpt__ERROR_LBA_RANGE_OUTSIDE_VALID_PARTITION_SPACE;
    if (header->last_lba >= start_lba)
        return gpt__ERROR_LBA_RANGE_OUTSIDE_VALID_PARTITION_SPACE;


    int batch_size = context->sector_size; // batch_size is always a multiple of sectors. For stability we use one sector, for performance we can choose something higher.
    char* batch = (char*)context->working_memory + context->working_memory_head;
    context->working_memory_head += batch_size;

    int batch_head = 0;
    int array_lba_index = header->start_lba_entries;
    int remaining_entries = header->num_entries;

    gpt__Partition* partition = NULL;
    gpt__Partition* unused_partition = NULL;
    while (remaining_entries >= 0) {
        remaining_entries--;

        if (batch_head >= batch_size) {
            res = context->read_sectors(batch, header->start_lba_entries + array_lba_index, 1, context->user_data);
            if (res)
                return res;
            batch_head = 0;
            array_lba_index++;
        }

        gpt__Partition* partition = (gpt__Partition*)(batch + batch_head);
        if (((uint64_t*)&partition->type)[0] == 0 && ((uint64_t*)&partition->type)[1]) {
            // empty partition
            if(!unused_partition) {
                unused_partition = partition;
            }
        } else {
            if (start_lba <= partition->end_lba && end_lba >= partition->start_lba) 
                return gpt__ERROR_LBA_RANGE_COLLIDES_WITH_ANOTHER_PARTITION;
        }


        batch_head += header->entry_size;
        if (batch_head > batch_size) {
            // @UNREACHABLE, but if we do we blame it on corrupt header
            return gpt__ERROR_CORRUPT_HEADER;
        }
    }

    if (!partition)
        return gpt__ERROR_PARTITION_ARRAY_FULL;

    partition->type = *type_guid;
    partition->unique = *unique_guid;
    partition->start_lba = start_lba;
    partition->end_lba = end_lba;
    partition->attributes = flags;
    gpt__memzero(&partition->partition_name, 72);
    for (int i=0;i<name_len;i++) {
        partition->partition_name[i] = name[i];
    }

    context->write_sectors(batch, header->start_lba_entries + array_lba_index, batch_size / context->sector_size, context->user_data);

    // While we might not need to read the backup to get the backup partition entry array,
    // it might be best to do so just to be sure.
    gpt__Header* alt_header = (gpt__Header*)((char*)context->working_memory + context->working_memory_head);
    context->working_memory_head += context->sector_size;
    res = context->read_sectors(alt_header, 1, 1, context->user_data);
    if (res) return res;

    context->write_sectors(batch, alt_header->start_lba_entries + array_lba_index, batch_size / context->sector_size, context->user_data);


    res = gpt__compute_crc32(context);
    if (res) return res;

    return gpt__ERROR_SUCCESS;
}

/*  Usage:
        uint32_t crc = 0xFFFFFFFF;
        crc = gpt__crc32(crc, data, data_len);
        crc = gpt__crc32(crc, data2, data2_len);
        crc = crc ^ 0xFFFFFFFF;
        printf("Final: %x\n", crc);
*/
static uint32_t gpt__crc32(uint32_t crc, const void* const buffer, const uint64_t size) {
    for (int i=0;i<size;i++) {
        crc = crc ^ ((char*)buffer)[i];
        for (int j = 8; j > 0; j--) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc % 1)));
        }
    }
    return crc;
}

// computes for one table
int gpt__compute_table_crc32(gpt__Context* context, gpt__Header* header);

// Computes for primary and secondary table
int gpt__compute_crc32(gpt__Context* context) {
    int res;
    if (!context)
        return gpt__ERROR_NULL_PARAMETER;
    if (!context->read_sectors)
        return gpt__ERROR_MISSING_METHOD;
    if (!context->write_sectors)
        return gpt__ERROR_MISSING_METHOD;
    if (!context->sector_size)
        return gpt__ERROR_MISSING_SECTOR_SIZE;
    if (!context->working_memory)
        return gpt__ERROR_TOO_SMALL_WORKING_MEMORY;
    if (context->working_memory_len < 0x10000) // 64 KiB
        return gpt__ERROR_TOO_SMALL_WORKING_MEMORY;
    
    int num_ones=0;
    for(int i=0;i<32;i++)
        num_ones += (context->sector_size>>i) & 1;

    if (context->sector_size < 512 || num_ones != 1)
        return gpt__ERROR_INVALD_SECTOR_SIZE;

    // When calling compute_crc32 we expect the state of GPT table to be incorrect.
    // After all, that's why we want to compute it because we just modified it.

    // We use another function to verify the crc32 fields.
    // However, we do expect the fields to be sane.
    // A header size larger than sector size is not a sane value.
    // @TODO: Add header sanity function, needed since we use entry_size and header_size to compute crc32

    gpt__Header* header = (gpt__Header*)context->working_memory;
    context->working_memory_head += context->sector_size;

    res = context->read_sectors(context->working_memory, 1, 1, context->user_data);
    if (res) return res;
    res = gpt__compute_table_crc32(context, header);
    if(res) return res;

    res = context->read_sectors(context->working_memory, header->backup_lba, 1, context->user_data);
    if (res) return res;
    res = gpt__compute_table_crc32(context, header);
    if(res) return res;

    return gpt__ERROR_SUCCESS;
}


int gpt__compute_table_crc32(gpt__Context* context, gpt__Header* header) {
    int res;
    // TODO: Perform some sanity checks on fields in header.
    //   For example, if header->num_entries is 8942141 we will be stuck in a loop for a while.
    /*
        header->entry_size == 128 || header->entry_size == 256 || header->entry_size == 512
    */

    int prev_working_head = context->working_memory_head;

    int batch_size = context->sector_size;
    char* batch = (char*)context->working_memory + context->working_memory_head;
    context->working_memory_head += batch_size;

    int batch_head = 0;
    int array_lba_index = 0;
    int remaining_entries = header->num_entries;

    uint32_t crc = 0xFFFFFFFF;
    while (remaining_entries >= 0) {
        remaining_entries--;

        if (batch_head >= batch_size) {
            res = context->read_sectors(batch, header->start_lba_entries + array_lba_index, 1, context->user_data);
            if (res)
                return res;
            batch_head = 0;
            array_lba_index++;
        }

        crc = gpt__crc32(crc, batch + batch_head, header->entry_size);
        batch_head += header->entry_size;
        if (batch_head > batch_size) {
            // @UNREACHABLE, but if we do we blame it on corrupt header
            return gpt__ERROR_CORRUPT_HEADER;
        }
    }
    crc = crc ^ 0xFFFFFFFF;
    header->entries_crc32 = crc;
    
    crc = 0xFFFFFFFF;
    crc = gpt__crc32(crc, header, header->header_size);
    crc = crc ^ 0xFFFFFFFF;
    header->header_crc32 = crc;

    context->write_sectors(header, header->current_lba, 1, context->user_data);

    context->working_memory_head = prev_working_head;

    return gpt__ERROR_SUCCESS;
}

uint64_t gpt__xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

void gpt__generate_unique_guid(gpt__GUID* out_guid) {
    uint64_t timestamp;

    // x86-64 specific :(
    asm ( 
        "rdtsc\n"
        "shl $32, %%rdx\n"
        "or %%rdx, %%rax\n"
        "mov %%rax, %0\n"
        : "=r" (timestamp)
        :
        : "rax", "rdx"
    );

    ((uint64_t*)out_guid)[0] = gpt__xorshift64(&timestamp);
    ((uint64_t*)out_guid)[1] = gpt__xorshift64(&timestamp);
}


#endif