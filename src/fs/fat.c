#include "util/fat.h"

#include <string.h>

void FAT32_init_boot_record(FAT32_boot_record* boot_record) {
    FAT_bpb*    bpb  = &boot_record->standard_ebpb;
    FAT32_ebpb* ebpb = &boot_record->extended_ebpb;

    memset(boot_record, 0, sizeof(FAT32_boot_record));

    bpb->jmp_short[0] = 0xEB; // jmp short 3C
    bpb->jmp_short[1] = 0x3C;
    bpb->jmp_short[2] = 0x90; // NOP
    
    memcpy(bpb->oem, "MSWIN4.1", 8);

    // Values should be set from somewhere else?
    bpb->bytes_per_sector      = 512;
    bpb->sectors_per_cluster   = 1;
    bpb->reserved_sectors      = 1;
    bpb->fat_count             = 2;
    bpb->dir_entries_count     = 224; // should result in entire sectors
    bpb->total_sectors         = 2880; // 2880 * 512 = 1400 KB
    bpb->media_descriptor_type = 0xF0; // ?
    bpb->sectors_per_fat       = 9;
    bpb->sectors_per_track     = 18;
    bpb->heads                 = 2;
    bpb->hidden_sectors        = 0;
    bpb->large_sector_count    = 0;

    ebpb->sectors_per_fat      = 9;
    ebpb->flags                = 0;
    ebpb->fat_version          = 0x0000; // high byte is major, low byte is minor
    ebpb->cluster_number_of_root_directory = 2;
    ebpb->sector_number_of_fsinfo = 0;
    ebpb->sector_number_of_backup_boot_sector = 0;
    ebpb->drive_number = 0;
    ebpb->signature = 0x29;
    ebpb->volume_id = 0x78563412; // some random number?
    memcpy(ebpb->volume_label, "FAT32      ", 11);
    memcpy(ebpb->system_id, "ELOS    ", 8);
}




void fat12_init_table(fat12* data) {
    char* entries = (uint8_t*)data + 512;
    int entry_count = (8 * 512 * 8) / 12; // 8 sectors * 512 bytes * 8 bits / 12 bits per entry

    // clear table
    for (int i=0;i<entry_count;i++) {
        fat12_write_table_entry(entries, i, UNUSED_CLUSTER);
    }
    fat12_write_table_entry(entries, 0, 0xFF0); // same value as BPB_media descriptor in boot.asm
    fat12_write_table_entry(entries, 1, 0xFFF);

    int entries_size = 8 * 512;
    char* entries_mirror = entries + entries_size; // offset to second file allocation table
    memcpy(entries_mirror, entries, entries_size);

    DirectoryEntry* root_entries = (DirectoryEntry*)((uint8_t*)data + 19 * 512); // root starts at sector index 19
    int entry_counts = (14*512)/32;
    memset(root_entries, 0, entry_counts * sizeof(DirectoryEntry));
}

int fat12_create_entry(fat12* data, char* name, bool is_dir) {
    // TODO: handle slash in name

    DirectoryEntry* root_entries = (DirectoryEntry*)((uint8_t*)data + 19 * 512); // root starts at sector index 19

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
    
    char* table = (uint8_t*)data + 512;
    fat12_write_table_entry(table, cluster, LAST_CLUSTER_MIN);

    return found_entry;
}

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
    int val = *(uint16_t*)(entries + off);
    if (entry_index & 1) {
        return val >> 4;
    } else {
        return val & 0xFFF;
    }
}

void fat12_write_file(fat12* data, int entry_index, int offset, char* buffer, int size) {
    DirectoryEntry* root_entries = (DirectoryEntry*)((char*)data + 19 * 512); // root starts at sector index 19
    DirectoryEntry* entry = root_entries + entry_index;
    char* table = (uint8_t*)data + 512;

    int cluster = entry->first_cluster_number;
    // Assert(cluster != 0);

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

            // printf("wrote %d at %4p\n", bytes, cluster_data-(uint8_t*)data);

            // Assert(bytes > 0);
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
            // Assert(false); // bug
        }
        head += 512;
    }
}

int fat12_alloc_cluster(fat12* data) {
    char* entries = (uint8_t*)data + 512;
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
    
    // Assert(found_entry != 0 && found_entry != 1);

    return found_entry;
}
char* fat12_get_cluster_memory(fat12* data, int cluster) {
    // Assert(cluster >= 2);
    return (uint8_t*)data + 512 * (33 + cluster - 2);
}
