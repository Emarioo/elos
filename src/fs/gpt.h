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


#ifdef ELOS_DEBUG
    #include <stdio.h>

    #undef log
    #define log(...) printf(__VA_ARGS__)

    int _hits;
    void gpt__trigger_break() {
        _hits++;
    }
#else
    #undef log
    #define log(...)
    #define gpt__trigger_break(...)
#endif

typedef int (*gpt__read_sectors_fn) (void* buffer, uint64_t lba, uint64_t count, void* user_data);
typedef int (*gpt__write_sectors_fn)(const void* buffer, uint64_t lba, uint64_t count, void* user_data);


typedef struct gpt__GUID {
    uint32_t data0;
    uint16_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[6];
} gpt__GUID;



typedef struct gpt__Context {
    // Input parameters
    gpt__read_sectors_fn read_sectors;
    gpt__write_sectors_fn write_sectors;
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
    gpt__ERROR_OUT_OF_MEMORY,
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



typedef struct gpt__Partition {
    gpt__GUID type;
    gpt__GUID unique;
    uint64_t start_lba;
    uint64_t end_lba; // inclusive
    uint64_t attributes;
    uint16_t partition_name[36]; // UTF-16
} gpt__Partition;



/*
    Creates an empty valid GUID Partition Table.
    
    @param total_sectors Is the size of the GUID Partition. It should be the size of the storage device or the max size you need for your raw image

    @param max_partition_entries A suggestion is 16384/128 = 128 (min_size/partition_entry_size). It is the minimum amount specified by UEFI

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
int gpt__create_partition(gpt__Context* context, const gpt__GUID* type_guid, const gpt__GUID* unique_guid, uint64_t start_lba, uint64_t end_lba, const char* name, uint64_t attributes);



/*
    Copies memory. Not ideal? maybe fine?
*/
int gpt__get_partition_by_index(gpt__Context* context, int index, gpt__Partition* out_partition);



/*
    The uniqueness is questionable.
*/
void gpt__generate_unique_guid(gpt__GUID* out_guid);



#define GPT__IS_KNOWN_ERROR(X) ((X) >= 0 && (X) <= gpt__ERROR_MAX)
extern const char* gpt__error_strings[gpt__ERROR_MAX];

extern const gpt__GUID gpt__GUID_EMPTY;
extern const gpt__GUID gpt__GUID_EFI_SYSTEM_PARTITION;



#ifdef ELOS_DEBUG

int gpt__print_header(gpt__Context* context);

#endif // ELOS_DEBUG



#ifdef GPT_IMPL

const char* gpt__error_strings[gpt__ERROR_MAX] = {
    "gpt__ERROR_SUCCESS",
    "gpt__ERROR_UNKNOWN_ERROR",
    "gpt__ERROR_NULL_PARAMETER",
    "gpt__ERROR_MISSING_METHOD",
    "gpt__ERROR_MISSING_SECTOR_SIZE",
    "gpt__ERROR_INVALD_SECTOR_SIZE",
    "gpt__ERROR_MISSING_WORKING_MEMORY",
    "gpt__ERROR_TOO_SMALL_WORKING_MEMORY",
    "gpt__ERROR_WORKING_MEMORY_HEAD_IS_NON_ZERO",
    "gpt__ERROR_CORRUPT_HEADER",
    "gpt__ERROR_MISSING_NAME",
    "gpt__ERROR_TOO_LONG_NAME",
    "gpt__ERROR_PARTITION_ARRAY_FULL",
    "gpt__ERROR_LBA_RANGE_COLLIDES_WITH_ANOTHER_PARTITION",
    "gpt__ERROR_LBA_RANGE_OUTSIDE_VALID_PARTITION_SPACE",
};

const gpt__GUID gpt__GUID_EMPTY = { 0 };
const gpt__GUID gpt__GUID_EFI_SYSTEM_PARTITION = { 0x28732AC1, 0x1FF8, 0xD211, 0x4BBA, { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } }; // C12A7328-F81F-11D2-BA4B-00A0C93EC93B


static void gpt__memzero(void* addr, int size) {
    for(int i=0;i<size/8;i++) {
        ((uint64_t*)addr)[i] = 0;
    }
}
static void gpt__memcpy(void* dst, const void* src, int size) {
    for(int i=0;i<size/4;i++) {
        ((uint32_t*)dst)[i] = ((uint32_t*)src)[i];
    }
}
static int gpt__strlen(const char* str) {
    int len = 0;
    while(str[len]) {
        len++;
    }
    return len;
}

void* gpt__alloc(gpt__Context* context, int size) {
    if (context->working_memory_head + size >= context->working_memory_len)
        return NULL;

    // ensure 8-byte alignment
    if (context->working_memory_head % 8 != 0)
        context->working_memory_head += 8 - size % 8;

    void* ptr = context->working_memory + context->working_memory_head;
    context->working_memory_head += size;

    return ptr;
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
    // if (context->working_memory_len < 0x10000) // 64 KiB
    //     return gpt__ERROR_OUT_OF_MEMORY;
    context->working_memory_head = 0;
    
    if (context->sector_size < 512 || (context->sector_size & (context->sector_size - 1)) != 0)
        return gpt__ERROR_INVALD_SECTOR_SIZE;

    const int MIN_BYTES_PARTITION_ARRAY = 16384; // Specified by UEFI

    int entry_size = 128;

    static gpt__GUID default_disc_guid = { 0 };

    int actual_partition_entries = max_partition_entries;
    // Ensure minimum entries specified by UEFI is used
    if(actual_partition_entries < (MIN_BYTES_PARTITION_ARRAY + entry_size-1) / entry_size)
        actual_partition_entries = (MIN_BYTES_PARTITION_ARRAY + entry_size-1) / entry_size;
    // Sector align entry count (partition entry array)
    if((actual_partition_entries * entry_size) % context->sector_size != 0)
        actual_partition_entries = ((actual_partition_entries * entry_size + context->sector_size-1) / context->sector_size) / entry_size;

    int num_sectors_for_array = (actual_partition_entries * entry_size + context->sector_size - 1) / context->sector_size;
    int header_size = 92;

    gpt__Header* header = (gpt__Header*)gpt__alloc(context, context->sector_size);
    gpt__memzero(header, context->sector_size);

    { // Protective MBR
        char* temp_sector = (char*)header;

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
        
        mbr__PartitionRecord* record = (mbr__PartitionRecord*)(temp_sector + 446);

        record->boot_indicator = 0x00;
        record->starting_chs[0] = 0;
        record->starting_chs[1] = 0x2;
        record->starting_chs[2] = 0;
        record->os_type = 0xEE;
        record->ending_chs[0] = 0xFF;
        record->ending_chs[1] = 0xFF;
        record->ending_chs[2] = 0xFF;
        record->starting_lba = 1;
        record->size_lba = total_sectors - 1;

        *(uint16_t*)(temp_sector+510) = 0xAA55;

        context->write_sectors(temp_sector, 0, 1, context->user_data);
    }

    gpt__memzero(header, context->sector_size);
    
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

    void* empty_sector = (char*)gpt__alloc(context, context->sector_size);
    gpt__memzero(empty_sector, context->sector_size);

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

    context->working_memory_head = 0;

    return gpt__ERROR_SUCCESS;
}


int gpt__create_partition(gpt__Context* context, const gpt__GUID* type_guid, const gpt__GUID* unique_guid, uint64_t start_lba, uint64_t end_lba, const char* name, uint64_t flags) {
    int res;

    int name_len = gpt__strlen(name);

    if(!name)
        return gpt__ERROR_MISSING_NAME;
    if(name_len >= 36)
        return gpt__ERROR_TOO_LONG_NAME;
    

    gpt__Header* header = (gpt__Header*)gpt__alloc(context, context->sector_size);
    res = context->read_sectors(header, 1, 1, context->user_data);
    if (res) return res;

    if (start_lba < header->first_lba)
        return gpt__ERROR_LBA_RANGE_OUTSIDE_VALID_PARTITION_SPACE;
    if (end_lba > header->last_lba)
        return gpt__ERROR_LBA_RANGE_OUTSIDE_VALID_PARTITION_SPACE;


    int batch_size = context->sector_size; // batch_size is always a multiple of sectors. For stability we use one sector, for performance we can choose something higher.
    char* batch = (char*)gpt__alloc(context, context->sector_size);

    int batch_head = 0;
    int array_lba_index = 0;
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
        if (((uint64_t*)&partition->type)[0] == 0 && ((uint64_t*)&partition->type)[1] == 0) {
            // empty partition
            if(!unused_partition) {
                unused_partition = partition;
                break;
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

    if (!unused_partition)
        return gpt__ERROR_PARTITION_ARRAY_FULL;

    partition = unused_partition;

    partition->type = *type_guid;
    partition->unique = *unique_guid;
    partition->start_lba = start_lba;
    partition->end_lba = end_lba;
    partition->attributes = flags;
    gpt__memzero(&partition->partition_name, 72);
    for (int i=0;i<name_len;i++) {
        // UTF-16 <- ASCII
        partition->partition_name[i] = name[i];
    }

    res = context->write_sectors(batch, header->start_lba_entries + array_lba_index, batch_size / context->sector_size, context->user_data);
    if (res) return res;

    // While we might not need to read the backup to get the backup partition entry array,
    // it might be best to do so just to be sure.
    gpt__Header* alt_header = (gpt__Header*)gpt__alloc(context, context->sector_size);
    res = context->read_sectors(alt_header, header->backup_lba, 1, context->user_data);
    if (res) return res;

    res = context->write_sectors(batch, alt_header->start_lba_entries + array_lba_index, batch_size / context->sector_size, context->user_data);
    if (res) return res;

    res = gpt__compute_crc32(context);
    if (res) return res;

    context->working_memory_head = 0;

    return gpt__ERROR_SUCCESS;
}


int gpt__get_partition_by_index(gpt__Context* context, int index, gpt__Partition* out_partition) {
    int res;

    gpt__Header* header = (gpt__Header*)gpt__alloc(context, context->sector_size);
    res = context->read_sectors(header, 1, 1, context->user_data);
    if (res) return res;

    // @TODO Verify GPT header

    char* temp_sector = (char*)gpt__alloc(context, context->sector_size);

    int array_lba_index = header->start_lba_entries;

    int sector_offset = header->start_lba_entries + (index * sizeof(gpt__Partition)) / context->sector_size;
    int part_index    = index % (context->sector_size / sizeof(gpt__Partition));

    // @TODO Check that sector index is less than
    res = context->read_sectors(temp_sector, sector_offset, 1, context->user_data);
    if (res) return res;

    gpt__Partition* partition = (gpt__Partition*)temp_sector + part_index;

    gpt__memcpy(out_partition, partition, sizeof(gpt__Partition));

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
        crc = crc ^ ((uint8_t*)buffer)[i];
        for (int j = 8; j > 0; j--) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return crc;
}

static bool _read_only_crc32;
static bool _enable_last_backup_crc32;
static uint32_t _last_entries_crc32;
static uint32_t _last_header_crc32;
static uint32_t _last_backup_entries_crc32;
static uint32_t _last_backup_header_crc32;

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
        return gpt__ERROR_OUT_OF_MEMORY;
    // if (context->working_memory_len < 0x10000) // 64 KiB
    //     return gpt__ERROR_TOO_SMALL_WORKING_MEMORY;

    if (context->sector_size < 512 || (context->sector_size & (context->sector_size - 1)) != 0)
        return gpt__ERROR_INVALD_SECTOR_SIZE;

    // When calling compute_crc32 we expect the state of GPT table to be incorrect.
    // After all, that's why we want to compute it because we just modified it.

    // We use another function to verify the crc32 fields.
    // However, we do expect the fields to be sane.
    // A header size larger than sector size is not a sane value.
    // @TODO: Add header sanity function, needed since we use entry_size and header_size to compute crc32

    gpt__Header* header = (gpt__Header*)gpt__alloc(context, context->sector_size);

    _enable_last_backup_crc32 = false;

    res = context->read_sectors(header, 1, 1, context->user_data);
    if (res) return res;
    res = gpt__compute_table_crc32(context, header);
    if(res) return res;

    _enable_last_backup_crc32 = true;

    res = context->read_sectors(header, header->backup_lba, 1, context->user_data);
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

    int batch_size = context->sector_size;
    char* batch = (char*)gpt__alloc(context, batch_size);

    int batch_head = 0;
    int array_lba_index = 0;
    int remaining_entries = 0;

    uint32_t crc = 0xFFFFFFFF;
    while (remaining_entries < header->num_entries) {
        remaining_entries++;

        if (batch_head <= 0) {
            res = context->read_sectors(batch, header->start_lba_entries + array_lba_index, 1, context->user_data);
            if (res)
                return res;
            batch_head = batch_size;
            array_lba_index++;
        }

        crc = gpt__crc32(crc, batch + batch_size - batch_head, header->entry_size);
        batch_head -= header->entry_size;
        if (batch_head > batch_size) {
            // @UNREACHABLE, but if we do we blame it on corrupt header
            return gpt__ERROR_CORRUPT_HEADER;
        }
    }
    crc = crc ^ 0xFFFFFFFF;
    if (!_read_only_crc32) {
        header->entries_crc32 = crc;
    }
    if (_enable_last_backup_crc32)
        _last_backup_entries_crc32 = crc;
    else
        _last_entries_crc32 = crc;

    int prev_header_crc32 = header->header_crc32;

    header->header_crc32 = 0;
    
    crc = 0xFFFFFFFF;
    crc = gpt__crc32(crc, header, header->header_size);
    crc = crc ^ 0xFFFFFFFF;
    if (!_read_only_crc32) {
        header->header_crc32 = crc;
    } else {
        header->header_crc32 = prev_header_crc32;
    }
    if (_enable_last_backup_crc32)
        _last_backup_header_crc32 = crc;
    else
        _last_header_crc32 = crc;

    if (!_read_only_crc32) {
        context->write_sectors(header, header->current_lba, 1, context->user_data);
    }

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


#ifdef ELOS_DEBUG


int gpt__print_header(gpt__Context* context) {
    int res;


    gpt__Header* header = (gpt__Header*) gpt__alloc(context, context->sector_size);
    if (!header) return gpt__ERROR_OUT_OF_MEMORY;

    res = context->read_sectors(header, 1, 1, context->user_data);
    if (res) return res;
    

    #define field(X) printf("  " # X ": %d (%x)\n", (int)(X), (int)(X));
    #define field_guid(X) printf("  " # X ": %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\n", (X).data0, (X).data1, (X).data2, (X).data3, (X).data4[0], (X).data4[1], (X).data4[2], (X).data4[3], (X).data4[4], (X).data4[5]);

    _read_only_crc32 = true;

    res = gpt__compute_crc32(context);
    if (res) return res;
    
    _read_only_crc32 = false;

    printf("GPT Header:\n");
    printf("  signature: %.*s\n", (int)sizeof(header->signature), header->signature);
    field(header->revision);
    field(header->header_size);
    field(header->header_crc32);
    field(header->current_lba);
    field(header->backup_lba);
    field(header->first_lba);
    field(header->last_lba);
    field_guid(header->disc_guid);
    field(header->start_lba_entries);
    field(header->num_entries);
    field(header->entry_size);
    field(header->entries_crc32);

    // for (int i=0;i<header->num_entries;i++) {

    // }

    gpt__Header* backup_header = (gpt__Header*) gpt__alloc(context, context->sector_size);
    if (!backup_header) return gpt__ERROR_OUT_OF_MEMORY;

    res = context->read_sectors(backup_header, header->backup_lba, 1, context->user_data);
    if (res) return res;

    
    printf("Backup GPT Header:\n");
    printf("  signature: %.*s\n", (int)sizeof(backup_header->signature), backup_header->signature);
    field(backup_header->revision);
    field(backup_header->header_size);
    field(backup_header->header_crc32);
    field(backup_header->current_lba);
    field(backup_header->backup_lba);
    field(backup_header->first_lba);
    field(backup_header->last_lba);
    field_guid(backup_header->disc_guid);
    field(backup_header->start_lba_entries);
    field(backup_header->num_entries);
    field(backup_header->entry_size);
    field(backup_header->entries_crc32);

    printf("New crc32 calculations:\n         header/entries: %08x/%08x\n  backup header/entries: %08x/%08x\n", _last_header_crc32, _last_entries_crc32, _last_backup_header_crc32, _last_backup_entries_crc32);


    return 0;
}


#endif // ELOS_DEBUG


#endif // GPT_IMPL