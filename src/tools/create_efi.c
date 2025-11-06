/*
    Read a specification
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fs/gpt.h"
#include "fs/fat.h"

#define SECTOR_SIZE 512


typedef struct Context {
    const char* out_path;
    FILE* file;
} Context;

int read_sectors(const char* buffer, uint64_t lba, uint64_t count, void* user_data) {
    int res;
    Context* ctx = user_data;

    res = fseek(ctx->file, SECTOR_SIZE * lba, SEEK_SET);
    if (res) return 1;
    res = fread(buffer, SECTOR_SIZE, count, ctx->file);
    if (res) return 1;

    return 0;
}
int write_sectors(const char* buffer, uint64_t lba, uint64_t count, void* user_data) {
    int res;
    Context* ctx = user_data;

    res = fseek(ctx->file, SECTOR_SIZE * lba, SEEK_SET);
    if (res) return 1;
    res = fwrite(buffer, SECTOR_SIZE, count, ctx->file);
    if (res) return 1;

    return 0;
}

#define CHECK\
    if (res) { printf("ERROR: %d\n", res); }

int main(int argc, const char* argv) {
    int res;
    // parse arguments
    // get files
    // put files into FAT32
    
    
    Context context = { 0 };
    context.out_path = "gpt.img";
    context.file = fopen(context.out_path, "+wb");


    gpt__Context gpt_context = {
        .read_sectors = read_sectors,
        .write_sectors = write_sectors,
        .user_data = &context,
        .sector_size = SECTOR_SIZE,
        .working_memory = malloc(0x10000),
        .working_memory_len = 0x10000,
        .working_memory_head = 0,
    };
    #define MiB 0x100000
    // Count file size
    int total_size = 8 * MiB;

    gpt__GUID tmp;
    gpt__generate_unique_guid(&tmp);
    
    res = gpt__overwrite_header(&gpt_context, total_size/SECTOR_SIZE, 32, &tmp);
    CHECK

    gpt__generate_unique_guid(&tmp);

    uint64_t lba_start = 128;
    uint64_t lba_end = lba_start + (total_size - 2 *  MiB)/SECTOR_SIZE;

    res = gpt__create_partition(&gpt_context, &gpt__GUID_EFI_SYSTEM_PARTITION, &tmp, lba_start, lba_end-1, "EFI Boot System", 0);
    CHECK


    // Now we want to fill the partition with a file system

    fat32__Context fat_context = {
        .read_sectors = read_sectors,
        .write_sectors = write_sectors,
        .user_data = &context,
        .sector_size = SECTOR_SIZE,
        .working_memory = malloc(0x10000),
        .working_memory_len = 0x10000,
        .working_memory_head = 0,
        
        // The fat32 API needs you to specify which partition to 
        // create/read fat32 file system to/from.
        // Rather than providing index to GPT entry or GUID we 
        // give LBA range.
        .lba_start = lba_start,
        .lba_end = lba_end,
    };

    res = fat32__overwrite_header(&fat_context);
    CHECK

    res = fat32__create_file(&fat_context, "name", fat32__CREATE);
    CHECK

    res = fat32__write_file(&fat_context, "name", fat32__CREATE);
    CHECK


    fclose(context.file);

    return 0;
}