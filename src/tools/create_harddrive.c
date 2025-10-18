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
} ISO;

void iso_dump(ISO* iso, const char* path, uint64_t size) {
    if (size < iso->data_len) {
        fprintf(stderr, "iso_dump: provided size %lu is too small, needs %lu.\n", size, iso->data_len);
        exit(1);
    }

    FILE* file = fopen(path,"wb");
    assert(file);
    size_t written = fwrite(iso->data, 1, iso->data_len, file);
    assert(written == iso->data_len);

    if (iso->data_len < size) {
        int pad_len = size - iso->data_len;
        void* pad_data = malloc(pad_len);
        memset(pad_data, 0xCD, pad_len);
        size_t written = fwrite(pad_data, 1, pad_len, file);
        assert(written == pad_len);
        free(pad_data);
    }

    fclose(file);
}

void iso_ensure_space(ISO* iso, uint64_t new_size) {
    if(new_size <= iso->data_max)
        return;

    // Can't reallocate if we locked a memory region. We would invalidate pointers.
    int max_locked_regions = sizeof(iso->locked_regions)/sizeof(*iso->locked_regions);
    for (int i=0;i<max_locked_regions;i++) {
        assert(iso->locked_regions[i] == NULL);
    }

    new_size += 0x4000;

    if (!iso->data) {
        iso->data = malloc(new_size);
        assert(iso->data);
        iso->data_len = 0;
        iso->data_max = new_size;
        memset(iso->data, 0xCD, new_size);
        return;
    }
    void* new_data = realloc(iso->data, new_size);
    assert(new_data);
    memset((uint8_t*)new_data + iso->data_max, 0xCD, new_size - iso->data_max);
    iso->data = new_data;
    iso->data_max = new_size;
}

void iso_write(ISO* iso, uint64_t offset, uint64_t size, void* data) {
    iso_ensure_space(iso, offset + size);

    memcpy((uint8_t*)iso->data + offset, data, size);
    if(offset + size > iso->data_len)
        iso->data_len = offset + size;
}

void iso_write1(ISO* iso, uint64_t offset, uint8_t value) {
    iso_ensure_space(iso, offset + 1);
    *(uint8_t*)((uint8_t*)iso->data + offset) = value;
    if(offset + 1 > iso->data_len)
        iso->data_len = offset + 1;
}
void iso_write2(ISO* iso, uint64_t offset, uint16_t value) {
    iso_ensure_space(iso, offset + 2);
    *(uint16_t*)((uint8_t*)iso->data + offset) = value;
    if(offset + 2 > iso->data_len)
        iso->data_len = offset + 2;
}
void iso_write4(ISO* iso, uint64_t offset, uint32_t value) {
    iso_ensure_space(iso, offset + 4);
    *(uint32_t*)((uint8_t*)iso->data + offset) = value;
    if(offset + 4 > iso->data_len)
        iso->data_len = offset + 4;
}

void iso_memset(ISO* iso, uint64_t offset, uint64_t size, uint8_t value) {
    iso_ensure_space(iso, offset + size);

    memset((uint8_t*)iso->data + offset, value, size);
    
    if(offset + size > iso->data_len)
        iso->data_len = offset + size;
}

void iso_write_from_file(ISO* iso, uint64_t offset, const char* path) {
    FILE* file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    iso_ensure_space(iso, offset + fileSize);

    fread((uint8_t*)iso->data + offset, 1, fileSize, file);
    
    if(offset + fileSize > iso->data_len)
        iso->data_len = offset + fileSize;

    fclose(file);
}

void* iso_lock_region(ISO* iso, uint64_t offset, uint64_t size) {
    iso_ensure_space(iso, offset + size);

    if(offset + size > iso->data_len)
        iso->data_len = offset + size;

    void* region = (uint8_t*)iso->data + offset;
    int max_locked_regions = sizeof(iso->locked_regions)/sizeof(*iso->locked_regions);
    int index_locked_region = -1;
    for (int i=0;i<max_locked_regions;i++) {
        if (iso->locked_regions[i] == NULL) {
            iso->locked_regions[i] = region;
            index_locked_region = i;
            break;
        }
    }
    assert(index_locked_region != -1); // no free region

    return region;
}
void iso_unlock_region(ISO* iso, void* data) {
    int max_locked_regions = sizeof(iso->locked_regions)/sizeof(*iso->locked_regions);
    for (int i=0;i<max_locked_regions;i++) {
        if (iso->locked_regions[i] == data) {
            iso->locked_regions[i] = NULL;
            return;
        }
    }
    assert(false); // provided pointer was never locked
}

int main() {
    const char* path_img = "bin/elos.img";
    const char* path_bootloader = "bin/elos/mbr_bootloader.bin";
    const char* path_bootloader2 = "bin/elos/setup.bin";
    const char* path_kernel = "bin/elos/kernel.bin";

    ISO iso = {0};

    iso_write_from_file(&iso, 0, path_bootloader);

    uint64_t partition_table_offset = 0x200-66;

    // Clear partition table
    iso_memset(&iso, partition_table_offset, 64, 0);

    int fat_size = 512 * 40; // we put setup.bin at sector 33

    void* fat = iso_lock_region(&iso, 0x0, fat_size);

    fat12_init_table(fat);

    int root_entry_index = fat12_create_entry(fat, "SETUP.BIN", 0);

    {
        FILE* file_setup = fopen(path_bootloader2, "rb");
        Assert(file_setup);
        fseek(file_setup, 0, SEEK_END);
        int file_size_setup = ftell(file_setup);
        fseek(file_setup, 0, SEEK_SET);
        char* data_setup = malloc(file_size_setup);
        fread(data_setup, 1, file_size_setup, file_setup);
        fclose(file_setup);

        fat12_write_file(fat, root_entry_index, 0, data_setup, file_size_setup);
        printf("wrote %d bytes of setup\n", file_size_setup);
    }

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
    iso_write(&iso, partition_table_offset, 16, &partition_entry);

    iso_dump(&iso, path_img, iso.data_len);
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
