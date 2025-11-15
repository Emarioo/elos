/*
    Read a specification
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define GPT_IMPL
#define FAT_IMPL

#include "fs/gpt.h"
#include "fs/fat.h"

#define SECTOR_SIZE 512


typedef struct Context {
    const char* out_path;
    FILE* file;
} Context;

int read_sectors(void* buffer, uint64_t lba, uint64_t count, void* user_data) {
    size_t res;
    Context* ctx = user_data;

    // printf("Read %lu %lu\n", lba, count);

    res = fseek(ctx->file, SECTOR_SIZE * lba, SEEK_SET);
    if (res) return 1;
    res = fread(buffer, 1, SECTOR_SIZE * count, ctx->file);
    if (res != SECTOR_SIZE * count) {
        memset(buffer + res, 0, SECTOR_SIZE - res);
    }

    return 0;
}
char _zero_buffer[4096];
int write_sectors(const void* buffer, uint64_t lba, uint64_t count, void* user_data) {
    size_t res;
    Context* ctx = user_data;

    // printf("Write %lu %lu\n", lba, count);

    fseek(ctx->file, 0, SEEK_END);
    uint64_t file_size = ftell(ctx->file);

    // printf("%lu %lu %lu\n", file_size, lba, count);

    if (file_size < lba * SECTOR_SIZE + count * SECTOR_SIZE) {
        int len = lba * SECTOR_SIZE + count * SECTOR_SIZE - file_size;
        int left = len;
        while (left) {
            int chunk_size = (left > 4096) ? 4096 : left;
            fwrite(_zero_buffer, 1, chunk_size, ctx->file);
            left -= chunk_size;
            // if (len > 100000){
            //     printf("%d %d\n", i, len);
            //     assert(false);
            // }
            // fputc(0, ctx->file);
        } 
    }

    res = fseek(ctx->file, SECTOR_SIZE * lba, SEEK_SET);
    if (res) return 1;
    res = fwrite(buffer, SECTOR_SIZE, count, ctx->file);
    if (res != count) return 1;

    fflush(ctx->file);

    return 0;
}

#define CHECK\
    if (res) { printf("ERROR: %d\n", res); return 1; }

#define CHECK_GPT\
    if (res) { printf("ERROR: %d, %s\n", res, GPT__IS_KNOWN_ERROR(res) ? gpt__error_strings[res] : ""); return 1; }

int copy_file(fat__Context* context, const char* src_path, const char* dst_path) {
    int res;
    res = fat__create_entry(context, dst_path, 0);
    if (res) return res;

    FILE* file = fopen(src_path, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* file_data = malloc(file_size);
    assert(file_data);
    fread(file_data, 1, file_size, file);
    fclose(file);

    res = fat__write_data(context, dst_path, 0, file_data, file_size);
    if (res) return res;

    free(file_data);

    return 0;
}


int print();

void print_usage() {
    #define TOOL_NAME "elos-img"
    printf(
        TOOL_NAME " lets you create FAT and GPT images.\n"
        "\n"
        "Commands:\n"
        "  --file <path>        Picks file to work on\n"
        "  --gpt-init <size>    Formats file with GPT\n"
        "  --gpt-partition-init <index> <start> <end>\n"
        "      Overwrites partition 'index'\n"
        "  --gpt-partition-init-from-file <index> <start> <end> <file>  Creates partition with file content\n"
        "  --partition <index>  Picks partition to work on\n"
        "\n"
        "  --fat-init <size>     Formats file or partition in file with FAT\n"
        "  --fat-add-dir <dst>   Add directory\n"
        "  --fat-copy-file <src> <dst>  Copies file into FAT\n"
        "\n"
        "  --fat-print           Print header information\n"
        "  --gpt-print           Print header information\n"
        "\n"
        "Example:\n"
        "  "TOOL_NAME" --file os.img --gpt-init 16M\n"
        "  "TOOL_NAME" --file os.img --gpt-partition-init 0 64K 13M\n"
        "  "TOOL_NAME" --file os.img --partition 0 --fat-init 12M\n"
        "  "TOOL_NAME" --file os.img --partition 0 --fat-copy-file bin/bootx64.efi /EFI/BOOT/BOOTX64.EFI\n"
        "\n"
    );
}

enum CMDKind {
    CMD_NONE,
    CMD_GPT_INIT,
    CMD_GPT_PARTITION_INIT,
    CMD_GPT_PARTITION_INIT_COPY_FILE,
    CMD_FAT_INIT,
    CMD_FAT_ADD_DIR,
    CMD_FAT_COPY_FILE,
    CMD_FAT_PRINT,
    CMD_GPT_PRINT,
};

typedef struct Options {
    int          operation;
    const char*  work_file;
    int          work_partition;
    uint64_t     size;
    int          index;
    uint64_t     start;
    uint64_t     end;
    const char*  dst;
    const char*  src;
    const char*  copy_path;
} Options;

uint64_t multiplier(char chr) {
    switch(chr | 32) {
        case 'k': return 1024;
        case 'm': return 1024*1024;
        case 'g': return 1024*1024*1024;
        case 's': return SECTOR_SIZE;
    }
    return 1;
}

static int _string_buffer_head;
static char _string_buffer[4096];

const char* replace_backslash(const char* path) {
    int len = strlen(path);

    if (_string_buffer_head + len + 1 > sizeof(_string_buffer)) {
        return NULL;
    }

    char* ptr = _string_buffer + _string_buffer_head;
    _string_buffer_head += len + 1;
    memcpy(ptr, path, len);
    ptr[len] = '\0';
    for (int i=0;i<len;i++) {
        if (ptr[i] == '\\')
            ptr[i] = '/';
    }
    return ptr;
}

int parse_arguments(int argc, const char* argv[], Options* out_options) {
    memset(out_options, 0, sizeof(Options));
    out_options->work_partition = -1;
    out_options->index = -1;

    int argi = 1;
    while (argi < argc) {
        const char* arg = argv[argi];
        argi++;

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(arg, "--file") == 0) {
            if (argi >= argc) {
                printf("Missing argument for '%s'\n", arg);
                return 1;
            }
            out_options->work_file = replace_backslash(argv[argi]);
            argi++;
        } else if (strcmp(arg, "--partition") == 0) {
            if (argi >= argc) {
                printf("Missing argument for '%s'\n", arg);
                return 1;
            }
            out_options->work_partition = atoi(argv[argi]);
            argi++;
        } else if (strcmp(arg, "--gpt-init") == 0) {
            if (argi >= argc) {
                printf("Missing argument for '%s'\n", arg);
                return 1;
            }
            out_options->operation = CMD_GPT_INIT;
            char* endptr;
            // @TODO We swallow trailing letter when they don't match supported multipliers.
            //    User thinks "Oh, 240KS for 240*1024 sectors is supported" but no, we just handle G,M,K,S suffix, no combination.
            out_options->size = strtoul(argv[argi], &endptr, 10);
            out_options->size *= multiplier(*endptr);
            argi++;
        } else if (strcmp(arg, "--gpt-partition-init") == 0) {
            if (argi + 2 >= argc) {
                printf("Missing arguments for '%s'\n", arg);
                return 1;
            }
            out_options->operation = CMD_GPT_PARTITION_INIT;
            out_options->index = atol(argv[argi]);
            argi++;
            char* endptr;
            out_options->start = strtoul(argv[argi], &endptr, 10);
            out_options->start *= multiplier(*endptr);
            argi++;
            out_options->end = strtoul(argv[argi], &endptr, 10);
            out_options->end *= multiplier(*endptr);
            argi++;
        } else if (strcmp(arg, "--gpt-partition-init-from-file") == 0) {
            if (argi + 3 >= argc) {
                printf("Missing arguments for '%s'\n", arg);
                return 1;
            }
            out_options->operation = CMD_GPT_PARTITION_INIT_COPY_FILE;
            out_options->index = atol(argv[argi]);
            argi++;
            char* endptr;
            out_options->start = strtoul(argv[argi], &endptr, 10);
            out_options->start *= multiplier(*endptr);
            argi++;
            out_options->end = strtoul(argv[argi], &endptr, 10);
            out_options->end *= multiplier(*endptr);
            argi++;
            out_options->copy_path = replace_backslash(argv[argi]);
            argi++;
        } else if (strcmp(arg, "--fat-init") == 0) {
            if (argi >= argc) {
                printf("Missing argument for '%s'\n", arg);
                return 1;
            }
            out_options->operation = CMD_FAT_INIT;
            char* endptr;
            out_options->size = strtoul(argv[argi], &endptr, 10);
            out_options->size *= multiplier(*endptr);
            argi++;
        } else if (strcmp(arg, "--fat-add-dir") == 0) {
            if (argi >= argc) {
                printf("Missing argument for '%s'\n", arg);
                return 1;
            }
            out_options->operation = CMD_FAT_ADD_DIR;
            out_options->dst = replace_backslash(argv[argi]);
            argi++;
        } else if (strcmp(arg, "--fat-copy-file") == 0) {
            if (argi + 1 >= argc) {
                printf("Missing arguments for '%s'\n", arg);
                return 1;
            }
            out_options->operation = CMD_FAT_COPY_FILE;
            out_options->src = replace_backslash(argv[argi]);
            argi++;
            out_options->dst = replace_backslash(argv[argi]);
            argi++;
        } else if (strcmp(arg, "--fat-print") == 0) {
            out_options->operation = CMD_FAT_PRINT;
        } else if (strcmp(arg, "--gpt-print") == 0) {
            out_options->operation = CMD_GPT_PRINT;
        } else {
            fprintf(stderr, "Unknown argument '%s', use a flag first, see --help\n", arg);
            return 1;
        }
    }
    return 0;
}

void parse_command_line(const char* cmd, const char*** argv, int* argc) {

}

int main(int argc, const char* argv[]) {
    int res;

    if (argc <= 1) {
        print_usage();
        return 0;
    }

    // parse_command_line("--file gpt.img --fat-init 8M", &argv, &argc);
    // parse_command_line("--file gpt.img --fat-copy-file bin/bootx86.efi /EFI/BOOT/BOOTX64.EFI", &argv, &argc);

    Options options = {0};
    parse_arguments(argc, argv, &options);

    const char* rw_mode = "rb+";
    if (options.operation == CMD_GPT_INIT || (options.operation == CMD_FAT_INIT && options.work_partition == -1)) {
        rw_mode = "wb+";
    }

    Context context = { 0 };
    context.out_path = options.work_file;
    context.file = fopen(options.work_file, rw_mode);
    if (!context.file) {
        printf("Could not open '%s' for create, read, and write\n", options.work_file);
        return 1;
    }

    gpt__Context gpt_context = {
        .read_sectors = read_sectors,
        .write_sectors = write_sectors,
        .user_data = &context,
        .sector_size = SECTOR_SIZE,
        .working_memory = malloc(0x10000),
        .working_memory_len = 0x10000,
        .working_memory_head = 0,
    };

    uint64_t lba_start = 0;
    uint64_t lba_end = 0;

    if (options.work_partition != -1) {
        gpt__Partition partition;
        res = gpt__get_partition_by_index(&gpt_context, options.index, &partition);
        if (res) {
            printf("Invalid partition index or invalid GPT file.\n");
            return 1;
        }
        lba_start = partition.start_lba;
        lba_end = partition.end_lba;
    }

    // Now we want to fill the partition with a file system
    fat__Context fat_context = {
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


    /*
        What's wrong:
            crc32 calculations are wrong, print headers and see.
            use my crc32 calculations on gpt from mtools
    */


    switch(options.operation) {
        case CMD_GPT_INIT: {
            gpt__GUID tmp;
            gpt__generate_unique_guid(&tmp);
            
            res = gpt__overwrite_header(&gpt_context, options.size/SECTOR_SIZE, 32, &tmp);
            CHECK_GPT
        } break;
        case CMD_GPT_PARTITION_INIT: {
            gpt__GUID tmp;
            gpt__generate_unique_guid(&tmp);

            uint64_t lba_start = options.start;
            uint64_t lba_end = options.end;

            // @TODO gpt__create_partition should let you specify index.
            //    Otherwise we need to work with GUID on the command line instead of index which is annoying.

            res = gpt__create_partition(&gpt_context, &gpt__GUID_EFI_SYSTEM_PARTITION, &tmp, lba_start, lba_end-1, "EFI Boot System", 0);
            CHECK_GPT
        } break;
        case CMD_GPT_PARTITION_INIT_COPY_FILE: {
            gpt__GUID tmp;
            gpt__generate_unique_guid(&tmp);

            uint64_t lba_start = options.start;
            uint64_t lba_end = options.end;

            // @TODO gpt__create_partition should let you specify index.
            //    Otherwise we need to work with GUID on the command line instead of index which is annoying.

            res = gpt__create_partition(&gpt_context, &gpt__GUID_EFI_SYSTEM_PARTITION, &tmp, lba_start, lba_end-1, "EFI Boot System", 0);
            CHECK_GPT

            int res;

            FILE* file = fopen(options.copy_path, "rb");
            assert(file);

            fseek(file, 0, SEEK_END);
            size_t file_size = ftell(file);
            fseek(file, 0, SEEK_SET);

            if (file_size > gpt_context.sector_size * (lba_end - lba_start)) {
                fprintf(stderr, "LBA start end too small (%lu .. %lu, %lu bytes), file size is %lu bytes.\n", lba_start, lba_end, 512*(lba_end - lba_start), file_size);
                return 1;
            }

            void* file_data = calloc(file_size + gpt_context.sector_size, 1);
            assert(file_data);
            fread(file_data, 1, file_size, file);
            fclose(file);

            res = write_sectors(file_data, lba_start, file_size / gpt_context.sector_size, gpt_context.user_data);
            CHECK

            free(file_data);
        } break;
        case CMD_FAT_INIT: {
            if (!options.work_file) {
                printf("Missing --file\n");
                return 1;
            }

            if (options.work_partition != -1) {
                uint64_t partition_size = (fat_context.lba_end - fat_context.lba_start) * SECTOR_SIZE;
                if (options.size < partition_size) {
                    printf("Partition %d is smaller than the size to init FAT with (partition: %lu, wanted fat size: %lu)\n", options.work_partition, partition_size, options.size);
                    return 1;
                }
            } else {
                fat_context.lba_start = 0;
                fat_context.lba_end = (options.size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            }

            res = fat__overwrite_header(&fat_context);
            CHECK
        } break;
        case CMD_FAT_ADD_DIR: {
            printf("--fat-add-dir not implemented!\n");
            return 1;
        } break;
        case CMD_FAT_COPY_FILE: {
            res = copy_file(&fat_context, options.src, options.dst);
            CHECK
        } break;
        case CMD_FAT_PRINT: {
            res = fat__print_header(&fat_context);
            CHECK
        } break;
        case CMD_GPT_PRINT: {
            res = gpt__print_header(&gpt_context);
            CHECK_GPT
        } break;
        default:
            print_usage();
    }
    
    fclose(context.file);

    return 0;
}

// #ifdef ELOS_DEBUG

// int print() {
//     int res;

//     // gpt__Context gpt_context = {
//     //     .read_sectors = read_sectors,
//     //     .write_sectors = write_sectors,
//     //     .user_data = &context,
//     //     .sector_size = SECTOR_SIZE,
//     //     .working_memory = malloc(0x10000),
//     //     .working_memory_len = 0x10000,
//     //     .working_memory_head = 0,
//     // };
//     #define MiB 0x100000
//     // // Count file size
//     int total_size = 8 * MiB;

    
//     Context context = { 0 };
//     // context.out_path = "bin/int/fat.img";
//     context.out_path = "gpt.img";
//     context.file = fopen(context.out_path, "r");

//     assert(context.file);

//     // printf("Things to fix?\n");
//     // printf("  Missing . .. entries, probably fine though\n");
//     // printf("  Creation access times are zero, might not like that?\n");
//     // printf("  fat12/fat16 calculation is wrong, we use fat16 when linux detects it as fat12?\n");
//     // return;


//     fseek(context.file, 0, SEEK_END);
//     uint64_t file_size = ftell(context.file);
//     fseek(context.file, 0, SEEK_SET);
    
//     uint64_t lba_start = 0;
//     uint64_t lba_end = lba_start + (file_size)/SECTOR_SIZE;

//     fat__Context fat_context = {
//         .read_sectors = read_sectors,
//         .write_sectors = write_sectors,
//         .user_data = &context,
//         .sector_size = SECTOR_SIZE,
//         .working_memory = malloc(0x10000),
//         .working_memory_len = 0x10000,
//         .working_memory_head = 0,
        
//         // The fat32 API needs you to specify which partition to 
//         // create/read fat32 file system to/from.
//         // Rather than providing index to GPT entry or GUID we 
//         // give LBA range.
//         .lba_start = lba_start,
//         .lba_end = lba_end,
//     };

//     res = fat__print_header(&fat_context);
//     CHECK

// }

// #endif // ELOS_DEBUG
