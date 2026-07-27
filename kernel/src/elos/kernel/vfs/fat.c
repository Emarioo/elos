
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
#include "elos/kernel/vfs/fat_impl.h"

#include "elos/kernel/vfs/vfile_system.h"

#define printf(...) KCON_printf(__VA_ARGS__)

#define SECTOR_SIZE 512


VFS_Handle_impl* reserve_handle();

int find_slash(const cstring path, int offset);


#define SET_VARS                                                                  \
    if (context->fat_version == fat__FAT32) {                                     \
        current_cluster = context->ebpb32->root_cluster;                          \
        sector_start = fat__cluster_to_sector_offset(context, current_cluster);   \
        sector_end = sector_start + context->bpb->sectors_per_cluster;            \
        sector_index = 0;                                                         \
    } else {                                                                      \
        current_cluster = -1;                                                     \
        sector_index = 0;                                                         \
        sector_start = context->bpb->reserved_sectors                             \
            + context->sectors_per_fat * context->bpb->fat_count;                 \
        sector_end = sector_start + (context->bpb->root_dir_entries               \
            * sizeof(fat__DirectoryEntry)) / context->sector_size;                \
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



bool fat_mkdir(VFS_Mount* mount, const cstring path) {
    int res;

    if (path.len == 1 && path.ptr[0] == '/') {
        // root directory always exists
        return true;
    }

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
    init_context(context, mount);

    FAT_ID rootDir = get_root_directory(context);

    int path_index = 0;
    cstring subname = { 0 };
    int slash_pos = 0;

    #define NEXT_SUBNAME                                   \
        path_index = slash_pos;                            \
        if (subname.ptr != path.ptr + path_index) {        \
            if (path.ptr[path_index] != '/') {             \
                printf("Expeting slash!\n");              \
                return false;                               \
            }                                              \
            path_index++;                                  \
            slash_pos = find_slash(path, path_index);  \
            if (slash_pos == -1) {                         \
                subname.ptr = path.ptr + path_index;       \
                subname.len = path.len - path_index;       \
                path_index = path.len;                     \
            } else {                                       \
                subname.ptr = path.ptr + path_index;       \
                subname.len = slash_pos - path_index;      \
                path_index = slash_pos;                    \
            }                                              \
        }


    FAT_ID currentDir = rootDir;
    
    while (1) {
        NEXT_SUBNAME
        FAT_ID nextDir = find_directory(context, currentDir, subname);

        if (!nextDir) {
            nextDir = create_fat_entry(context, currentDir, subname, true);
            if (!nextDir) {
                printf("Could not make directory! %s\n", subname.ptr);
                break;
            }
        }
        currentDir = nextDir;
    
        if (slash_pos == -1) {
            return true;
        }
    }

    return false;
    #undef NEXT_SUBNAME
}


VFS_FileObject* fat_mkfile(VFS_Mount* mount, const cstring path) {
    int res;

    if (path.len == 1 && path.ptr[0] == '/') {
        // Can't create root directory
        return NULL;
    }

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
    init_context(context, mount);

    FAT_ID rootDir = get_root_directory(context);

    int path_index = 0;
    cstring subname = { 0 };
    int slash_pos = 0;

    #define NEXT_SUBNAME                                   \
        path_index = slash_pos;                            \
        if (subname.ptr != path.ptr + path_index) {        \
            if (path.ptr[path_index] != '/') {             \
                printf("Expeting slash!\n");               \
                return NULL;                               \
            }                                              \
            path_index++;                                  \
            slash_pos = find_slash(path, path_index);      \
            if (slash_pos == -1) {                         \
                subname.ptr = path.ptr + path_index;       \
                subname.len = path.len - path_index;       \
                path_index = path.len;                     \
            } else {                                       \
                subname.ptr = path.ptr + path_index;       \
                subname.len = slash_pos - path_index;      \
                path_index = slash_pos;                    \
            }                                              \
        }


    FAT_ID currentDir = rootDir;
    
    while (1) {
        NEXT_SUBNAME
        FAT_ID nextDir = find_directory(context, currentDir, subname);

        if (nextDir) {
            if (slash_pos == -1) {
                return NULL;
            }
            currentDir = nextDir;
            continue;
        }

        if (slash_pos != -1) {
            return NULL;
        }
        FAT_ID fileContent = create_fat_entry(context, currentDir, subname, false);
        if (fileContent == FAT_ID_NULL) {
            printf("Could not make file! %s\n", subname.ptr);
            break;
        }
        int clusterIndex;
        int sector_start;
        int sectorEnd;
        extract_fat_id(context, fileContent, &clusterIndex, &sector_start, &sectorEnd);

        VFS_FileObject* fileObject = find_file_object(mount, clusterIndex);
        fileObject->direntryIndex = context->entries_index;
        fileObject->direntrySector = context->sector_start + context->sector_index;
        return fileObject;
    }

    return NULL;
    #undef NEXT_SUBNAME
}


VFS_FileObject fileObjects[1000];
// int            fileObjects_len;
int            fileObjects_cap = ARRAY_LENGTH(fileObjects);


VFS_FileObject* find_file_object(VFS_Mount* mount, u32 clusterIndex) {
    // @TODO Use hash map of some sort. Maybe binary search.
    VFS_FileObject* free_obj = NULL;
    for (int i = 0; i < fileObjects_cap; i++) {
        VFS_FileObject* obj = &fileObjects[i];
        if (obj->clusterIndex == clusterIndex && obj->mount->diskDevice == mount->diskDevice && obj->mount->start_lba == mount->start_lba) {
            return obj;
        }
        if (fileObjects[i].clusterIndex == 0) {
            free_obj = obj;
            break;
        }
    }
    if (free_obj) {
        free_obj->clusterIndex = clusterIndex;
        free_obj->mount = mount;
    }
    return free_obj;
}

bool iter_fat(VFS_Mount* mount, VFS_FileObject* fileObject, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* entries) {
    int res;

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
    init_context(context, mount);

    char stackBuffer[512];
    int buffer_head = 0;
    int sectorSize = context->sector_size;

    FAT_ID currentDir;
    if (fileObject->isRootDirectory) {
        currentDir = get_root_directory(context);
    } else {
        fat__DirectoryEntry* direntryBlock = (fat__DirectoryEntry*)(stackBuffer + buffer_head);
        buffer_head += sectorSize;
        
        res = DISK_read(fileObject->mount->diskDevice, (fileObject->mount->start_lba + fileObject->direntrySector) * sectorSize, sectorSize, direntryBlock);
        if (!res) return false;

        fat__DirectoryEntry* entry = &direntryBlock[fileObject->direntryIndex];

        u32 currentCluster = (u32)entry->cluster_low | (u32)entry->cluster_high << 16;
        currentDir = make_fat_id(context, currentCluster);
    }

    // @TODO Validate that cookie is valid to some degree. If it's 78
    //    by mistake or accident by user then that may be fine
    //    if we have that many entries in the directory.
    //    If it's beyond directory then it's the end, but still valid i suppose.

    u32 entryIndex = (u32)*cookie;
    char longName[256];

    u32 maxEntries = *entryCount;
    u32 numEntries = 0;
    
    while (numEntries < maxEntries) {
        fat__DirectoryEntry* entry = fat_next_entry(context, currentDir, &entryIndex, longName);

        cstring entryName = {
            .ptr = longName,
            .len = strlen(longName),
        };

        if (!entry) {
            // No more entries
            break;
        }

        ELOS_DirectoryEntry* vfsEntry = &entries[numEntries];
        numEntries++;

        vfsEntry->fileSize = entry->file_size;
        vfsEntry->isDirectory = entry->attributes & fat__DIRECTORY;
        vfsEntry->isReadOnly = entry->attributes & fat__READ_ONLY;
        vfsEntry->name_len = snprintf(vfsEntry->name, sizeof(vfsEntry->name), "%s", entryName.ptr);
        vfsEntry->lastWriteTime_us = 0; // @TODO Last write time
    }
    *entryCount = numEntries;
    *cookie = entryIndex;

    return true;
}


VFS_FileObject* search_fat(VFS_Mount* mount, const cstring path) {
    int res;

    DiskDevice device = mount->diskDevice;
    u64 start_lba = mount->start_lba;

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
    init_context(context, mount);

    if (path.len == 0 || (path.len == 1 && path.ptr[0] == '/')) {
        // 1 is an invalid clusterIndex, 0 means unused file object.
        // All code for FAT could use some refactoring.
        VFS_FileObject* fileObject = find_file_object(mount, 1);
        fileObject->isRootDirectory = true;
        return fileObject;
    }

    int current_cluster;
    int sector_start;
    int sector_end;
    int sector_index;

    SET_VARS


    char tempSector[512];
    int entry_number = 0;

    int path_index = 0;
    cstring subname = { 0 };
    int slash_pos = -1;

    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    while (1) {
        
        if (subname.ptr != path.ptr + path_index) {
            if (path.ptr[path_index] != '/') {
                printf("Expecting slash!\n");
                return NULL;
            }
            path_index++;
            
            slash_pos = find_slash(path, path_index);
            if (slash_pos == -1) {
                // No slash
                subname.ptr = path.ptr + path_index;
                subname.len = path.len - path_index;
                path_index = path.len;
            } else {
                subname.ptr = path.ptr + path_index;
                subname.len = slash_pos - path_index;
                path_index = slash_pos;
            }
        }

        res = DISK_read(device, (start_lba + sector_start + sector_index) * SECTOR_SIZE, SECTOR_SIZE, tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return NULL;
        }

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (int i=0;i<entries_len;i++) {
            fat__DirectoryEntry* entry = &entries[i];
            entry_number++;

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
            if (entry->file_name[0] == 0xE5) {
                // Not present
                continue;
            }
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                sector_index = sector_end - sector_start - 1;
                break;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ')
                continue;
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ')
                continue;
            
            bool same;
            if (longName_startIndex != longName_lastIndex) {
                cstring entryName2 = {
                    .ptr = longName_buffer + longName_startIndex,
                    .len = longName_lastIndex - longName_startIndex,
                };
                longName_startIndex = longName_lastIndex;
                same = fat__string_equal(subname, entryName2);
            } else {
                char entryName[20];
                int  entryName_len = fat__sane_cstring(entry, entryName);
                cstring entryName2 = { entryName, entryName_len };
                same = fat__string_equal(subname, entryName2);
            }

            if (!same) {
                continue;
            }

            if (slash_pos == -1) {
                // printf("Found file/directory %s\n", subname);

                u32 clusterIndex = entry->cluster_low | (entry->cluster_high << 16);

                // Check if (diskDevice,clusterIndex) refers to a VFS_VirtualNode already.
                // If not create on with that identity.

                // VFS_VirtualNode refers to a file object.

                // If we move a directory entry to another file then cluster still refers to the correct
                // file. The VFS_VirtualNode refers to directoryEntrySector + directoryEntryIndex which is updated
                // when we move. If we find a node with the cluster info then we can move it.

                VFS_FileObject* fileObject = find_file_object(mount, clusterIndex);
                fileObject->direntryIndex = i;
                fileObject->direntrySector = sector_start + sector_index;
      
                return fileObject;
            }

            if ((entry->attributes & fat__DIRECTORY) == 0) {
                char temp_name[256];
                memcpy(temp_name, subname.ptr, subname.len);
                temp_name[subname.len] = '\0';
                printf("Sub path refers to file not directory, can't go deeper %s\n", temp_name);
                return NULL;
            }

            // Found directory, go deeper
            current_cluster = (int)entry->cluster_low | ((int)entry->cluster_high << 16);
            sector_index = -1; // incremented later
            sector_start = fat__cluster_to_sector_offset(context, current_cluster);
            sector_end = sector_start + context->bpb->sectors_per_cluster;

            path_index = slash_pos;
            break;
        }
        sector_index++;

        if (sector_start + sector_index >= sector_end) {
            // printf("Could not find %s\n", path.ptr);
            return NULL;
        }
    }
    return NULL;
}




u64 read_fat(VFS_Handle_impl* handle, u64 offset, u64 size, void* buffer) {
    int res;

    #define SECTOR_SIZE 512

    DiskDevice device = handle->fileObject->mount->diskDevice;
    u64        start_lba = handle->fileObject->mount->start_lba;

    VFS_FileObject* fileObject = handle->fileObject;

    u8 stackBuffer[2*512];
    int buffer_head = 0;


    FATContext _ctx = {0};
    FATContext* context = &_ctx;


    init_context(context, handle->fileObject->mount);


    fat__DirectoryEntry* direntryBlock = (fat__DirectoryEntry*)(stackBuffer + buffer_head);
    buffer_head += SECTOR_SIZE;

    res = DISK_read(device, (start_lba + fileObject->direntrySector) * SECTOR_SIZE, SECTOR_SIZE, direntryBlock);
    if (!res) return 0;

    fat__DirectoryEntry* direntry = &direntryBlock[fileObject->direntryIndex];
    u64 fileSize = direntry->file_size;

    // @TODO Update access time

    uint32_t cluster = fileObject->clusterIndex;
    int advance_clusters = offset / (context->bpb->bytes_per_sector * context->bpb->sectors_per_cluster);

    while (advance_clusters) {
        advance_clusters--;
        uint32_t next_cluster = fat__get_fat(context, cluster);
        if (next_cluster == fat__END_OF_FILE) {
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
            } else if (next_cluster == fat__END_OF_FILE) {
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
            res = DISK_read(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, data_sector);
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
            res = DISK_read(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, (char*)buffer + buffer_offset);
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


u64 write_fat(VFS_Handle_impl* handle, u64 offset, u64 size, const void* buffer) {
     int res;

    #define SECTOR_SIZE 512

    DiskDevice device = handle->fileObject->mount->diskDevice;
    u64        start_lba = handle->fileObject->mount->start_lba;

    VFS_FileObject* fileObject = handle->fileObject;

    u8 stackBuffer[2*512];
    int buffer_head = 0;


    FATContext _ctx = {0};
    FATContext* context = &_ctx;


    init_context(context, handle->fileObject->mount);

    // @TODO Error checkking on set_fat/get_fat, they read disk and can return false


    fat__DirectoryEntry* direntryBlock = (fat__DirectoryEntry*)(stackBuffer + buffer_head);
    buffer_head += SECTOR_SIZE;

    res = DISK_read(device, (start_lba + fileObject->direntrySector) * SECTOR_SIZE, SECTOR_SIZE, direntryBlock);
    if (!res) return 0;

    fat__DirectoryEntry* direntry = &direntryBlock[fileObject->direntryIndex];
    u64 fileSize = direntry->file_size;

    // @TODO Update access time
    //       Modified time

    uint32_t cluster = fileObject->clusterIndex;
    // We assume cluster is valid? bad?
    if (cluster <= 1 || cluster == -1) {
        printf("write_fat: fileObject->clusterIndex = %d BAD!\n", cluster);
        return 0;
    }
    int advance_clusters = offset / (context->bpb->bytes_per_sector * context->bpb->sectors_per_cluster);

    while (advance_clusters) {
        advance_clusters--;
        uint32_t next_cluster = fat__get_fat(context, cluster);
        if (next_cluster == -1) {
            // Disk read failed
            return 0;
        }
        if (next_cluster == fat__END_OF_FILE) {
            // Extend file
            int freeCluster = get_free_cluster(context);
            if (freeCluster == -1) {
                return 0;
            }
            res = fat__set_fat(context, cluster, freeCluster);
            if (!res) return 0;
            res = fat__set_fat(context, freeCluster, fat__END_OF_FILE);
            if (!res) return 0;
            cluster = freeCluster;
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
                // Disk read failed
                printf("write_fat: Disk read failed, clusters %d -> %d\n", cluster, next_cluster);
                return buffer_offset;
            } else if (next_cluster == fat__END_OF_FILE) {
                // Extend file
                int freeCluster = get_free_cluster(context);
                if (freeCluster == -1) {
                    return 0;
                }
                res = fat__set_fat(context, cluster, freeCluster);
                if (!res) return 0;
                res = fat__set_fat(context, freeCluster, fat__END_OF_FILE);
                if (!res) return 0;
                cluster = freeCluster;
                sector_index = 0;
            } else {
                cluster = next_cluster;
                sector_index = 0;
            }
        }

        int sector_offset = fat__cluster_to_sector_offset(context, cluster) + sector_index;

        int sectorByteAlignment = (offset + buffer_offset) % context->sector_size;

        if ((sectorByteAlignment != 0) || (buffer_offset + context->sector_size > size)) {
            // writing a partial sector.
            // We must read the sector
            // memcpy in our partial data to write then
            // do a full sector write
            res = DISK_read(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, data_sector);
            if (!res) {
                printf("write_fat: Failed read at sector %d\n", context->start_lba + sector_offset);
                return buffer_offset;
            }

            int part_size = context->sector_size - sectorByteAlignment;
            if (part_size > size - buffer_offset) {
                part_size = size - buffer_offset;
            }
            memcpy(data_sector + sectorByteAlignment, (char*)buffer + buffer_offset, part_size);

            res = DISK_write(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, data_sector);
            if (!res) {
                printf("write_fat: Failed write at sector %d\n", context->start_lba + sector_offset);
                return buffer_offset;
            }

            buffer_offset += part_size;
            sector_index++;
        } else {
            res = DISK_write(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, (char*)buffer + buffer_offset);
            if (!res) {
                printf("write_fat: Failed write at sector %d\n", context->start_lba + sector_offset);
                return buffer_offset;
            }

            buffer_offset += context->sector_size;
            sector_index++;
        }
    }

    direntry->file_size = offset + buffer_offset;

    res = DISK_write(device, (start_lba + fileObject->direntrySector) * SECTOR_SIZE, SECTOR_SIZE, direntryBlock);
    if (!res) return 0;

    return buffer_offset;
}


bool fat_remove(VFS_Mount* mount, const cstring path) {
    int res;

    if (path.len == 1 && path.ptr[0] == '/') {
        // root directory always exists
        return true;
    }

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
    init_context(context, mount);

    FAT_ID rootDir = get_root_directory(context);

    int path_index = 0;
    cstring subname = { 0 };
    int slash_pos = 0;

    #define NEXT_SUBNAME                                   \
        path_index = slash_pos;                            \
        if (subname.ptr != path.ptr + path_index) {        \
            if (path.ptr[path_index] != '/') {             \
                printf("Expeting slash!\n");              \
                return false;                               \
            }                                              \
            path_index++;                                  \
            slash_pos = find_slash(path, path_index);  \
            if (slash_pos == -1) {                         \
                subname.ptr = path.ptr + path_index;       \
                subname.len = path.len - path_index;       \
                path_index = path.len;                     \
            } else {                                       \
                subname.ptr = path.ptr + path_index;       \
                subname.len = slash_pos - path_index;      \
                path_index = slash_pos;                    \
            }                                              \
        }


    FAT_ID currentDir = rootDir;
    
    while (1) {
        NEXT_SUBNAME

        if (slash_pos == -1) {
            bool yes = delete_entry(context, currentDir, subname);
            return yes;
        } else {
            FAT_ID nextDir = find_directory(context, currentDir, subname);
            if (!nextDir) {
                return false;
            }
            currentDir = nextDir;
        }
    }

    return false;
}


int fat__get_fat(FATContext* context, int cluster) {
    int res;
    
    uint32_t value = -1;

    char tempBuffer[2*512];

    if (context->fat_version == fat__FAT16 || context->fat_version == fat__FAT32) {
        // Clean cluster number divide, FAT entry can't span across sectors.
        int sector_index = context->bpb->reserved_sectors + cluster / context->fat_entries_per_sector;
        res = DISK_read(context->device, (context->start_lba + sector_index) * context->sector_size, context->sector_size, tempBuffer);
        if (!res) return -1;

        void* sector_buffer = tempBuffer;
        int entry_index = cluster % context->fat_entries_per_sector;

        if (context->fat_version == fat__FAT32) {
            value = ((uint32_t*)sector_buffer)[entry_index];
        } else if (context->fat_version == fat__FAT16) {
            value = ((uint16_t*)sector_buffer)[entry_index];
            if (value == 0xFFFF)
                value = fat__END_OF_FILE;
            if (value == 0xFFF8)
                value = fat__RESERVED;
        }
    } else {
        int fat_offset = cluster + cluster / 2;
        
        int sector_index = context->bpb->reserved_sectors + fat_offset / context->sector_size;

        res = DISK_read(context->device, (context->start_lba + sector_index) * context->sector_size, 2 * context->sector_size, tempBuffer);
        if (!res) return -1;

        char* sector_buffer = tempBuffer;

        if (cluster & 1) {
            // check odd or even
            value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) >> 4;
        } else {
            value = *((uint16_t*)&sector_buffer[fat_offset % context->sector_size]) & 0x0FFF;
        }
      
        if (value == 0xFFF)
            value = fat__END_OF_FILE;
        if (value == 0xFF8)
            value = fat__RESERVED;
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




/*
    EXTRA HELPER FUNCTIONS
*/







void init_context(FATContext* context, VFS_Mount* mount) {
    int res;

    res = DISK_read(mount->diskDevice, mount->start_lba * SECTOR_SIZE, SECTOR_SIZE, context->_bootSector);
    if (res == 0) {
        return;
    }

    fat__BPB* bootBlock = (fat__BPB*)(context->_bootSector);
    
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
}


int fat__detect_type(fat__BPB* bpb) {
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


u64 fat__sane_mtime(const fat__DirectoryEntry* entry) {
    return FAT_ToUnixMicroseconds(entry->modified_date, entry->modified_time, 0);
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
    if (currentCluster == FAT_ID_NON_FAT32_ROOT_DIR) {
        printf("make_fat_id: Don't pass in cluster 1!\n");
        kernel_bug();
        return FAT_ID_NULL;
    }
    return currentCluster;
}
void extract_fat_id(FATContext* context, FAT_ID fatID, int* cluster, int* sectorStart, int* sectorEnd) {
    if (fatID == FAT_ID_NON_FAT32_ROOT_DIR) {
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
    return FAT_ID_NON_FAT32_ROOT_DIR;
}


FAT_ID find_directory(FATContext* context, FAT_ID currentDir, const cstring subname) {
    int res;
    int current_cluster;
    int sector_start;
    int sector_end;
    int sector_index = 0;
    extract_fat_id(context, currentDir, &current_cluster, &sector_start, &sector_end);

    char tempSector[512];

    int entry_number = 0;
    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    while (1) {
        
        res = DISK_read(context->device, (context->start_lba + sector_start + sector_index) * SECTOR_SIZE, SECTOR_SIZE, tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return FAT_ID_NULL;
        }

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (int i=0;i<entries_len;i++) {
            fat__DirectoryEntry* entry = &entries[i];
            entry_number++;

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
            if (entry->file_name[0] == 0xE5) {
                // Not present
                continue;
            }
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                sector_index = sector_end - sector_start - 1;
                break;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ')
                continue;
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ')
                continue;
            
            bool same;
            if (longName_startIndex != longName_lastIndex) {
                cstring entryName2 = {
                    .ptr = longName_buffer + longName_startIndex,
                    .len = longName_lastIndex - longName_startIndex,
                };
                longName_startIndex = longName_lastIndex;
                same = fat__string_equal(subname, entryName2);
            } else {
                char entryName[20];
                int  entryName_len = fat__sane_cstring(entry, entryName);
                cstring entryName2 = { entryName, entryName_len };
                same = fat__string_equal(subname, entryName2);
            }

            if (!same) {
                continue;
            }

            if ((entry->attributes & fat__DIRECTORY) == 0) {
                char temp_name[256];
                memcpy(temp_name, subname.ptr, subname.len);
                temp_name[subname.len] = '\0';
                printf("Sub path refers to file not directory, can't go deeper %s\n", temp_name);
                return FAT_ID_NULL;
            }

            int cluster = (int)entry->cluster_low | ((int)entry->cluster_high << 16);

            FAT_ID fatID = make_fat_id(context, cluster);
            return fatID;
        }
        sector_index++;

        if (sector_start + sector_index >= sector_end) {
            if (current_cluster == -1) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                return FAT_ID_NULL;
            }
            // @TODO Search next cluster on FAT32
            return FAT_ID_NULL;
        }
    }

    return FAT_ID_NULL;
}

FAT_ID create_fat_entry(FATContext* context, FAT_ID currentDir, const cstring subname, bool isDirectory) {
    int res;
    int current_cluster;
    int sector_start;
    int sector_end;
    int sector_index = 0;
    extract_fat_id(context, currentDir, &current_cluster, &sector_start, &sector_end);

    char tempSector[512];

    int entry_number = 0;
    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    bool fillMode = false;

    int neededLFNEntries = required_long_name_entries(subname);
    int remainingLFNEntries = neededLFNEntries;

    int char_index = subname.len-1;

    int freeCluster = -1;

    while (1) {
        // printf("READ sector=0x%zx\n", (context->start_lba + sector_start + sector_index) * SECTOR_SIZE);
        res = DISK_read(context->device, (context->start_lba + sector_start + sector_index) * SECTOR_SIZE, SECTOR_SIZE, tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return FAT_ID_NULL;
        }

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)tempSector;
        int entries_len = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

        for (int ei=0;ei<entries_len;ei++) {
            fat__DirectoryEntry* entry = &entries[ei];
            entry_number++;

            if (fillMode) {
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

                    // @TODO When filling entries and hopping to next cluster
                    //   We should write previous cluster.
                } else {
                    // No more entries in directory
                    // sector_index = sector_end - sector_start - 1;
                    // break;

                    if (isDirectory) {
                        entry->attributes = fat__DIRECTORY;
                        entry->file_size = context->bpb->sectors_per_cluster * context->sector_size;
                    } else {
                        entry->attributes = 0;
                        entry->file_size = 0;
                    }
                    // @TODO Set times

                    entry->cluster_low = freeCluster & 0xFFFF;
                    entry->cluster_high = freeCluster >> 16;

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
                    // printf("WRITE sector=0x%zx\n", (context->start_lba + sector_start + sector_index) * SECTOR_SIZE);
                    res = DISK_write(context->device, (context->start_lba + sector_start + sector_index) * SECTOR_SIZE, SECTOR_SIZE, tempSector);
                    if (res == 0) {
                        printf("create_directory: Disk write failed %.*s\n", subname.len, subname.ptr);
                        return FAT_ID_NULL;
                    }

                    if (isDirectory) {
                        // Clear directory entries of newly made directory
                        memset(tempSector, 0, SECTOR_SIZE);
                        int dir_sector_start = fat__cluster_to_sector_offset(context, freeCluster);
                        for (int i = 0; i < context->bpb->sectors_per_cluster; i++) {
                            // printf("CLEARING sector=0x%zx\n", (context->start_lba + dir_sector_start + i) * context->sector_size);
                            res = DISK_write( context->device,
                                (context->start_lba + dir_sector_start + i) * context->sector_size,
                                context->sector_size, tempSector);
                        }
                    }

                    context->sector_index = sector_index;
                    context->sector_start = sector_start;
                    context->entries_index = ei;
                    
                    FAT_ID fatID = make_fat_id(context, freeCluster);
                    return fatID;
                }
            }
            if (entry->file_name[0] == 0xE5) {
                // Not present
                // @TODO Check how many consecutive ones we have. subname may fit.
                continue;
            }
            // We should truncate entry if we look for non directory and entry is non-directory and
            // has same same. We need to look for an entry with that name first.
            // Then look for free entries. Ideally remember the free entries we find when looking for named entry.
            if (entry->file_name[0] != 0) {
                // entry is used
                continue;
            }
            
            int totalEntries = ((sector_end - sector_start) * context->sector_size) / sizeof(fat__DirectoryEntry);
            int remainingEntries = totalEntries - entry_number;

            if (current_cluster != -1) {
                // @TODO Consider next cluster.
                //  Note that root dir doesn't have one.
            }

            if (remainingEntries < neededLFNEntries + 1) { // +1 for short name entry
                return FAT_ID_NULL;
            }

            freeCluster = get_free_cluster(context);
            if (freeCluster == -1) {
                return FAT_ID_NULL;
            }
            res = fat__set_fat(context, freeCluster, fat__END_OF_FILE);
            if (!res) return FAT_ID_NULL;

            // Loop again but this time fill in entries.
            fillMode = true;
            ei--;
            entry_number--;
            continue;
        }
        sector_index++;

        if (sector_start + sector_index >= sector_end) {
            if (current_cluster == -1) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                return FAT_ID_NULL;
            }
            // @TODO Search next cluster on FAT32
            return FAT_ID_NULL;
        }
    }

    return FAT_ID_NULL;
}


bool fat_delete_entry(FATContext* context, FAT_ID currentDir, int entryIndex);

bool delete_entry(FATContext* context, FAT_ID currentDir, const cstring subname) {
    int res;
    // int current_cluster;
    // int sector_start;
    // int sector_end;
    // int sector_index = 0;
    // extract_fat_id(context, currentDir, &current_cluster, &sector_start, &sector_end);

    char tempSector[512];

    u32 entryIndex = 0;
    char longName[256];

    
    while (1) {
        u32 prevIndex = entryIndex;

        fat__DirectoryEntry* entry = fat_next_entry(context, currentDir, &entryIndex, longName);

        cstring entryName = {
            .ptr = longName,
            .len = strlen(longName),
        };

        if (fat__string_equal(subname, entryName)) {
            // DELETE ENTRY, how?
            // We can't just delete entry because it may be preceded by long name.
            fat_delete_entry(context, currentDir, prevIndex);
            return true;
        }
    }

    return false;
}

bool fat_delete_entry(FATContext* context, FAT_ID currentDir, int entryIndex) {
    int res;
    if (currentDir != context->currentDir) {
        extract_fat_id(context, currentDir, &context->current_cluster, &context->sector_start, &context->sector_end);
        context->currentDir = currentDir;
    }

    int entriesPerSector = SECTOR_SIZE / sizeof(fat__DirectoryEntry);
    context->sector_index = (entryIndex) / entriesPerSector;
    int sub_entryIndex = (entryIndex) % entriesPerSector;

    while (1) {
                      
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == -1) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                // return false;
            }
            // @TODO Search next cluster on FAT32
            return false;
        }

        res = DISK_read(context->device, (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return false;
        }

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;

        for (;sub_entryIndex<entriesPerSector;sub_entryIndex++) {
            fat__DirectoryEntry* entry = &entries[sub_entryIndex];
                // entry_number++;

            if (entry->attributes == fat__LFN) {
                fat__LongNameEntry* nameEntry = (fat__LongNameEntry*)entry;
                entry->file_name[0] = 0xE5;
                entry->cluster_low = 0;
                entry->cluster_high = 0;
                continue;
            }
            if (entry->file_name[0] == 0xE5) {
                printf("FAT READ wanted to delete entry but found 0xE5?\n");
                return false;
            }
            if (entry->file_name[0] == 0) {
                printf("FAT READ wanted to delete entry but found 0x0?\n");
                return false;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ') {
                printf("FAT wont't delete '.'\n");
                return false;
            }
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ') {
                printf("FAT wont't delete '..'\n");
                return false;
            }

            int currentCluster = (int)entry->cluster_low | ((int)entry->cluster_high << 16);
            // Check if cluster is valid?

            entry->file_name[0] = 0xE5;
            entry->attributes = 0;
            entry->cluster_low = 0;
            entry->cluster_high = 0;

            // Free clusters
            while (1) {
                int nextCluster = fat__get_fat(context, currentCluster);
                fat__set_fat(context, currentCluster, 0);
                if (nextCluster == 0 || nextCluster == fat__RESERVED || nextCluster == fat__END_OF_FILE) {
                    break;
                }
                currentCluster = nextCluster;
            }

            return true;
        }
        context->sector_index++;

        // if (sector_start + sector_index >= sector_end) {
        //     if (current_cluster == -1) {
        //         // Root directory on FAT12/FAT16 has limited entires in root.
        //         return false;
        //     }
        //     // @TODO Search next cluster on FAT32
        //     return false;
        // }
    }

    return false;
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

    return -1;
}


fat__DirectoryEntry* fat_next_entry(FATContext* context, FAT_ID currentDir, u32* inout_entryIndex, char* longName) {
    int res;
    if (currentDir != context->currentDir) {
        extract_fat_id(context, currentDir, &context->current_cluster, &context->sector_start, &context->sector_end);
        context->currentDir = currentDir;
        // context->entries_index = 0;
    }

    char longName_buffer[256];
    const int longName_lastIndex = sizeof(longName_buffer)-1;
    int longName_startIndex = longName_lastIndex;
    longName_buffer[longName_lastIndex] = '\0';

    int entriesPerSector = SECTOR_SIZE / sizeof(fat__DirectoryEntry);

    context->sector_index = (*inout_entryIndex) / entriesPerSector;
    int sub_entryIndex = (*inout_entryIndex) % entriesPerSector;

    while (1) {
        // Get next root.
                
        if (context->sector_start + context->sector_index >= context->sector_end) {
            if (context->current_cluster == -1) {
                // Root directory on FAT12/FAT16 has limited entires in root.
                // return false;
            }
            // @TODO Search next cluster on FAT32
            // return false;
            return NULL;
        }

        // printf("nextENT READ sector=0x%zx\n", (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE);
        res = DISK_read(context->device, (context->start_lba + context->sector_start + context->sector_index) * SECTOR_SIZE, SECTOR_SIZE, context->tempSector);
        if (res == 0) {
            printf("Could not read\n");
            return false;
        }

        fat__DirectoryEntry* entries = (fat__DirectoryEntry*)context->tempSector;

        for (;sub_entryIndex<entriesPerSector;sub_entryIndex++) {
            fat__DirectoryEntry* entry = &entries[sub_entryIndex];
            // entry_number++;

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
            if (entry->file_name[0] == 0xE5) {
                // Not present
                continue;
            }
            if (entry->file_name[0] == 0) {
                // No more entries in directory
                context->sector_index = context->sector_end - context->sector_start;
                break;
            }

            if (entry->file_name[0] == '.' && entry->file_name[1] == ' ')
                continue;
            if (entry->file_name[0] == '.' && entry->file_name[1] == '.' && entry->file_name[2] == ' ')
                continue;
            
            if (longName_startIndex != longName_lastIndex) {
                cstring entryName2 = {
                    .ptr = longName_buffer + longName_startIndex,
                    .len = longName_lastIndex - longName_startIndex,
                };
                longName_startIndex = longName_lastIndex;
                memcpy(longName, entryName2.ptr, entryName2.len);
            } else {
                int entryName_len = fat__sane_cstring(entry, longName);
            }

            if (sub_entryIndex+1 == entriesPerSector) {
                context->sector_index++;
                sub_entryIndex = 0;
            } else {
                sub_entryIndex++;
            }
            *inout_entryIndex = sub_entryIndex + context->sector_index * entriesPerSector;

            return entry;
        }
    }
    return NULL;
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

u64 FAT_ToUnixMicroseconds(u16 fatDate, u16 fatTime, u8 creationTenths) {
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

