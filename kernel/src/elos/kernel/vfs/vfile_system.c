
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


#define printf(...) KCON_printf(__VA_ARGS__)

static volatile u32 g_vfs_lock;

static VFS_Mount* g_mounts;
static int g_mounts_cap;
static int g_next_mountIndex;

static VFS_Handle_impl* g_handles;
static int g_handles_cap;
static int g_next_handleIndex;

// static VFS_VirtualNode* g_vfs_root;

VFS_Mount* reserve_mount() {
    VFS_Mount* returnValue = NULL;

    if (!g_mounts) {
        g_mounts_cap = 1000;
        g_mounts = PMEM_alloc(sizeof(VFS_Mount) * g_mounts_cap);
        if (!g_mounts) {
            goto exit;
        }
    }

    if (g_next_mountIndex >= g_mounts_cap) {
        goto exit;
    }

    VFS_Mount* node = &g_mounts[g_next_mountIndex];
    g_next_mountIndex++;
    memset(node, 0, sizeof(*node));

    returnValue = node;

exit:
    return returnValue;
}


void unreserve_mount(VFS_Mount* mount) {
    VFS_Mount* lastMount = &g_mounts[g_next_mountIndex];
    if (lastMount != mount) {
        printf("CAN'T UNRESERVE NODE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_next_mountIndex--;
}


VFS_Handle_impl* reserve_handle() {
    VFS_Handle_impl* returnValue = NULL;

    if (!g_handles) {
        g_handles_cap = 1000;
        g_handles = PMEM_alloc(sizeof(VFS_Handle_impl) * g_handles_cap);
        if (!g_handles) {
            goto exit;
        }
    }

    if (g_next_handleIndex >= g_handles_cap) {
        goto exit;
    }

    VFS_Handle_impl* handle = &g_handles[g_next_handleIndex];
    g_next_handleIndex++;
    memset(handle, 0, sizeof(*handle));

    returnValue = handle;

exit:
    return returnValue;
}


void unreserve_handle(VFS_Handle_impl* handle) {
    VFS_Handle_impl* lastHandle = &g_handles[g_next_handleIndex];
    if (lastHandle != handle) {
        printf("CAN'T UNRESERVE HANDLE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_next_handleIndex--;
}



int find_slash(const cstring path, int offset) {
    int head = 0;
    const char* ptr = path.ptr + offset;
    int len = path.len - offset;
    while (head < len && ptr[head] != '/' && ptr[head] != '\0') {
        head++;
    }
    if (head >= len)
        return -1;
    return head + offset;
}


// bool equal_node_name(VFS_VirtualNode* node, const cstring nodeName) {
//     return node->name_len == nodeName.len && !memcmp(node->name, nodeName.ptr, nodeName.len);
// }

#define get_node_name_length(NODE) ((NODE)->name_len)
#define get_max_node_name_length(NODE) (sizeof((NODE)->name))

// VFS_VirtualNode* find_child_node(VFS_VirtualNode* node, const cstring nodeName) {
//     VFS_VirtualNode* child = node->child;

//     while (child) {
//         if (equal_node_name(child, nodeName)) {
//             return child;
//         }
//         child = child->sibling;
//     }

//     return NULL;
// }

// bool remove_child_node(VFS_VirtualNode* node, const cstring nodeName) {
//     if (!node->child)
//         return false;

//     if (equal_node_name(node->child, nodeName)) {
//         // @TODO Memory leak
//         node->child = node->child->sibling;
//         return true;
//     }

//     VFS_VirtualNode* prev_child = node->child;
//     VFS_VirtualNode* child = node->child->sibling;

//     while (child) {
//         if (equal_node_name(child, nodeName)) {
//             // @TODO Memory leak
//             prev_child->child = child->sibling;
//             return true;
//         }
//         prev_child = child;
//         child = child->sibling;
//     }

//     return false;
// }

bool find_partition(DiskDevice device, int partitionIndex, u64* start_lba, u64* end_lba);
VFS_FileObject* search_fat(DiskDevice device, const cstring path, u64 start_lba, u64 end_lba);


bool VFS_mkdir(const char* cpath) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    
    // Search mounted device.
    // DiskInfo info = {0};
    // DISK_get_info(currentNode->mounted_diskDevice, &info);
    // printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
    
    cstring subpath = PTR_CSTR(cpath + subIndex);

    returnValue = fat_mkdir(mount->diskDevice, mount->start_lba, mount->end_lba, subpath);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool VFS_mount(const char* cpath, DiskDevice device, int partitionIndex) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);
    
    u64 start_lba;
    u64 end_lba;
    bool foundPart = find_partition(device, partitionIndex, &start_lba, &end_lba);
    if (!foundPart) {
        goto exit;
    }

    VFS_Mount* mount = reserve_mount();
    if (!mount) {
        goto exit;
    }
    mount->diskDevice = device;
    mount->partitionIndex = partitionIndex;
    mount->start_lba = start_lba;
    mount->end_lba = end_lba;
    mount->name_len = snprintf(mount->name, sizeof(mount->name), "%s", cpath);
    if (mount->name[mount->name_len] == '/') {
        unreserve_mount(mount);
        goto exit;
    }

    // @TODO When we mount disk we need to lock it so only file system can modify it.
    //   If we run a program that reformats GPT then you must first unmount
    //   which will free internal caching stuff and then you can reformat safely.
    //   Then remount if you wish.
    //   Since file system only accesses disk devices we leave the locking for later.
    //   That reformat use case and how it is done needs to mature first.

    returnValue = true;

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


VFS_Handle VFS_open(const char* cpath, VFS_OpenFlags flags) {
    VFS_Handle returnValue = VFS_NULL_HANDLE;
    UNLOCK_INT(&g_vfs_lock);

    // From a path we get back a directory, a file, a mounting point
    // A virtual node directory.
    VFS_FileObject* obj = resolveFileObject(cpath);
    if (!obj) {
        goto exit;
    }

    VFS_Handle_impl* handle = reserve_handle();
    handle->flags = flags;
    handle->fileObject = obj;
    returnValue = TO_HANDLE(handle);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;

}


void VFS_info(VFS_Handle _handle, VFS_HandleInfo* info) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    
    if (handle->fileObject) {
        int res;
        char stackBuffer[512];
        int buffer_head = 0;
        int sectorSize = 512;
        VFS_FileObject* fileObject = handle->fileObject;

        fat__DirectoryEntry* direntryBlock = (fat__DirectoryEntry*)(stackBuffer + buffer_head);
        buffer_head += sectorSize;

        memset(info, 0, sizeof(*info));
        
        res = DISK_read(fileObject->device, (fileObject->start_lba + fileObject->direntrySector) * sectorSize, sectorSize, direntryBlock);
        if (!res) return;

        fat__DirectoryEntry* entry = &direntryBlock[fileObject->direntryIndex];
        
        info->isDirectory = entry->attributes & fat__DIRECTORY;
        info->readOnly = entry->attributes & fat__READ_ONLY;
        info->fileSize = entry->file_size;
        info->blockSize = sectorSize;
        info->lastWriteTime_us = fat__sane_mtime(entry);
    } else {
        memset(info, 0, sizeof(*info));
    }
}


u64 VFS_read(VFS_Handle _handle, u64 offset, u64 size, void* buffer) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    
    if (handle->fileObject) {
        return read_fat(handle, offset, size, buffer);
    } else {
        return 0;
    }
}

u64 VFS_write(VFS_Handle _handle, u64 offset, u64 size, const void* buffer) {
    return 0;
}



void VFS_close(VFS_Handle _handle) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    // handle->node = NULL;
    // @TODO Free handle
}


VFS_Mount* resolveMount(const char* cpath, int* sub_index) {
    VFS_Mount* largestFit = NULL;
    int        largestFit_length = 0;

    // @TODO Optimize by sorting mounts by name length.
    for (int i=0;i<g_next_mountIndex;i++) {
        VFS_Mount* mount = &g_mounts[i];
        
        bool eq = !strncmp(mount->name, cpath, mount->name_len);
        bool delimiter = (cpath[mount->name_len] == '\0' || cpath[mount->name_len] == '/' || mount->name[mount->name_len-1] == '/');
        if (eq && delimiter) {
            if (mount->name_len > largestFit_length) {
                largestFit = mount;
                largestFit_length = mount->name_len;
            }
        }
    }

    if (largestFit && sub_index) {
        *sub_index = largestFit->name[largestFit->name_len-1] == '/' ? largestFit->name_len-1 : largestFit->name_len;
    }

    return largestFit;
}

VFS_FileObject* resolveFileObject(const char* cpath) {
    VFS_FileObject* returnValue = NULL;
    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }

    // Search mounted device.
    DiskInfo info = {0};
    DISK_get_info(mount->diskDevice, &info);
    printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
    
    cstring subpath = PTR_CSTR(cpath + subIndex);

    VFS_FileObject* obj = search_fat(mount->diskDevice, subpath, mount->start_lba, mount->end_lba);
    if (!obj) {
        goto exit;
    }

    returnValue = obj;
    
exit:
    return returnValue;
}


// bool VFS_remove(const char* cpath) {
//     bool returnValue = false;
//     UNLOCK_INT(&g_vfs_lock);

    
    
//     VFS_VirtualNode* currentNode = g_vfs_root;

//     int path_index = 0;

//     cstring path = PTR_CSTR(cpath);

//     while (currentNode) {

//         // We do not allow multiple slashes to force consistency.
//         // Neither do we allow .. or .
//         if (path.ptr[path_index] != '/') {
//             // Corrupt path
//             goto exit;
//         }
        
//         if (currentNode->mounted_diskDevice != DISK_NULL_DEVICE) {
//             // Search mounted device.
//             DiskInfo info = {0};
//             DISK_get_info(currentNode->mounted_diskDevice, &info);
//             printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
            
//             cstring subpath = PTR_CSTR(path.ptr + path_index);
            
//             u64 start_lba;
//             u64 end_lba;
//             bool foundPart = find_partition(currentNode->mounted_diskDevice, currentNode->mounted_partitionIndex, &start_lba, &end_lba);
//             if (!foundPart) {
//                 goto exit;
//             }
            
//             returnValue = fat_remove(currentNode->mounted_diskDevice, start_lba, end_lba, subpath);
//             goto exit;
//         }

//         path_index++;

//         cstring nodeName = {0};

//         int slash_pos = find_slash(path, path_index);
//         if (slash_pos == -1) {
//             // No slash
//             nodeName.ptr = path.ptr + path_index;
//             nodeName.len = path.len - path_index;
//             path_index = path.len;
//         } else {
//             nodeName.ptr = path.ptr + path_index;
//             nodeName.len = slash_pos - path_index;
//             path_index = slash_pos;
//         }



//         if (slash_pos != -1) {
//             VFS_VirtualNode* child = find_child_node(currentNode, nodeName);
//             currentNode = child;
//         } else {
//             returnValue = remove_child_node(currentNode, nodeName);
//             goto exit;
//         }
//     }

// exit:
//     UNLOCK_INT(&g_vfs_lock);
//     return returnValue;
// }

// bool VFS_rename(const char* old_path, const char* new_path) {
 
//     bool returnValue = false;
//     UNLOCK_INT(&g_vfs_lock);


//     VFS_VirtualNode* old_node = find_node(old_path);
    
//     VFS_VirtualNode* new_node = find_node(new_path);

//     // If nodes on same file system then special thing.

//     // Otherwise copy data from one to the other.

//     if (SameFileSystem(old_node, new_node) && IsFatSystem(old_node)) {

//     } else {
//         VFS_remove(new_path);
//         // Remove new path.
//         char buffer[512];
//         FileSize();
//         ReadData();
//     }



//     VFS_VirtualNode* currentNode = g_vfs_root;

//     int path_index = 0;

//     // @TODO Code copied from remove, need's cleanup.
//     cstring path = PTR_CSTR(cpath);

//     while (currentNode) {

//         // We do not allow multiple slashes to force consistency.
//         // Neither do we allow .. or .
//         if (path.ptr[path_index] != '/') {
//             // Corrupt path
//             goto exit;
//         }
        
//         if (currentNode->mounted_diskDevice != DISK_NULL_DEVICE) {
//             // Search mounted device.
//             DiskInfo info = {0};
//             DISK_get_info(currentNode->mounted_diskDevice, &info);
//             printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
            
//             cstring subpath = PTR_CSTR(path.ptr + path_index);
            
//             u64 start_lba;
//             u64 end_lba;
//             bool foundPart = find_partition(currentNode->mounted_diskDevice, currentNode->mounted_partitionIndex, &start_lba, &end_lba);
//             if (!foundPart) {
//                 goto exit;
//             }
            
//             returnValue = fat_remove(currentNode->mounted_diskDevice, start_lba, end_lba, subpath);
//             goto exit;
//         }

//         path_index++;

//         cstring nodeName = {0};

//         int slash_pos = find_slash(path, path_index);
//         if (slash_pos == -1) {
//             // No slash
//             nodeName.ptr = path.ptr + path_index;
//             nodeName.len = path.len - path_index;
//             path_index = path.len;
//         } else {
//             nodeName.ptr = path.ptr + path_index;
//             nodeName.len = slash_pos - path_index;
//             path_index = slash_pos;
//         }



//         if (slash_pos != -1) {
//             VFS_VirtualNode* child = find_child_node(currentNode, nodeName);
//             currentNode = child;
//         } else {
//             returnValue = remove_child_node(currentNode, nodeName);
//             goto exit;
//         }
//     }

// exit:
//     UNLOCK_INT(&g_vfs_lock);
//     return returnValue;
// }



bool find_partition(DiskDevice device, int partitionIndex, u64* start_lba, u64* end_lba) {
    bool res;

    u64 sectorSize = 512;

    u8  stackBuffer[2 * 512];
    int buffer_head = 0;
    
    gpt__Header* gptHeader = (gpt__Header*)(stackBuffer + buffer_head);
    buffer_head += sectorSize;

    int gpt_header_lba = 1;
    int gpt_partArray_lba = 2;

    res = DISK_read(device, gpt_header_lba * sectorSize, sectorSize, gptHeader);
    if (!res) {
        return false;
    }


    if (memcmp(gptHeader->signature, "EFI PART", 8)) {
        // NOT Guid Partition Table.

        // @TODO MBR CODE HAS NOT BEEN TESTED.

        mbr__Header* mbrHeader = (mbr__Header*)(gptHeader);

        res = DISK_read(device, 0, sectorSize, mbrHeader);
        if (!res) {
            return false;
        }

        if (mbrHeader->bootSignature != MBR_BOOT_SIGNATURE) {
            // No known partition format.
            return false;
        }

        if (partitionIndex < 0 || partitionIndex >= ARRAY_LENGTH(mbrHeader->partitionRecords)) {
            // out of bounds
            return false;
        }

        mbr__PartitionRecord* mbrPartition = &mbrHeader->partitionRecords[partitionIndex];

        *start_lba = mbrPartition->starting_lba;
        *end_lba = mbrPartition->starting_lba + mbrPartition->size_lba;

        printf("Found MBR Partition (#%d) at LBA %d - %d\n", partitionIndex, *start_lba, *end_lba);
        return true;
    } else {
        gpt__Partition* partitionBlock = (gpt__Partition*)(stackBuffer + buffer_head);
        buffer_head += sectorSize;

        if (gptHeader->entry_size > sectorSize) {
            return false;
        }
        if (partitionIndex < 0 || partitionIndex >= gptHeader->num_entries) {
            return false;
        }

        u64 partitionByteOffset = partitionIndex * gptHeader->entry_size;


        res = DISK_read(device, gpt_partArray_lba * sectorSize + (partitionByteOffset / sectorSize) * sectorSize, sectorSize, partitionBlock);
        if (!res) {
            return false;
        }

        gpt__Partition* partition = &partitionBlock[(partitionByteOffset % sectorSize) / gptHeader->entry_size];

        char partitionName[ARRAY_LENGTH(partition->partition_name) + 1];
        for (int i=0;i<ARRAY_LENGTH(partition->partition_name);i++) {
            u16 chr = partition->partition_name[i];
            partitionName[i] = (char)chr;
            if (chr == 0)
                break;
        }
        partitionName[ARRAY_LENGTH(partition->partition_name)] = '\0';

        *start_lba = partition->start_lba;
        *end_lba = partition->end_lba + 1; // end_lba is inclusive, we deal in exlusive ends

        printf("Found GPT Partition '%s' (#%d) at LBA %d - %d\n", partitionName, partitionIndex, *start_lba, *end_lba);
        return true;
    }
}

