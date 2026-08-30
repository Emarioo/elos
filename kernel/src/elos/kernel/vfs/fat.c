
#include "elos/vfs.h"
#include "elos/kernel/vfs/vfile_system.h"

#include "elos/common/string.h"
#include "elos/common/types.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"
#include "elos/kernel_console.h"

#include "elos/kernel/vfs/gpt.h"
#include "elos/kernel/vfs/mbr.h"
#include "elos/kernel/vfs/fat.h"

#include "elos/kernel/vfs/vfile_system.h"

#define printf(...) KCON_printf(__VA_ARGS__)

#define SECTOR_SIZE 512


typedef struct {
    int fat_version;
    int sectors_per_fat;
    int fat_entries_per_sector; // may not align to a whole sector (because of FAT12)

    int sector_size;

    fat__BPB* bpb;
    fat16__EBPB* ebpb16;
    fat32__EBPB* ebpb32;

    u64 start_lba;
    u64 end_lba;
    DiskDevice device;

    char _bootSector[512];

    char tempSector[512];
    FAT_ID currentDir;
    u32 current_cluster;
    u32 entries_index;
    u64 sector_start;
    u64 sector_end;
    u32 sector_index;

} FATContext;



FAT_ID get_root_directory(FATContext* context);

FAT_ID find_directory(FATContext* context, FAT_ID currentDir, const cstring subname);

// Assumes name doesn't already exist. Use find_directory first.
// @TODO find_directory can be optimized to cache a free entry it finds.
// If subsequently calling create_directory then it use that entry without
// iterating whole directory again.
FAT_ID create_fat_entry(FATContext* context, FAT_ID currentDir, const cstring subname, bool isDirectory);
bool delete_entry(FATContext* context, FAT_ID currentDir, const cstring subname);


fat__DirectoryEntry* fat_next_entry(FATContext* context, FAT_ID currentDir, u32* entryIndex, char* longName);

int get_free_cluster(FATContext* context);


FAT_ID make_fat_id(FATContext* context, int currentCluster);

void extract_fat_id(FATContext* context, FAT_ID fatID, u32* cluster, u64* sectorStart, u64* sectorEnd);


#define fat__FAT_NONE 0
#define fat__FAT12 12
#define fat__FAT16 16
#define fat__FAT32 32

int fat__detect_type(fat__BPB* bpb);

int fat__sane_cstring(const fat__DirectoryEntry* entry, char* out_path);
int fat__cluster_to_sector_offset(FATContext* context, int cluster);


int fat__get_fat(FATContext* context, int cluster);
int fat__set_fat(FATContext* context, int cluster, uint32_t value);

bool fat__string_equal(const cstring name, const cstring entryName);

bool init_context(FATContext* context, VFS_Mount* mount);


static bool IsLeapYear(int year);
static int DaysBeforeMonth(int month, bool leap);

u64 fat_convert_to_unix_us(u16 fatDate, u16 fatTime, u8 creationTenths);



int required_long_name_entries(const cstring name);




/*
    EXTRA HELPER FUNCTIONS
*/







bool init_context(FATContext* context, VFS_Mount* mount) {
    int res;

    memset(context, 0, sizeof(*context));

    res = DISK_read(mount->diskDevice, mount->start_lba * SECTOR_SIZE, SECTOR_SIZE, context->_bootSector);
    if (res == 0) {
        return false;
    }

    fat__BPB* bootBlock = (fat__BPB*)(context->_bootSector);
    
    // @TODO If values are wrong or corrupt then we should return false.
    context->device    = mount->diskDevice;
    context->start_lba = mount->start_lba;
    context->end_lba   = mount->end_lba;

    context->fat_version = fat__detect_type(bootBlock);
    context->bpb = bootBlock;
    context->ebpb16 = (void*)((char*)bootBlock + 36);
    context->ebpb32 = (void*)((char*)bootBlock + 36);
    context->sector_size = 512;

    if (context->fat_version == fat__FAT32) {
        context->sectors_per_fat = context->ebpb32->sectors_per_fat_32;
        context->fat_entries_per_sector = context->sector_size / 4;
    } else if (context->fat_version == fat__FAT16) {
        context->sectors_per_fat = context->bpb->sectors_per_fat_16;
        context->fat_entries_per_sector = context->sector_size / 2;
    } else if (context->fat_version == fat__FAT12) {
        context->sectors_per_fat = context->bpb->sectors_per_fat_16;
        context->fat_entries_per_sector = 0; // Can't use this for fat12, doesn't divide cleanly
    }
    return true;
}


int fat__detect_type(fat__BPB* bpb) {
    if (bpb->bytes_per_sector == 0 || bpb->sectors_per_cluster == 0)
        return fat__FAT_NONE;

    if (bpb->sectors_per_fat_16 == 0)
        return fat__FAT32;

    // Otherwise FAT12 or FAT16
    uint32_t root_dir_sectors = ((bpb->root_dir_entries * sizeof(fat__DirectoryEntry))
                                + (bpb->bytes_per_sector - 1))
                                / bpb->bytes_per_sector;

    uint32_t total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    uint32_t fat_size = bpb->sectors_per_fat_16;
    uint32_t data_sectors = total_sectors
        - (bpb->reserved_sectors + (bpb->fat_count * fat_size) + root_dir_sectors);

    uint32_t cluster_count = data_sectors / bpb->sectors_per_cluster;

    if (total_sectors == 0 || data_sectors == 0)
        return fat__FAT_NONE;

    if (cluster_count < 4085)
        return fat__FAT12;
    else
        return fat__FAT16;
}



int fat__cluster_to_sector_offset(FATContext* context, int cluster) {
    int sectorSize = 512;

    int clusterStart = context->bpb->reserved_sectors
        + context->bpb->fat_count * context->sectors_per_fat
        + (context->bpb->root_dir_entries * sizeof(fat__DirectoryEntry) + context->sector_size-1)/context->sector_size;
    // root_dir_entries is zero on FAT32
    // First 2 clusters are reserved.
    return clusterStart + (cluster - 2) * context->bpb->sectors_per_cluster;
}


int fat__sane_cstring(const fat__DirectoryEntry* entry, char* out_path) {
    if (entry->attributes == fat__LFN) {
        return 0;
    }

    int filename_len = 8;
    while (filename_len > 0 && (entry->file_name[filename_len-1] == 0 || entry->file_name[filename_len-1] == ' ')) {
        filename_len--;
    }

    if (filename_len > 0) {
        memcpy(out_path, entry->file_name, filename_len);
    }

    int extension_len = 3;
    while (extension_len > 0 && (entry->file_name[8 + extension_len-1] == 0 || entry->file_name[8 + extension_len-1] == ' ')) {
        extension_len--;
    }

    if (extension_len > 0) {
        out_path[filename_len] = '.';
        memcpy(out_path + filename_len + 1, entry->file_name+8, extension_len);
        out_path[filename_len + 1 + extension_len] = '\0';
        return filename_len + 1 + extension_len;
    } else {
        out_path[filename_len] = '\0';
        return filename_len;
    }
}





bool fat__string_equal(const cstring name, const cstring entryName) {
    if (entryName.len != name.len) {
        return false;
    }

    for (int i=0;i<entryName.len;i++) {
        char chr0 = entryName.ptr[i];
        char chr1 = name.ptr[i];

        // Case-insensitive compare
        if ((chr0 >= 'A' && chr0 <= 'Z') || (chr0 >= 'a' && chr0 <= 'z')) {
            chr0 |= 32;
        }
        if ((chr1 >= 'A' && chr1 <= 'Z') || (chr1 >= 'a' && chr1 <= 'z')) {
            chr1 |= 32;
        }
        if (chr0 != chr1)
            return false;
    }
    return true;
}


FAT_ID make_fat_id(FATContext* context, int currentCluster) {
    if (currentCluster == FAT_ID_ROOT_DIR) {
        printf("make_fat_id: Don't pass in cluster 1!\n");
        kernel_bug();
        return FAT_ID_NULL;
    }
    return currentCluster;
}
void extract_fat_id(FATContext* context, FAT_ID fatID, u32* cluster, u64* sectorStart, u64* sectorEnd) {
    if (fatID == FAT_ID_ROOT_DIR) {
        *cluster = -1;
        *sectorStart = context->bpb->reserved_sectors
            + context->sectors_per_fat * context->bpb->fat_count;
        *sectorEnd = *sectorStart + (context->bpb->root_dir_entries
            * sizeof(fat__DirectoryEntry)) / context->sector_size;
        return;
    }

    int current_cluster = fatID;
    *cluster = current_cluster;
    *sectorStart = fat__cluster_to_sector_offset(context, current_cluster);
    *sectorEnd = *sectorStart + context->bpb->sectors_per_cluster;
}


FAT_ID get_root_directory(FATContext* context) {
    if (context->fat_version == fat__FAT32) {
        return make_fat_id(context, context->ebpb32->root_cluster);
    }     
    return FAT_ID_ROOT_DIR;
}




int get_free_cluster(FATContext* context) {
    int nextCluster = 2;

    int clusterCount;
    if (context->fat_version == fat__FAT12) {
        clusterCount = (context->sectors_per_fat * context->sector_size * 2) / 3;
    } else if (context->fat_version == fat__FAT16) {
        clusterCount = (context->sectors_per_fat * context->sector_size) / 2;
    } else if (context->fat_version == fat__FAT32) {
        clusterCount = (context->sectors_per_fat * context->sector_size) / 4;
    }

    while (nextCluster < clusterCount) {
        int value = fat__get_fat(context, nextCluster);

        if (value == 0) {
            return nextCluster;
        }
        nextCluster++;
    }

    return FAT_CLUSTER_INVALID;
}


/*
    Date and time code is AI generated
*/

static bool IsLeapYear(int year) {
    return ((year % 4) == 0) &&
           (((year % 100) != 0) || ((year % 400) == 0));
}

static int DaysBeforeMonth(int month, bool leap) {
    static const int table[12] =
    {
        0,   // Jan
        31,  // Feb
        59,  // Mar
        90,  // Apr
        120, // May
        151, // Jun
        181, // Jul
        212, // Aug
        243, // Sep
        273, // Oct
        304, // Nov
        334  // Dec
    };

    int days = table[month - 1];

    if(leap && month > 2)
        days += 1;

    return days;
}

u64 fat_convert_to_unix_us(u16 fatDate, u16 fatTime, u8 creationTenths) {
    int day   = (fatDate & 0x1F);
    int month = (fatDate >> 5) & 0x0F;
    int year  = ((fatDate >> 9) & 0x7F) + 1980;

    int second = (fatTime & 0x1F) * 2;
    int minute = (fatTime >> 5) & 0x3F;
    int hour   = (fatTime >> 11) & 0x1F;

    // FAT creationTenths stores 10ms increments.
    // Valid range usually 0-199.
    int milliseconds = creationTenths * 10;

    // Days since Unix epoch (1970-01-01)
    u64 days = 0;

    for(int y = 1970; y < year; y++)
    {
        days += IsLeapYear(y) ? 366 : 365;
    }

    days += DaysBeforeMonth(month, IsLeapYear(year));
    days += (day - 1);

    u64 totalSeconds =
        days * 86400ULL +
        hour * 3600ULL +
        minute * 60ULL +
        second;

    u64 totalMicroseconds =
        totalSeconds * 1000000ULL +
        (u64)milliseconds * 1000ULL;

    return totalMicroseconds;
}


u64 fat__sane_mtime(const fat__DirectoryEntry* entry) {
    return fat_convert_to_unix_us(entry->modified_date, entry->modified_time, 0);
}

FAT_ID fat_encode_id(FATContext* context, u32 cluster, u32 sectorIndex, u32 entryIndex, u32 clusterContent) {
    u64 value = 
          (((u64)cluster         & 0xFFFFFF) << 40)
        | (((u64)sectorIndex     & 0xFF)     << 32)
        | (((u64)entryIndex      & 0xFF)     << 24)
        | (((u64)clusterContent  & 0xFFFFFF) << 0);
    // printf("Encode 0x%zx <- %u %u %u %u\n", value, cluster, sectorIndex, entryIndex, clusterContent);
    return value;
}

void fat_decode_id(FATContext* context, FAT_ID id, u32* dir_cluster, u32* dir_sectorIndex, u32* dir_entryIndex, u64* dir_sectorStart, u64* dir_sectorEnd, u32* content_cluster, u64* content_sectorStart, u64* content_sectorEnd) {
    u32 id_cluster        = (id >> 40) & 0xFFFFFF;
    u32 id_sectorIndex    = (id >> 32) & 0xFF;
    u32 id_entryIndex     = (id >> 24) & 0xFF;
    u32 id_clusterContent = (id >> 0)  & 0xFFFFFF;
    if (dir_cluster)
        *dir_cluster = -1;
    if (dir_sectorIndex)
        *dir_sectorIndex = -1;
    if (dir_entryIndex)
        *dir_entryIndex = -1;
    if (dir_sectorStart)
        *dir_sectorStart = -1;
    if (dir_sectorEnd)
        *dir_sectorEnd = -1;
    if (content_cluster)
        *content_cluster = -1;
    if (id == FAT_ID_ROOT_DIR && context->fat_version != fat__FAT32) {
        if (content_sectorStart)
            *content_sectorStart = context->bpb->reserved_sectors
                + context->sectors_per_fat * context->bpb->fat_count;
        if (content_sectorEnd)
            *content_sectorEnd = *content_sectorStart + (context->bpb->root_dir_entries
                * sizeof(fat__DirectoryEntry)) / context->sector_size;
    } else if (id == FAT_ID_ROOT_DIR && context->fat_version == fat__FAT32) {
        if (content_cluster)
            *content_cluster = context->ebpb32->root_cluster;
        if (content_sectorStart)
            *content_sectorStart = fat__cluster_to_sector_offset(context, *content_cluster);
        if (content_sectorEnd)
            *content_sectorEnd = *content_sectorStart + context->bpb->sectors_per_cluster;
    } else {
        if (dir_cluster)
            *dir_cluster = id_cluster;
        if (dir_sectorIndex)
            *dir_sectorIndex = id_sectorIndex;
        if (dir_entryIndex)
            *dir_entryIndex = id_entryIndex;
        if (dir_sectorStart) {
            if (id_cluster == 0xFFFFFF) {
                if (context->fat_version != fat__FAT32) {
                    *dir_sectorStart = context->bpb->reserved_sectors
                        + context->sectors_per_fat * context->bpb->fat_count;
                    *dir_sectorEnd = *dir_sectorStart + (context->bpb->root_dir_entries
                        * sizeof(fat__DirectoryEntry)) / context->sector_size;
                } else if (context->fat_version == fat__FAT32) {
                    *dir_cluster = context->ebpb32->root_cluster;
                    *dir_sectorStart = fat__cluster_to_sector_offset(context, *dir_cluster);
                    *dir_sectorEnd = *dir_sectorStart + context->bpb->sectors_per_cluster;
                }
            } else {
                *dir_sectorStart = fat__cluster_to_sector_offset(context, id_cluster);
                *dir_sectorEnd = *dir_sectorStart + context->bpb->sectors_per_cluster;
            }
        }
        if (content_cluster)
            *content_cluster = id_clusterContent;
        if (content_sectorStart)
            *content_sectorStart = fat__cluster_to_sector_offset(context, id_clusterContent);
        if (content_sectorEnd)
            *content_sectorEnd = *content_sectorStart + context->bpb->sectors_per_cluster;
    }
    // printf("Decode 0x%zx -> %u %u %u %zu %zu %u\n", id, id_cluster, id_sectorIndex, id_entryIndex, !dir_sectorStart ? 0 : *dir_sectorStart, !dir_sectorEnd ? 0 : *dir_sectorEnd, id_clusterContent);
}



FAT_ID fat_get_root(VFS_Mount* mount) {
    return FAT_ID_ROOT_DIR;
}


FAT_ID fat_lookup(VFS_Mount* mount, FAT_ID directory, const cstring subname) {
    FAT_ID returnValue = FAT_ID_NULL;

    FATContext _context;
    FATContext* context = &_context;
    init_context(context, mount);

    int res;
    fat_decode_id(context, directory, NULL, NULL, NULL, NULL, NULL, &context->current_cluster, &context->sector_start, &context->sector_end);

    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    // int entriesPerSector = SECTOR_SIZE / sizeof(fat__DirectoryEntry);


    while (1) {
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == FAT_CLUSTER_INVALID) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                goto exit;
            }
            u32 nextCluster = fat__get_fat(context, context->current_cluster);
            if (!VALID_CLUSTER(nextCluster)) {
                goto exit;
            }
            context->current_cluster = nextCluster;
            context->sector_start = fat__cluster_to_sector_offset(context, nextCluster);
            context->sector_end = context->sector_start + context->bpb->sectors_per_cluster;
            context->sector_index = 0;

        }

        // printf("nextENT READ sector=0x%zx\n", (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE);
        res = DISK_read(context->device, (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return false;
        }

    
        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (int subEntryIndex=0;subEntryIndex<entries_len;subEntryIndex++) {
            fat__DirectoryEntry* entry = &entries[subEntryIndex];

            if (entry->attributes == fat__LFN) {
                fat__LongNameEntry* nameEntry = (fat__LongNameEntry*)entry;
                if (nameEntry->order & fat__LAST_LONG_ENTRY) {
                    longName_startIndex = longName_lastIndex;
                }
                for (int ci=5+6+2-1;ci>=0;ci--) {
                    u16 chr;
                    if (ci >= 0 && ci < 5) {
                        chr = nameEntry->file_name0[ci];
                    } else if (ci >= 5 && ci < 5+6) {
                        chr = nameEntry->file_name1[ci-5];
                    } else if (ci >= 5+6 && ci < 5+6+2) {
                        chr = nameEntry->file_name2[ci-5-6];
                    }

                    if (chr == 0 || chr == 0xFFFF || (chr == ' '
                        && longName_startIndex == longName_lastIndex))
                    {
                        // End character.
                        longName_startIndex = longName_lastIndex;
                    } else {
                        longName_startIndex--;
                        longName_buffer[longName_startIndex] = chr;
                    }
                }
                continue;
            }
            if (entry->file_name[0] == FAT_ENTRY_UNUSED) {
                // Not present
                continue;
            }
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                goto exit;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ')
                continue;
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ')
                continue;
            
            cstring entryName = {0};
            if (longName_startIndex != longName_lastIndex) {
                entryName = (cstring){
                    .ptr = longName_buffer + longName_startIndex,
                    .len = longName_lastIndex - longName_startIndex,
                };
                longName_startIndex = longName_lastIndex;
            } else {
                entryName.ptr = longName_buffer;
                entryName.len = fat__sane_cstring(entry, longName_buffer);
            }
            bool same = fat__string_equal(subname, entryName);

            if (!same) {
                continue;
            }

            u32 clusterIndex = entry->cluster_low | (entry->cluster_high << 16);

            // printf("lookup %s -> %d, %zu %u\n", entryName.ptr, clusterIndex, context->sector_start + context->sector_index, subEntryIndex);

            returnValue = fat_encode_id(context, context->current_cluster, context->sector_index, subEntryIndex, clusterIndex);
            goto exit;
        }
        context->sector_index++;
    }
exit:
    return returnValue;
}

FAT_ID fat_make_entry(VFS_Mount* mount, FAT_ID directory, const cstring subname, u32 attributes) {
    FAT_ID returnValue = FAT_ID_NULL;

    FATContext _context;
    FATContext* context = &_context;
    init_context(context, mount);

    // First we need to find an empty slot for all needed LFN entries if any) and the actual file/directory entry.

    int neededLFNEntries = required_long_name_entries(subname);
    
    u64 target_sectorStart = 0;
    u64 target_sectorEnd = 0;
    u64 target_sectorIndex = 0;
    u64 target_subEntryIndex = 0;
    int freeEntries = 0;

    int res;

    fat_decode_id(context, directory, NULL, NULL, NULL, NULL, NULL, &context->current_cluster, &context->sector_start, &context->sector_end);


    while (1) {
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == FAT_CLUSTER_INVALID) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                goto exit;
            }
            u32 nextCluster = fat__get_fat(context, context->current_cluster);
            if (nextCluster == FAT_CLUSTER_EOF) {
                // Make more entries.

                int remainingEntries = neededLFNEntries + 1 - freeEntries;

                // We sanity checkt that one cluster is enough to hold our entry name which it should be.
                int entriesPerCluster = context->bpb->sectors_per_cluster * context->fat_entries_per_sector;
                if (remainingEntries > entriesPerCluster) {
                    goto exit;
                }

                // @TODO When adding new cluster to directory's cluster chain we
                //   should probably increase fileSize of directory.
                //   Our implementation will be fine but there may incompatibility
                //   issues with other programs.

                u32 newCluster = get_free_cluster(context);
                if (newCluster == FAT_CLUSTER_INVALID) {
                    goto exit;
                }

                res = fat__set_fat(context, newCluster, FAT_CLUSTER_EOF);
                if (!res) {
                    goto exit;
                }

                memset(context->tempSector, 0, SECTOR_SIZE);
                int dir_sector_start = fat__cluster_to_sector_offset(context, newCluster);
                for (int i = 0; i < context->bpb->sectors_per_cluster; i++) {
                    // printf("CLEARING sector=0x%zx\n", (context->start_lba + dir_sector_start + i) * context->sector_size);
                    res = DISK_write( context->device,
                        (context->start_lba + dir_sector_start + i) * context->sector_size,
                        context->sector_size, context->tempSector);
                    if (!res) {
                        goto exit;
                    }
                }

                res = fat__set_fat(context, context->current_cluster, newCluster);
                if (!res) {
                    goto exit;
                }

                goto fill_entries;
            }
            if (!VALID_CLUSTER(nextCluster)) {
                goto exit;
            }
            context->current_cluster = nextCluster;
            context->sector_start = fat__cluster_to_sector_offset(context, nextCluster);
            context->sector_end = context->sector_start + context->bpb->sectors_per_cluster;
            context->sector_index = 0;
        }

        // printf("nextENT READ sector=0x%zx\n", (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE);
        res = DISK_read(context->device, (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return false;
        }

    
        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (int subEntryIndex=0;subEntryIndex<entries_len;subEntryIndex++) {
            fat__DirectoryEntry* entry = &entries[subEntryIndex];

            if (entry->file_name[0] == FAT_ENTRY_UNUSED || entry->file_name[0] == 0) {
                // Free spot in the middle of all entries.
                if (freeEntries == 0) {
                    target_sectorStart = context->sector_start;
                    target_sectorEnd = context->sector_end;
                    target_sectorIndex = context->sector_index;
                    target_subEntryIndex = subEntryIndex;
                }
                freeEntries++;
                if (freeEntries >= neededLFNEntries + 1) {
                    goto fill_entries;
                }
                if (entry->file_name[0] == 0) {
                    int remainingSectors = context->sector_end - context->sector_start - (context->sector_index+1);
                    int remainingEntriesInCluster = entries_len - subEntryIndex + remainingSectors * entries_len;
                    if (freeEntries + remainingEntriesInCluster >= neededLFNEntries + 1) {
                        goto fill_entries;
                    }
                    // We need another cluster to fit entries.
                    break;
                }
                continue;
            }

            // We broke the consecutive streak.
            freeEntries = 0;
        }
        context->sector_index++;
    }


fill_entries:

    u64 prev_readByteOffset = 0;

    // If this fails then we will have written an incomplete entry.
    // We could try to undo our previous write (if any) but it may also fail.
    // Perhaps we should try to undo anyway.
    #define FLUSH_PREVIOUS_READ() if (prev_readByteOffset) { \
        res = DISK_write(context->device, prev_readByteOffset, SECTOR_SIZE, context->tempSector); \
        if (!res) { \
            printf("fat_make_entry: Disk write failed %.*s\n", subname.len, subname.ptr); \
            goto exit; \
        } \
    }

    int contentCluster = get_free_cluster(context);
    res = fat__set_fat(context, contentCluster, FAT_CLUSTER_EOF);
    if (!res) {
        goto exit;
    }

    // @TODO If we fail later then we'll be leaking the cluster we allocated.

    context->sector_start = target_sectorStart;
    context->sector_end   = target_sectorEnd;
    context->sector_index = target_sectorIndex;
    
    int subEntryIndex = target_subEntryIndex;

    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    int char_index = subname.len-1;

    while (1) {
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == FAT_CLUSTER_INVALID) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                goto exit;
            }
            u32 nextCluster = fat__get_fat(context, context->current_cluster);
            if (!VALID_CLUSTER(nextCluster)) {
                goto exit;
            }
            context->current_cluster = nextCluster;
            context->sector_start = fat__cluster_to_sector_offset(context, nextCluster);
            context->sector_end = context->sector_start + context->bpb->sectors_per_cluster;
            context->sector_index = 0;

        }

        // printf("READ sector=0x%zx\n", (context->start_lba + sector_start + sector_index) * SECTOR_SIZE);
        FLUSH_PREVIOUS_READ()
        prev_readByteOffset = (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE;
        res = DISK_read(context->device, prev_readByteOffset, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return FAT_ID_NULL;
        }

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        int remainingLFNEntries = neededLFNEntries;

        for (;subEntryIndex<entries_len;subEntryIndex++) {
            fat__DirectoryEntry* entry = &entries[subEntryIndex];

            if (remainingLFNEntries) {
                memset(entry, 0, sizeof(*entry));
                fat__LongNameEntry* nameEntry = (void*)entry;
                nameEntry->attributes = fat__LFN;
                nameEntry->checksum = 0; // @TODO Fix
                if (char_index == subname.len-1) {
                    nameEntry->order = fat__LAST_LONG_ENTRY | remainingLFNEntries;
                } else {
                    nameEntry->order = remainingLFNEntries;
                }

                for (int ci = char_index % fat__LFN_MAX_CHARS; ci >= 0; ci--) {
                    u16 chr;
                    if (ci >= 0 && ci < 5) {
                        chr = nameEntry->file_name0[ci] = subname.ptr[char_index];
                        char_index--;
                    } else if (ci >= 5 && ci < 5+6) {
                        chr = nameEntry->file_name1[ci-5] = subname.ptr[char_index];
                        char_index--;
                    } else if (ci >= 5+6 && ci < 5+6+2) {
                        chr = nameEntry->file_name2[ci-5-6] = subname.ptr[char_index];
                        char_index--;
                    }
                }

                remainingLFNEntries--;

            } else {
                if (attributes & fat__DIRECTORY) {
                    entry->attributes = fat__DIRECTORY;
                    entry->file_size = context->bpb->sectors_per_cluster * context->sector_size;
                } else {
                    entry->attributes = 0;
                    entry->file_size = 0;
                }
                // @TODO Set times

                entry->cluster_low = contentCluster & 0xFFFF;
                entry->cluster_high = contentCluster >> 16;

                if (neededLFNEntries) {
                    for (int i=0;i<6;i++) {
                        entry->file_name[i] = subname.ptr[i];
                    }
                    entry->file_name[6] = '~';
                    entry->file_name[7] = '1';

                    int dot_index = -1;
                    for (int i=subname.len-1;i>=0;i--) {
                        char chr = subname.ptr[i];
                        if (chr == '.') {
                            dot_index = i;
                            break;
                        }
                    }
                    entry->file_name[8] = ' ';
                    entry->file_name[9] = ' ';
                    entry->file_name[10] = ' ';
                    if (dot_index == -1) {
                        // no extension
                    } else {
                        for (int i = 0; i < (subname.len - (dot_index + 1)) && i < 3; i++) {
                            entry->file_name[8 + i] = subname.ptr[dot_index + 1 + i];
                        }
                    }
                } else {
                    int dot_index = -1;
                    for (int i=subname.len-1;i>=0;i--) {
                        char chr = subname.ptr[i];
                        if (chr == '.') {
                            dot_index = i;
                            break;
                        }
                    }
                    for (int i = 0; i < 8; i++) {
                        if (i < subname.len && (dot_index == -1 || i < dot_index)) {
                            entry->file_name[i] = subname.ptr[i];
                        } else {
                            entry->file_name[i] = ' ';
                        }
                    }
                    if (dot_index == -1) {
                        entry->file_name[8] = ' ';
                        entry->file_name[9] = ' ';
                        entry->file_name[10] = ' ';
                    } else {
                        for (int i = 0; i < (subname.len - (dot_index + 1)); i++) {
                            entry->file_name[8 + i] = subname.ptr[dot_index + 1 + i];
                        }
                    }
                }

                FLUSH_PREVIOUS_READ()
                prev_readByteOffset = 0;

                if (attributes & fat__DIRECTORY) {
                    // Clear directory entries of newly made directory
                    memset(context->tempSector, 0, SECTOR_SIZE);
                    int dir_sector_start = fat__cluster_to_sector_offset(context, contentCluster);
                    for (int i = 0; i < context->bpb->sectors_per_cluster; i++) {
                        // printf("CLEARING sector=0x%zx\n", (context->start_lba + dir_sector_start + i) * context->sector_size);
                        res = DISK_write( context->device,
                            (context->start_lba + dir_sector_start + i) * context->sector_size,
                            context->sector_size, context->tempSector);
                    }
                }

                returnValue = fat_encode_id(context, context->current_cluster, target_sectorIndex, target_subEntryIndex, contentCluster);
                goto exit;
            }
        }
        subEntryIndex = 0;
        context->sector_index++;
    }

exit:
    return returnValue;
    #undef FLUSH_PREVIOUS_READ
}

bool fat_remove_entry(VFS_Mount* mount, FAT_ID id) {
    FAT_ID returnValue = FAT_ID_NULL;

    FATContext _context;
    FATContext* context = &_context;
    init_context(context, mount);


    u32 subEntryIndex = 0;
    u32 contentCluster;

    int res;
    fat_decode_id(context, id, &context->current_cluster, &context->sector_index, &subEntryIndex, &context->sector_start, &context->sector_end, &contentCluster, NULL, NULL);

    u32 nextCluster = contentCluster;
    while (VALID_CLUSTER(nextCluster)) {
        u32 tempCluster = nextCluster;
        nextCluster = fat__get_fat(context, nextCluster);
        fat__set_fat(context, tempCluster, FAT_CLUSTER_UNUSED);
    }




    u64 prev_readByteOffset = 0;

    // If this fails then we will have written an incomplete entry.
    // We could try to undo our previous write (if any) but it may also fail.
    // Perhaps we should try to undo anyway.
    #define FLUSH_PREVIOUS_READ() if (prev_readByteOffset) { \
        res = DISK_write(context->device, prev_readByteOffset, SECTOR_SIZE, context->tempSector); \
        if (!res) { \
            printf("fat_remove_entry: Disk write failed\n"); \
            goto exit; \
        } \
    }


    while (1) {
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == FAT_CLUSTER_INVALID) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                goto exit;
            }
            u32 nextCluster = fat__get_fat(context, context->current_cluster);
            if (!VALID_CLUSTER(nextCluster)) {
                goto exit;
            }
            context->current_cluster = nextCluster;
            context->sector_start = fat__cluster_to_sector_offset(context, nextCluster);
            context->sector_end = context->sector_start + context->bpb->sectors_per_cluster;
            context->sector_index = 0;

        }

        // printf("nextENT READ sector=0x%zx\n", (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE);
        FLUSH_PREVIOUS_READ();
        prev_readByteOffset = (mount->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE;
        res = DISK_read(mount->diskDevice, prev_readByteOffset, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("fat_remove_entry: Could not read\n");
                goto exit;
        }

    
        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (;subEntryIndex<entries_len;subEntryIndex++) {
            fat__DirectoryEntry* entry = &entries[subEntryIndex];

            if (entry->attributes == fat__LFN) {
                memset(entry, 0, sizeof(*entry));
                entry->file_name[0] = FAT_ENTRY_UNUSED;
                continue;
            } else {
                // We assumwe disk isn't corrupt and contains the coressponding non LFN entry here.
                memset(entry, 0, sizeof(*entry));
                entry->file_name[0] = FAT_ENTRY_UNUSED;

                FLUSH_PREVIOUS_READ();

                returnValue = true;
                goto exit;
            }
        }
        subEntryIndex = 0;
        context->sector_index++;
    }
exit:
    return returnValue;
    #undef FLUSH_PREVIOUS_READ
}


u64 fat_read(VFS_Mount* mount, FAT_ID file, u64 offset, u64 size, void* buffer) {

    int res;

    #define SECTOR_SIZE 512

    FATContext _ctx = {0};
    FATContext* context = &_ctx;


    init_context(context, mount);


    u32 cluster;
    u32 subEntryIndex;
    fat_decode_id(context, file, &context->current_cluster, &context->sector_index, &subEntryIndex, &context->sector_start, &context->sector_end, &cluster, NULL, NULL);

    fat__DirectoryEntry* direntryBlock = (fat__DirectoryEntry*)(context->tempSector);

    res = DISK_read(mount->diskDevice, (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE, SECTOR_SIZE, direntryBlock);
    if (!res) return 0;

    fat__DirectoryEntry* direntry = &direntryBlock[subEntryIndex];
    u64 fileSize = direntry->file_size;

    // printf("read bytes=%d fiSz=%zd %s %u, %zu %u\n", size, fileSize, direntry->file_name, cluster, context->sector_start + context->sector_index, subEntryIndex);

    // @TODO Update access time

    int advance_clusters = offset / (context->bpb->bytes_per_sector * context->bpb->sectors_per_cluster);

    while (advance_clusters) {
        advance_clusters--;
        uint32_t next_cluster = fat__get_fat(context, cluster);
        if (!VALID_CLUSTER(next_cluster)) {
            // Offset points beyond the file, nothing to read.
            printf("read_fat: Can't read beyond file\n");
            return 0;
        } else {
            cluster = next_cluster;
        }
    }

    char data_sector[512];

    int sector_index = (offset/context->sector_size) % context->bpb->sectors_per_cluster;
    uint64_t buffer_offset = 0;
    while (buffer_offset < size) {

        if (sector_index >= context->bpb->sectors_per_cluster) {
            uint32_t next_cluster = fat__get_fat(context, cluster);
            if (next_cluster == -1) {
                printf("read_fat: Disk read failed, clusters %d -> %d\n", cluster, next_cluster);
                return buffer_offset;
            } else if (next_cluster == FAT_CLUSTER_EOF) {
                // No more to read, return what we did read.
                // printf("No more clusters, %d -> %d\n", cluster, next_cluster);
                return buffer_offset;
            } else {
                cluster = next_cluster;
                sector_index = 0;
            }
        }

        int sector_offset = fat__cluster_to_sector_offset(context, cluster) + sector_index;

        int alignment = (offset + buffer_offset) % context->sector_size;

        // @TODO Don't read more than file size.

        if ((alignment != 0) || (buffer_offset + context->sector_size > size) || (buffer_offset + offset + context->sector_size > fileSize)) {
            // writing a partial sector.
            // We must read the sector
            // memcpy in our partial data to write then
            // do a full sector write
            res = DISK_read(mount->diskDevice, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, data_sector);
            if (!res) {
                printf("read_fat: Failed read at sector %d\n", context->start_lba + sector_offset);
                return buffer_offset;
            }

            int part_size = context->sector_size - alignment;
            if (part_size > size - buffer_offset) {
                part_size = size - buffer_offset;
            }
            if (part_size > fileSize - (offset + buffer_offset)) {
                part_size = fileSize - (offset + buffer_offset);
            }
            memcpy((char*)buffer + buffer_offset, data_sector + alignment, part_size);

            buffer_offset += part_size;
            sector_index++;
        } else {
            res = DISK_read(mount->diskDevice, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, (char*)buffer + buffer_offset);
            if (!res) {
                printf("read_fat: Failed read at sector %d\n", context->start_lba + sector_offset);
                return buffer_offset;
            }

            buffer_offset += context->sector_size;
            sector_index++;
        }
    }

    return buffer_offset;
}

u64 fat_write(VFS_Mount* mount, FAT_ID file, u64 offset, u64 size, const void* buffer) {
    int res;

    #define SECTOR_SIZE 512

    FATContext _ctx = {0};
    FATContext* context = &_ctx;

    init_context(context, mount);

    u32 cluster;
    u32 subEntryIndex;
    fat_decode_id(context, file, &context->current_cluster, &context->sector_index, &subEntryIndex, &context->sector_start, &context->sector_end, &cluster, NULL, NULL);

    int advance_clusters = offset / (context->bpb->bytes_per_sector * context->bpb->sectors_per_cluster);

    while (advance_clusters) {
        advance_clusters--;
        uint32_t next_cluster = fat__get_fat(context, cluster);
        
        if (next_cluster == FAT_CLUSTER_EOF) {
            // Extend file
            int freeCluster = get_free_cluster(context);
            if (freeCluster == -1) {
                return 0;
            }
            res = fat__set_fat(context, cluster, freeCluster);
            if (!res) return 0;
            res = fat__set_fat(context, freeCluster, FAT_CLUSTER_EOF);
            if (!res) return 0;
            cluster = freeCluster;
        } else if (!VALID_CLUSTER(next_cluster)) {
            // Disk read failed
            return 0;
        } else {
            cluster = next_cluster;
        }
    }

    char data_sector[512];

    int sector_index = (offset/context->sector_size) % context->bpb->sectors_per_cluster;
    uint64_t buffer_offset = 0;
    while (buffer_offset < size) {

        if (sector_index >= context->bpb->sectors_per_cluster) {
            uint32_t next_cluster = fat__get_fat(context, cluster);
            if (next_cluster == FAT_CLUSTER_EOF) {
                // Extend file
                int freeCluster = get_free_cluster(context);
                if (!VALID_CLUSTER(freeCluster)) {
                    goto update_entry;
                }
                res = fat__set_fat(context, cluster, freeCluster);
                if (!res) {
                    goto update_entry;
                }
                res = fat__set_fat(context, freeCluster, FAT_CLUSTER_EOF);
                if (!res) {
                    goto update_entry;
                }
                cluster = freeCluster;
                sector_index = 0;
            } else if (!VALID_CLUSTER(next_cluster)) {
                printf("write_fat: Disk read failed, clusters %d -> %d\n", cluster, next_cluster);
                goto update_entry;
            } else {
                cluster = next_cluster;
                sector_index = 0;
            }
        }

        int sector_offset = fat__cluster_to_sector_offset(context, cluster) + sector_index;

        int sectorByteAlignment = (offset + buffer_offset) % context->sector_size;

        u64 byteOffset = (mount->start_lba + sector_offset) * context->sector_size;
        if ((sectorByteAlignment != 0) || (buffer_offset + context->sector_size > size)) {
            // writing a partial sector.
            // We must read the sector
            // memcpy in our partial data to write then
            // do a full sector write
            u64 byteOffset = (mount->start_lba + sector_offset) * context->sector_size;
            res = DISK_read(mount->diskDevice, byteOffset, context->sector_size, data_sector);
            if (!res) {
                printf("write_fat: Failed read at 0x%zx\n", byteOffset);
                goto update_entry;
            }

            int part_size = context->sector_size - sectorByteAlignment;
            if (part_size > size - buffer_offset) {
                part_size = size - buffer_offset;
            }
            memcpy(data_sector + sectorByteAlignment, (char*)buffer + buffer_offset, part_size);

            res = DISK_write(mount->diskDevice, byteOffset, context->sector_size, data_sector);
            if (!res) {
                printf("write_fat: Failed write at 0x%zx\n", byteOffset);
                goto update_entry;
            }

            buffer_offset += part_size;
            sector_index++;
        } else {
            res = DISK_write(mount->diskDevice, byteOffset, context->sector_size, (char*)buffer + buffer_offset);
            if (!res) {
                printf("write_fat: Failed write at 0x%zx\n", byteOffset);
                goto update_entry;
            }

            buffer_offset += context->sector_size;
            sector_index++;
        }
    }

update_entry:

    u64 returnValue = 0; // @TODO Value to indicate failure.
    u64 prev_readByteOffset = 0;

    // If this fails then we will have written an incomplete entry.
    // We could try to undo our previous write (if any) but it may also fail.
    // Perhaps we should try to undo anyway.
    #define FLUSH_PREVIOUS_READ() if (prev_readByteOffset) { \
        res = DISK_write(context->device, prev_readByteOffset, SECTOR_SIZE, context->tempSector); \
        if (!res) { \
            printf("fat_remove_entry: Disk write failed\n"); \
            goto exit; \
        } \
    }


    while (1) {
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == FAT_CLUSTER_INVALID) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                goto exit;
            }
            u32 nextCluster = fat__get_fat(context, context->current_cluster);
            if (!VALID_CLUSTER(nextCluster)) {
                goto exit;
            }
            context->current_cluster = nextCluster;
            context->sector_start = fat__cluster_to_sector_offset(context, nextCluster);
            context->sector_end = context->sector_start + context->bpb->sectors_per_cluster;
            context->sector_index = 0;

        }

        prev_readByteOffset = (mount->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE;
        res = DISK_read(mount->diskDevice, prev_readByteOffset, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("fat_remove_entry: Could not read\n");
                goto exit;
        }

    
        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (;subEntryIndex<entries_len;subEntryIndex++) {
            fat__DirectoryEntry* entry = &entries[subEntryIndex];

            if (entry->attributes == fat__LFN) {
                continue;
            } else {
                // @TODO Update access time
                //       Modified time
                entry->file_size = offset + buffer_offset;

                FLUSH_PREVIOUS_READ();

                returnValue = buffer_offset;
                goto exit;
            }
        }
        subEntryIndex = 0;
        context->sector_index++;
    }
exit:
    return returnValue;
}



bool fat_info(VFS_Mount* mount, FAT_ID file, VFS_HandleInfo* info) {


    int res;
    char stackBuffer[512];
    int buffer_head = 0;
    int sectorSize = 512;

    FATContext _context;
    FATContext* context = &_context;
    init_context(context, mount);

    // printf("FAT INFO\n");

    u32 subEntryIndex;
    fat_decode_id(context, file, &context->current_cluster, &context->sector_index, &subEntryIndex, &context->sector_start, &context->sector_end, NULL, NULL, NULL);


    fat__DirectoryEntry* direntryBlock = (fat__DirectoryEntry*)(stackBuffer);

    memset(info, 0, sizeof(*info));
    
    res = DISK_read(mount->diskDevice, (mount->start_lba + context->sector_start + context->sector_index) * sectorSize, sectorSize, direntryBlock);
    if (!res) return false;

    fat__DirectoryEntry* entry = &direntryBlock[subEntryIndex];
    
    info->isDirectory = entry->attributes & fat__DIRECTORY;
    info->readOnly = entry->attributes & fat__READ_ONLY;
    info->fileSize = entry->file_size;
    info->blockSize = sectorSize;
    info->lastWriteTime_us = fat__sane_mtime(entry);
    return true;
}


bool fat_iterate(VFS_Mount* mount, FAT_ID directory, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* vfs_entries) {
    FAT_ID returnValue = FAT_ID_NULL;

    FATContext _context;
    FATContext* context = &_context;
    init_context(context, mount);

    int res;
    fat_decode_id(context, directory, NULL, NULL, NULL, NULL, NULL, &context->current_cluster, &context->sector_start, &context->sector_end);

    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    u32 entryIndex = (u32)*cookie;
    u32 maxEntries = *entryCount;
    u32 numEntries = 0;

    int entriesPerSector = SECTOR_SIZE / sizeof(fat__DirectoryEntry);
    context->sector_index = (entryIndex) / entriesPerSector;
    u32 subEntryIndex = (entryIndex) % entriesPerSector;

    while (1) {
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == FAT_CLUSTER_INVALID) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                goto exit;
            }
            u32 nextCluster = fat__get_fat(context, context->current_cluster);
            if (!VALID_CLUSTER(nextCluster)) {
                goto exit;
            }
            context->current_cluster = nextCluster;
            context->sector_start = fat__cluster_to_sector_offset(context, nextCluster);
            context->sector_end = context->sector_start + context->bpb->sectors_per_cluster;
            context->sector_index = 0;

        }

        // printf("nextENT READ sector=0x%zx\n", (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE);
        res = DISK_read(context->device, (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("Could not read\n");
            goto exit;
        }

    
        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (;subEntryIndex<entries_len;subEntryIndex++) {
            fat__DirectoryEntry* entry = &entries[subEntryIndex];

            if (entry->attributes == fat__LFN) {
                fat__LongNameEntry* nameEntry = (fat__LongNameEntry*)entry;
                if (nameEntry->order & fat__LAST_LONG_ENTRY) {
                    longName_startIndex = longName_lastIndex;
                }
                for (int ci=5+6+2-1;ci>=0;ci--) {
                    u16 chr;
                    if (ci >= 0 && ci < 5) {
                        chr = nameEntry->file_name0[ci];
                    } else if (ci >= 5 && ci < 5+6) {
                        chr = nameEntry->file_name1[ci-5];
                    } else if (ci >= 5+6 && ci < 5+6+2) {
                        chr = nameEntry->file_name2[ci-5-6];
                    }

                    if (chr == 0 || chr == 0xFFFF || (chr == ' '
                        && longName_startIndex == longName_lastIndex))
                    {
                        // End character.
                        longName_startIndex = longName_lastIndex;
                    } else {
                        longName_startIndex--;
                        longName_buffer[longName_startIndex] = chr;
                    }
                }
                continue;
            }
            if (entry->file_name[0] == FAT_ENTRY_UNUSED) {
                // Not present
                continue;
            }
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                returnValue = true;
                subEntryIndex++;
                goto exit;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ')
                continue;
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ')
                continue;
            

            cstring entryName = { 0 };
            if (longName_startIndex != longName_lastIndex) {
                entryName = (cstring){
                    .ptr = longName_buffer + longName_startIndex,
                    .len = longName_lastIndex - longName_startIndex,
                };
                longName_startIndex = longName_lastIndex;
            } else {
                entryName.ptr = longName_buffer;
                entryName.len = fat__sane_cstring(entry, longName_buffer);
            }

            ELOS_DirectoryEntry* vfsEntry = &vfs_entries[numEntries];
            numEntries++;

            vfsEntry->fileSize = entry->file_size;
            vfsEntry->isDirectory = entry->attributes & fat__DIRECTORY;
            vfsEntry->isReadOnly = entry->attributes & fat__READ_ONLY;
            vfsEntry->name_len = snprintf(vfsEntry->name, sizeof(vfsEntry->name), "%.*s", entryName.len, entryName.ptr);
            vfsEntry->lastWriteTime_us = 0; // @TODO Last write time

            // We check this here instead of before we add entry because we don't allow 0 as max entries.
            if (numEntries >= maxEntries) {
                returnValue = true;
                subEntryIndex++;
                goto exit;
            }
        }
        subEntryIndex = 0;
        context->sector_index++;
    }

exit:
    *entryCount = numEntries;
    *cookie = subEntryIndex + context->sector_index * entriesPerSector;
    return returnValue;
}



bool fat_is_fat(VFS_Mount* mount) {
    FATContext _context;
    FATContext* context = &_context;
    init_context(context, mount);
    return context->fat_version == fat__FAT32 || context->fat_version == fat__FAT12 || context->fat_version == fat__FAT16;
}




int required_long_name_entries(const cstring name) {
    int dot_index = -1;
    for (int i=name.len-1;i>=0;i--) {
        char chr = name.ptr[i];
        if (chr == '.') {
            dot_index = i;
            break;
        }
    }
    if (dot_index == -1) {
        if (name.len <= 8) {
            return 0;
        }
    } else {
        if (dot_index <= 8 && name.len - (dot_index+1) <= 3) {
            return 0;
        }
    }
    return (name.len + 12) / 13;
}



int fat__get_fat(FATContext* context, int cluster) {
    int res;
    
    uint32_t value = -1;

    char tempBuffer[2*512];

    if (context->fat_version == fat__FAT16 || context->fat_version == fat__FAT32) {
        // Clean cluster number divide, FAT entry can't span across sectors.
        int sector_index = context->bpb->reserved_sectors + cluster / context->fat_entries_per_sector;
        res = DISK_read(context->device, (context->start_lba + sector_index) * context->sector_size, context->sector_size, tempBuffer);
        if (!res) return FAT_CLUSTER_INVALID;

        void* sector_buffer = tempBuffer;
        int entry_index = cluster % context->fat_entries_per_sector;

        if (context->fat_version == fat__FAT32) {
            value = ((uint32_t*)sector_buffer)[entry_index];
        } else if (context->fat_version == fat__FAT16) {
            value = ((uint16_t*)sector_buffer)[entry_index];
            if (value == 0xFFFF)
                value = FAT_CLUSTER_EOF;
            if (value == 0xFFF8)
                value = FAT_CLUSTER_RESERVED;
        }
    } else {
        int fat_offset = cluster + cluster / 2;
        
        int sector_index = context->bpb->reserved_sectors + fat_offset / context->sector_size;

        res = DISK_read(context->device, (context->start_lba + sector_index) * context->sector_size, 2 * context->sector_size, tempBuffer);
        if (!res) return FAT_CLUSTER_INVALID;

        char* sector_buffer = tempBuffer;

        if (cluster & 1) {
            // check odd or even
            value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) >> 4;
        } else {
            value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) & 0x0FFF;
        }
      
        if (value == 0xFFF)
            value = FAT_CLUSTER_EOF;
        if (value == 0xFF8)
            value = FAT_CLUSTER_RESERVED;
    }

    return value;
}
int fat__set_fat(FATContext* context, int cluster, uint32_t value) {
    int res;

    char tempBuffer[2 * 512];

    if (context->fat_version == fat__FAT16 ||
        context->fat_version == fat__FAT32)
    {
        int sector_index =
            context->bpb->reserved_sectors +
            cluster / context->fat_entries_per_sector;

        res = DISK_read(
            context->device,
            (context->start_lba + sector_index) * context->sector_size,
            context->sector_size,
            tempBuffer);

        if (!res)
            return false;

        void* sector_buffer = tempBuffer;
        int entry_index = cluster % context->fat_entries_per_sector;

        if (context->fat_version == fat__FAT32)
        {
            ((uint32_t*)sector_buffer)[entry_index] = value;
        }
        else
        {
            ((uint16_t*)sector_buffer)[entry_index] =
                (uint16_t)value;
        }

        res = DISK_write(
            context->device,
            (context->start_lba + sector_index) * context->sector_size,
            context->sector_size,
            tempBuffer);

        return res;
    }
    else
    {
        int fat_offset = cluster + cluster / 2;

        int sector_index =
            context->bpb->reserved_sectors +
            fat_offset / context->sector_size;

        res = DISK_read(
            context->device,
            (context->start_lba + sector_index) * context->sector_size,
            2 * context->sector_size,
            tempBuffer);

        if (!res)
            return false;

        char* sector_buffer = tempBuffer;

        uint16_t* entry =
            (uint16_t*)&sector_buffer[
                fat_offset % context->sector_size
            ];

        uint16_t current = *entry;

        if (cluster & 1)
        {
            // odd cluster
            current &= 0x000F;
            current |= (value & 0x0FFF) << 4;
        }
        else
        {
            // even cluster
            current &= 0xF000;
            current |= value & 0x0FFF;
        }

        *entry = current;

        res = DISK_write(
            context->device,
            (context->start_lba + sector_index) * context->sector_size,
            2 * context->sector_size,
            tempBuffer);

        return res;
    }
}

