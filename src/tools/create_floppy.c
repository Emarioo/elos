#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// #include "elos/fat12.h"
#include "make_fat.h"

#define Assert(X) ((X) || (fprintf(stderr,"%s:%d. %s",__FILE__,__LINE__, #X), *(char*)0 = 0))

void create_fat32();

int main() {
    // create_fat32();

    // return 0;

    const char* path_out_floppy = "bin/elos.bin";
    const char* path_boot = "bin/elos/bootloader.bin";
    const char* path_setup = "bin/elos/setup.bin";
    const char* path_kernel = "bin/elos/kernel.bin";

    FILE* file_boot = fopen(path_boot, "rb");
    Assert(file_boot);
    fseek(file_boot, 0, SEEK_END);
    int file_size_boot = ftell(file_boot);
    fseek(file_boot, 0, SEEK_SET);
    
    FILE* file_setup = fopen(path_setup, "rb");
    Assert(file_setup);
    fseek(file_setup, 0, SEEK_END);
    int file_size_setup = ftell(file_setup);
    fseek(file_setup, 0, SEEK_SET);
    char* data_setup = malloc(file_size_setup);
    fread(data_setup, 1, file_size_setup, file_setup);

    int data_size = 1440 * 1024;
    char* data = malloc(data_size);
    memset(data, 0xCD, data_size);

    fread(data + 0, 1, file_size_boot, file_boot);
    printf("wrote %d bytes of bootloader\n", file_size_boot);

    // FILE* file_kernelc = fopen(path_kernelc, "rb");
    // Assert(file_kernelc);
    // fseek(file_kernelc, 0, SEEK_END);
    // int file_size_kernelc = ftell(file_kernelc);
    // fseek(file_kernelc, 0, SEEK_SET);
    // char* data_kernelc = malloc(file_size_kernelc);
    // fread(data_kernelc, 1, file_size_kernelc, file_kernelc);

    fat12_init_table(data);

    int root_entry_index = fat12_create_entry(data, "SETUP.BIN", 0);
    fat12_write_file(data, root_entry_index, 0, data_setup, file_size_setup);
    printf("wrote %d bytes of setup\n", file_size_setup);
    
    // int kernelc_root_entry_index = fat12_create_entry(data, "KERNELC.BIN", 0);
    // fat12_write_file(data, kernelc_root_entry_index, 0, data_kernelc, file_size_kernelc);
    // printf("wrote %d bytes of kernel\n", file_size_kernel);

    FILE* file_floppy = fopen(path_out_floppy,"wb");
    fwrite(data, 1, data_size, file_floppy);
    fclose(file_floppy);

    printf("wrote floppy.img\n");

    return 0;
}

void create_fat32() {
    int res;
    FAT32_object* fat = fat32_create();
    int fat_file_size = 1024*1024;
    
    int fd = fat32_open(fat, "SETUP.BIN");
    assert(fd != -1);
    
    uint64_t written = fat32_write(fat, fd, "Hello sailor\n", 13);
    assert(written == 13);

    fat32_close(fat, fd);
    fd = -1;

    res = fat32_dump_to_file(fat, fat_file_size, "fat_dump.bin");
    assert(res == 0);

    fat32_destroy(fat);
    fat = NULL;

}


