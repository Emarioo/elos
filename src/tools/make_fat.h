/*
    Component running on host system which creates FAT discs/binaries.

*/

#pragma once

#include "fat.h"

#define MAKE_FAT_MAX_DESCRIPTORS 256
#define MAKE_FAT_SEEK_SET 0
#define MAKE_FAT_SEEK_CUR 1
#define MAKE_FAT_SEEK_END 2

typedef struct {
    bool used;
    uint64_t head_offset;
} FAT32_object_descriptor;

typedef struct {
    FAT32_object_descriptor descriptors[MAKE_FAT_MAX_DESCRIPTORS];

    char* data;
    uint64_t data_len;
    uint64_t data_max;
} FAT32_object;

typedef struct {
    uint64_t st_size;
} FAT32_object_stat;

FAT32_object* fat32_create     ();
int          fat32_dump_to_file(FAT32_object* fat, uint64_t file_size, const char* path);
void         fat32_destroy     (FAT32_object* fat);

int fat32_open (FAT32_object* fat, const char* path);
int fat32_close(FAT32_object* fat, int fd);

uint64_t fat32_read (FAT32_object* fat, int fd, void* buf, uint64_t count);
uint64_t fat32_write(FAT32_object* fat, int fd, void* buf, uint64_t count);

uint64_t fat32_lseek(FAT32_object* fat, int fd, uint64_t offset, int whence);
int      fat32_fstat(FAT32_object* fat, int fd, FAT32_object_stat* statbuf);
