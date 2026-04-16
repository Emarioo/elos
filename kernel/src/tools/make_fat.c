#include "tools/make_fat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FAT32_object* fat32_create() {
    FAT32_object* obj = malloc(sizeof(FAT32_object));
    memset(obj, 0, sizeof(FAT32_object));
    return obj;
}

int fat32_dump_to_file(FAT32_object* fat, uint64_t file_size, const char* path) {
    FILE* f = fopen(path, "wb");
    if(!f) {
        fprintf(stderr, "Cannot open %s for writing\n", path);
        return -1;
    }
    if (fat->data_len > file_size) {
        fprintf(stderr, "FAT structure occupies more bytes than 'file_size' parameter specified (%ld vs %ld)\n (file: %s)\n", fat->data_len, file_size, path);
        return -1;
    }
    size_t written = fwrite(fat->data, 1, fat->data_max, f);
    if (written != file_size) {
        fprintf(stderr, "Could not write %lu bytes to %s. (fwrite returned %ld)\n", file_size, path, (int64_t)written);
        return -1;
    }
    fclose(f);
    return 0;
}
void fat32_destroy(FAT32_object* fat) {
    free(fat->data);
    free(fat);
}

int fat32_open (FAT32_object* fat, const char* path) {
    int fd = -1;
    for (int i = 1; i < MAKE_FAT_MAX_DESCRIPTORS; i++) {
        if(!fat->descriptors[i].used) {
            fd = i;
            break;
        }
    }
    if(fd < 0) {
        fprintf(stderr, "No more descriptors available (max %d)\n", MAKE_FAT_MAX_DESCRIPTORS);
        return -1;
    }
    FAT32_object_descriptor* descriptor = &fat->descriptors[fd];
    memset(descriptor, 0, sizeof(FAT32_object_descriptor));
    
    // create/open file in FAT



    descriptor->used = true;
    return fd;
}
int fat32_close(FAT32_object* fat, int fd) {
    if(fd < 0 || fd >= MAKE_FAT_MAX_DESCRIPTORS)
        return -1;
    if (!fat->descriptors[fd].used)
        return -1;
    fat->descriptors[fd].used = false;
    return 0;
}

uint64_t fat32_read(FAT32_object* fat, int fd, void* buf, uint64_t count) {
    return -1;
}
uint64_t fat32_write(FAT32_object* fat, int fd, void* buf, uint64_t count) {
    return -1;
}

uint64_t fat32_lseek(FAT32_object* fat, int fd, uint64_t offset, int whence) {
    return -1;
}
int fat32_fstat(FAT32_object* fat, int fd, FAT32_object_stat* statbuf) {
    return -1;
}