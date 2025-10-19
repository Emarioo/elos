#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// #include "elos/fat12.h"
#include "make_fat.h"

#define Assert(X) ((X) || (fprintf(stderr,"%s:%d. %s",__FILE__,__LINE__, #X), *(char*)0 = 0))

#pragma pack(push, 1)
typedef struct {
    uint8_t  status;
    uint8_t  begin_head;
    uint16_t begin_track : 6;
    uint16_t begin_cylinder : 10;
    uint8_t  partition_type;
    uint8_t  end_head;
    uint16_t end_track : 6;
    uint16_t end_cylinder : 10;
    uint32_t lba_start;
    uint32_t num_sectors;
} MBR_PartitionEntry;
#pragma pack(pop)


// must be zero initialized
typedef struct {
    void* data;
    uint64_t data_len;
    uint64_t data_max;

    void* locked_regions[4];
} Image;

void img_dump(Image* img, const char* path, uint64_t size) {
    if (size < img->data_len) {
        fprintf(stderr, "img_dump: provided size %lu is too small, needs %lu.\n", size, img->data_len);
        exit(1);
    }

    FILE* file = fopen(path,"wb");
    assert(file);
    size_t written = fwrite(img->data, 1, img->data_len, file);
    assert(written == img->data_len);

    if (img->data_len < size) {
        int pad_len = size - img->data_len;
        void* pad_data = malloc(pad_len);
        memset(pad_data, 0xCD, pad_len);
        size_t written = fwrite(pad_data, 1, pad_len, file);
        assert(written == pad_len);
        free(pad_data);
    }

    fclose(file);
}

void img_ensure_space(Image* img, uint64_t new_size) {
    if(new_size <= img->data_max)
        return;

    // Can't reallocate if we locked a memory region. We would invalidate pointers.
    int max_locked_regions = sizeof(img->locked_regions)/sizeof(*img->locked_regions);
    for (int i=0;i<max_locked_regions;i++) {
        assert(img->locked_regions[i] == NULL);
    }

    new_size += 0x4000;

    if (!img->data) {
        img->data = malloc(new_size);
        assert(img->data);
        img->data_len = 0;
        img->data_max = new_size;
        memset(img->data, 0xCD, new_size);
        return;
    }
    void* new_data = realloc(img->data, new_size);
    assert(new_data);
    memset((uint8_t*)new_data + img->data_max, 0xCD, new_size - img->data_max);
    img->data = new_data;
    img->data_max = new_size;
}

void img_write(Image* img, uint64_t offset, uint64_t size, void* data) {
    img_ensure_space(img, offset + size);

    memcpy((uint8_t*)img->data + offset, data, size);
    if(offset + size > img->data_len)
        img->data_len = offset + size;
}

void img_write1(Image* img, uint64_t offset, uint8_t value) {
    img_ensure_space(img, offset + 1);
    *(uint8_t*)((uint8_t*)img->data + offset) = value;
    if(offset + 1 > img->data_len)
        img->data_len = offset + 1;
}
void img_write2(Image* img, uint64_t offset, uint16_t value) {
    img_ensure_space(img, offset + 2);
    *(uint16_t*)((uint8_t*)img->data + offset) = value;
    if(offset + 2 > img->data_len)
        img->data_len = offset + 2;
}
void img_write4(Image* img, uint64_t offset, uint32_t value) {
    img_ensure_space(img, offset + 4);
    *(uint32_t*)((uint8_t*)img->data + offset) = value;
    if(offset + 4 > img->data_len)
        img->data_len = offset + 4;
}

void img_memset(Image* img, uint64_t offset, uint64_t size, uint8_t value) {
    img_ensure_space(img, offset + size);

    memset((uint8_t*)img->data + offset, value, size);
    
    if(offset + size > img->data_len)
        img->data_len = offset + size;
}

void img_write_from_file(Image* img, uint64_t offset, const char* path) {
    FILE* file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    img_ensure_space(img, offset + fileSize);

    fread((uint8_t*)img->data + offset, 1, fileSize, file);
    
    if(offset + fileSize > img->data_len)
        img->data_len = offset + fileSize;

    fclose(file);
}

void* img_lock_region(Image* img, uint64_t offset, uint64_t size) {
    img_ensure_space(img, offset + size);

    if(offset + size > img->data_len)
        img->data_len = offset + size;

    void* region = (uint8_t*)img->data + offset;
    int max_locked_regions = sizeof(img->locked_regions)/sizeof(*img->locked_regions);
    int index_locked_region = -1;
    for (int i=0;i<max_locked_regions;i++) {
        if (img->locked_regions[i] == NULL) {
            img->locked_regions[i] = region;
            index_locked_region = i;
            break;
        }
    }
    assert(index_locked_region != -1); // no free region

    return region;
}
void img_unlock_region(Image* img, void* data) {
    int max_locked_regions = sizeof(img->locked_regions)/sizeof(*img->locked_regions);
    for (int i=0;i<max_locked_regions;i++) {
        if (img->locked_regions[i] == data) {
            img->locked_regions[i] = NULL;
            return;
        }
    }
    assert(false); // provided pointer was never locked
}

int main(int argc, char** argv) {

    if (argc <= 1) {
        fprintf(stderr, "Missing path argument, create_image <out_image_path>\n");
        exit(1);
    }

    const char* path_img = argv[1];
    const char* path_bootloader = "bin/elos/mbr_bootloader.bin";
    const char* path_bootloader2 = "bin/elos/setup.bin";
    const char* path_kernel = "bin/elos/kernel.bin";

    Image img = {0};

    img_write_from_file(&img, 0, path_bootloader);

    uint64_t partition_table_offset = 0x200-66;

    // Clear partition table
    img_memset(&img, partition_table_offset, 64, 0);

    int fat_size = 512 * (2880-1); // total sectors in bootloader

    void* fat = img_lock_region(&img, 0x0, fat_size);

    fat16_init_table(fat);
    // fat12_init_table(fat);

    int root_entry_index = fat16_create_entry(fat, "SETUP.BIN", 0);
    // int root_entry_index = fat12_create_entry(fat, "SETUP.BIN", 0);

    {
        FILE* file_setup = fopen(path_bootloader2, "rb");
        Assert(file_setup);
        fseek(file_setup, 0, SEEK_END);
        int file_size_setup = ftell(file_setup);
        fseek(file_setup, 0, SEEK_SET);
        char* data_setup = malloc(file_size_setup);
        fread(data_setup, 1, file_size_setup, file_setup);
        fclose(file_setup);

        fat16_write_file(fat, root_entry_index, 0, data_setup, file_size_setup);
        // fat12_write_file(fat, root_entry_index, 0, data_setup, file_size_setup);
        printf("wrote %d bytes of setup\n", file_size_setup);
    }

    fat16_mirror_fat(fat); // there are two FATs, the second one should be mirrored

    MBR_PartitionEntry partition_entry = {0};
    partition_entry.status = 0x80; // HDD drive number
    partition_entry.num_sectors = 0x1000/0x200;
    partition_entry.begin_head = 0;
    partition_entry.begin_track = 1;
    partition_entry.begin_cylinder = 0;
    partition_entry.partition_type = 0x6; // https://en.wikipedia.org/wiki/Partition_type
    partition_entry.begin_head = 0;
    partition_entry.begin_track = 1 + fat_size / 512;
    partition_entry.begin_cylinder = 0;
    partition_entry.lba_start = 1;
    partition_entry.num_sectors = fat_size / 512;
    img_write(&img, partition_table_offset, 16, &partition_entry);

    img_dump(&img, path_img, img.data_len);
    printf("Constructed IMAGE %s\n", path_img);

    // int kernelc_root_entry_index = fat12_create_entry(data, "KERNELC.BIN", 0);
    // fat12_write_file(data, kernelc_root_entry_index, 0, data_kernelc, file_size_kernelc);
    // printf("wrote %d bytes of kernel\n", file_size_kernel);

    // FILE* file_floppy = fopen(path_out_floppy,"wb");
    // fwrite(data, 1, data_size, file_floppy);
    // fclose(file_floppy);

    // printf("wrote floppy.img\n");

    return 0;
}
