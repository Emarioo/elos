#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define Assert(X) ((X) || (fprintf(stderr,"%s:%d. %s",__FILE__,__LINE__, #X), *(char*)0 = 0))

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


enum DirectoryAttributes {
    READ_ONLY=0x01,
    HIDDEN=0x02,
    SYSTEM=0x04,
    VOLUME_ID=0x08,
    DIRECTORY=0x10,
    ARCHIVE=0x20,
    LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID
};

#pragma pack(push,1)
typedef struct {
    char file_name[11];
    u8 attributes;
    u8 _reserved;
    u8 creation_time_ms; // not actually ms, more like 100 hundreths of a second
    u16 creation_time;
    u16 creation_date;
    u16 accessed_date;
    u16 zero; // zero for FAT12, high 16 bits of first cluster number otherwise
    u16 modified_time;
    u16 modified_date;
    u16 first_cluster_number;
    u32 file_size;
} DirectoryEntry;
typedef struct {
    u8 order;
    u16 file_name5[5];
    u8 attributes;
    u8 zero;
    u8 checksum;
    u16 file_name6[6];
    u16 zero2;
    u16 file_name2[6];
} LongNameEntry;
#pragma pack(pop)

// #define UNUSED_CLUSTER 0x123
#define UNUSED_CLUSTER 0x000
#define RESERVED_CLUSTER_MIN 0xFF0
#define RESERVED_CLUSTER_MAX 0xFF6
#define BAD_CLUSTER 0xFF7
#define LAST_CLUSTER_MIN 0xFF8
#define LAST_CLUSTER_MAX 0xFFF

void fat12_write_table_entry(char* entries, int entry_index, int value) {
    int off = entry_index + (entry_index / 2); // (entry_index * 12) / 8
    // printf("off: %d\n", off);
    if (entry_index & 1) {
        entries[off] =  (entries[off] & 0x0F) | ((value & 0xF) << 4);
        entries[off + 1] = (value >> 4) & 0xFF;
    } else {
        entries[off] = value & 0xFF;
        entries[off + 1] = (entries[off + 1] & 0xF0) | (value >> 8);
    }
}
int fat12_read_table_entry(char* entries, int entry_index) {
    int off = entry_index + (entry_index / 2); // (entry_index * 12) / 8
    // printf("off: %d\n", off);
    int val = *(u16*)(entries + off);
    if (entry_index & 1) {
        return val >> 4;
    } else {
        return val & 0xFFF;
    }
}

void fat12_init_table(char* data) {
    char* entries = data + 512;
    int entry_count = 8 * 512 * 8 / 12; // 8 sectors * 512 bytes * 8 bits / 12 bits per entry

    // clear table
    for (int i=0;i<entry_count;i++) {
        fat12_write_table_entry(entries, i, UNUSED_CLUSTER);
    }
    fat12_write_table_entry(entries, 0, 0xFF0); // same value as BPB_media descriptor in boot.asm
    fat12_write_table_entry(entries, 1, 0xFFF);

    int entries_size = 8 * 512;
    char* entries_mirror = entries + entries_size; // offset to second file allocation table
    memcpy(entries_mirror, entries, entries_size);
}
// returns logical cluster number
int fat12_alloc_cluster(char* data) {
    char* entries = data + 512;
    int entry_count = 8 * 512 * 8 / 12; // 8 sectors * 512 bytes * 8 bits / 12 bits per entry

    int found_entry = -1;
    for (int i=0;i<entry_count;i++) {
        int value = fat12_read_table_entry(entries, i);
        if(value == UNUSED_CLUSTER) {
            found_entry = i;
            break;
        } else if(value == BAD_CLUSTER){
            break;
        }
    }
    if(found_entry == -1)
        return -1;
    
    Assert(found_entry != 0 && found_entry != 1);

    return found_entry;
}
int fat12_create_entry(char* data, char* name, int is_dir) {
    // handle slash in name

    DirectoryEntry* root_entries = (DirectoryEntry*)(data + 19 * 512); // root starts at sector index 19

    int entry_count = (14*512)/32;
    int found_entry = -1;
    for(int i=0;i<entry_count;i++){
        DirectoryEntry* entry = root_entries + i;
        if (entry->file_name[0] == 0 || entry->file_name[0] == 0xE5) {
            // free
            found_entry = i;
            break;
        } else {
            // go to next one
        }
    }
    if(found_entry == -1) {
        return 0;
    }
    DirectoryEntry* entry = root_entries + found_entry;

    int name_len = strlen(name);
    int dot_pos = 0;
    while(name[dot_pos] && name[dot_pos] != '.') dot_pos++;
    if (dot_pos == name_len)
        return 0;
    
    memset(entry->file_name, ' ', sizeof(entry->file_name));
    memcpy(entry->file_name, name, dot_pos);
    memcpy(entry->file_name + 8, name + dot_pos + 1, name_len - dot_pos - 1);
    if (is_dir)
        entry->attributes = DIRECTORY;
    else
        entry->attributes = ARCHIVE;

    int cluster = fat12_alloc_cluster(data);
    if(cluster == -1)
        return 0;

    entry->first_cluster_number = cluster;
    
    char* table = data + 512;
    fat12_write_table_entry(table, cluster, LAST_CLUSTER_MIN);

    return found_entry;
}
char* fat12_get_cluster_memory(char* data, int cluster) {
    Assert(cluster >= 2);
    return data + 512 * (33 + cluster - 2);
}
void fat12_write_file(char* data, int entry_index, int offset, char* buffer, int size) {
    DirectoryEntry* root_entries = (DirectoryEntry*)(data + 19 * 512); // root starts at sector index 19
    DirectoryEntry* entry = root_entries + entry_index;
    char* table = data + 512;

    int cluster = entry->first_cluster_number;
    Assert(cluster != 0);

    int written_bytes = 0;
    int head = 0;
    while(size > 0) {
        if(offset < head + 512) {
            // write bytes
            // we assume first cluster exists
            int off = offset - head;
            if(off < 0)
                off = 0;
            char* cluster_data = fat12_get_cluster_memory(data, cluster);
            
            int bytes = size - off;
            if (bytes > 512) {
                bytes = 512;
            }

            printf("wrote %d at %4p\n", bytes, cluster_data-data);

            Assert(bytes > 0);
            memcpy(cluster_data + off, buffer + written_bytes, bytes);
            written_bytes += bytes;
            size -= bytes;

            if(size == 0) {
                break;
            }
        }

        // next cluster
        int value = fat12_read_table_entry(table, cluster);
        if (value >= LAST_CLUSTER_MIN && value <= LAST_CLUSTER_MAX) {
            // alloc new cluster
            int new_cluster = fat12_alloc_cluster(data);
            fat12_write_table_entry(table, cluster, new_cluster); // old cluster points to new cluster
            fat12_write_table_entry(table, new_cluster, LAST_CLUSTER_MIN); // new cluster is the end
            cluster = new_cluster;
            char* cluster_data = fat12_get_cluster_memory(data, new_cluster);
            memset(cluster_data, 0, 512);
        } else if (value > 0 && value < RESERVED_CLUSTER_MIN) {
            // next cluster
            cluster = value;
        } else {
            Assert(false); // bug
        }
        head += 512;
    }
}

int main() {
    const char* path_floppy = "bin/floppy.img";
    const char* path_boot = "bin/bootloader.bin";
    const char* path_kernel = "bin/kernel.bin";
    const char* path_kernelc = "bin/kernel_c.bin";
    FILE* file_floppy = fopen(path_floppy,"wb");

    FILE* file_boot = fopen(path_boot, "rb");
    Assert(file_boot);
    fseek(file_boot, 0, SEEK_END);
    int file_size_boot = ftell(file_boot);
    fseek(file_boot, 0, SEEK_SET);
    
    FILE* file_kernel = fopen(path_kernel, "rb");
    Assert(file_kernel);
    fseek(file_kernel, 0, SEEK_END);
    int file_size_kernel = ftell(file_kernel);
    fseek(file_kernel, 0, SEEK_SET);
    char* data_kernel = malloc(file_size_kernel);
    fread(data_kernel, 1, file_size_kernel, file_kernel);

    // FILE* file_kernelc = fopen(path_kernelc, "rb");
    // Assert(file_kernelc);
    // fseek(file_kernelc, 0, SEEK_END);
    // int file_size_kernelc = ftell(file_kernelc);
    // fseek(file_kernelc, 0, SEEK_SET);
    // char* data_kernelc = malloc(file_size_kernelc);
    // fread(data_kernelc, 1, file_size_kernelc, file_kernelc);

    int data_size = 1440 * 1024;
    char* data = malloc(data_size);
    memset(data, 0, data_size);

    fread(data + 0, 1, file_size_boot, file_boot);
    printf("wrote %d bytes of bootloader\n", file_size_boot);



    fat12_init_table(data);

    int root_entry_index = fat12_create_entry(data, "KERNEL.BIN", 0);
    
    fat12_write_file(data, root_entry_index, 0, data_kernel, file_size_kernel);
    printf("wrote %d bytes of kernel\n", file_size_kernel);

    // int kernelc_root_entry_index = fat12_create_entry(data, "KERNELC.BIN", 0);
    // fat12_write_file(data, kernelc_root_entry_index, 0, data_kernelc, file_size_kernelc);

    fwrite(data, 1, data_size, file_floppy);

    printf("wrote floppy.img\n");

    return 0;
}