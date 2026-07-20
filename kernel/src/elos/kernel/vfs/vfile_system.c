
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

static VFS_Node* g_nodes;
static int g_nodes_cap;
static int g_next_nodeIndex;

static VFS_Handle_impl* g_handles;
static int g_handles_cap;
static int g_next_handleIndex;

static VFS_Node* g_vfs_root;

VFS_Node* reserve_node() {
    VFS_Node* returnValue = NULL;

    if (!g_nodes) {
        g_nodes_cap = 1000;
        g_nodes = PMEM_alloc(sizeof(VFS_Node) * g_nodes_cap);
        if (!g_nodes) {
            goto exit;
        }
    }

    if (g_next_nodeIndex >= g_nodes_cap) {
        goto exit;
    }

    VFS_Node* node = &g_nodes[g_next_nodeIndex];
    g_next_nodeIndex++;
    memset(node, 0, sizeof(*node));

    returnValue = node;

exit:
    return returnValue;
}


void unreserve_node(VFS_Node* node) {
    VFS_Node* lastNode = &g_nodes[g_next_nodeIndex];
    if (lastNode != node) {
        printf("CAN'T UNRESERVE NODE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_next_nodeIndex--;
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


bool equal_node_name(VFS_Node* node, const cstring nodeName) {
    return node->name_len == nodeName.len && !memcmp(node->name, nodeName.ptr, nodeName.len);
}

#define get_node_name_length(NODE) ((NODE)->name_len)
#define get_max_node_name_length(NODE) (sizeof((NODE)->name))

VFS_Node* find_child_node(VFS_Node* node, const cstring nodeName) {
    VFS_Node* child = node->child;

    while (child) {
        if (equal_node_name(child, nodeName)) {
            return child;
        }
        child = child->sibling;
    }

    return NULL;
}

bool remove_child_node(VFS_Node* node, const cstring nodeName) {
    if (!node->child)
        return false;

    if (equal_node_name(node->child, nodeName)) {
        // @TODO Memory leak
        node->child = node->child->sibling;
        return true;
    }

    VFS_Node* prev_child = node->child;
    VFS_Node* child = node->child->sibling;

    while (child) {
        if (equal_node_name(child, nodeName)) {
            // @TODO Memory leak
            prev_child->child = child->sibling;
            return true;
        }
        prev_child = child;
        child = child->sibling;
    }

    return false;
}

bool find_partition(DiskDevice device, int partitionIndex, u64* start_lba, u64* end_lba);
VFS_Handle_impl* search_fat(DiskDevice device, const cstring path, u64 start_lba, u64 end_lba);


bool VFS_mkdir_internal(const char* cpath, VFS_Node** lastVisitedNode);

bool VFS_mkdir(const char* cpath) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);
    
    returnValue = VFS_mkdir_internal(cpath, NULL);

    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool VFS_mount(const char* cpath, DiskDevice device, int partitionIndex) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    VFS_Node* mountNode = NULL;
    bool res = VFS_mkdir_internal(cpath, &mountNode);
    if (!res) {
        goto exit;
    }

    mountNode->mounted_diskDevice = device;
    returnValue = true;

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

bool VFS_mkdir_internal(const char* cpath, VFS_Node** lastVisitedNode) {
    bool returnValue = false;

    if (!g_vfs_root) {
        VFS_Node* node = reserve_node();
        if (!node) {
            goto exit;
        }
        g_vfs_root = node;
    }

    VFS_Node* currentNode = g_vfs_root;

    int path_index = 0;

    cstring path = PTR_CSTR(cpath);

    while (currentNode) {

        if (path.ptr[path_index] != '/') {
            // Corrupt path
            goto exit;
        }

        if (currentNode->mounted_diskDevice != DISK_NULL_DEVICE) {
            // Search mounted device.
            DiskInfo info = {0};
            DISK_get_info(currentNode->mounted_diskDevice, &info);
            printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
            
            cstring subpath = PTR_CSTR(path.ptr + path_index);
            
            u64 start_lba;
            u64 end_lba;
            bool foundPart = find_partition(currentNode->mounted_diskDevice, currentNode->mounted_partitionIndex, &start_lba, &end_lba);
            if (!foundPart) {
                goto exit;
            }

            bool yes = fat_mkdir(currentNode->mounted_diskDevice, start_lba, end_lba, subpath);
            
            if (lastVisitedNode)
                *lastVisitedNode = currentNode;

            returnValue = yes;
            goto exit;
        }

        path_index++;

        cstring nodeName = {0};

        int slash_pos = find_slash(path, path_index);
        if (slash_pos == -1) {
            // No slash
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = path.len - path_index;
            path_index = path.len;
        } else {
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = slash_pos - path_index;
            path_index = slash_pos;
        }

        VFS_Node* child = find_child_node(currentNode, nodeName);
        if (!child) {
            // CREATE DIRECTORY/NODE
            child = reserve_node();
            
            if (nodeName.len > get_max_node_name_length(child)) {
                unreserve_node(child);
                goto exit;
            }

            memcpy(child->name, nodeName.ptr, nodeName.len);
            child->name_len = nodeName.len;
            child->parent = currentNode;
            
            // Put node first. This new node we just created will
            // probably be used more frequently.
            child->sibling = currentNode->child;
            currentNode->child = child;

        }

        if (lastVisitedNode)
            *lastVisitedNode = child;
        currentNode = child;
        if (slash_pos == -1) {
            break;
        }
    }

    returnValue = true;
    
exit:
    return returnValue;
}

VFS_Handle VFS_open(const char* cpath, VFS_OpenFlags flags) {
    VFS_Handle returnValue = VFS_NULL_HANDLE;
    UNLOCK_INT(&g_vfs_lock);

    
    VFS_Node* currentNode = g_vfs_root;

    int path_index = 0;

    cstring path = PTR_CSTR(cpath);

    while (currentNode) {

        // We do not allow multiple slashes to force consistency.
        // Neither do we allow .. or .
        if (path.ptr[path_index] != '/') {
            // Corrupt path
            goto exit;
        }
        
        if (currentNode->mounted_diskDevice != DISK_NULL_DEVICE) {
            // Search mounted device.
            DiskInfo info = {0};
            DISK_get_info(currentNode->mounted_diskDevice, &info);
            printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
            
            cstring subpath = PTR_CSTR(path.ptr + path_index);
            
            u64 start_lba;
            u64 end_lba;
            bool foundPart = find_partition(currentNode->mounted_diskDevice, currentNode->mounted_partitionIndex, &start_lba, &end_lba);
            if (!foundPart) {
                goto exit;
            }
            
            VFS_Handle_impl* handle = search_fat(currentNode->mounted_diskDevice, subpath, start_lba, end_lba);
            if (!handle) {
                goto exit;
            }

            returnValue = (VFS_Handle)handle;
            goto exit;
        }

        path_index++;

        cstring nodeName = {0};

        int slash_pos = find_slash(path, path_index);
        if (slash_pos == -1) {
            // No slash
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = path.len - path_index;
            path_index = path.len;
        } else {
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = slash_pos - path_index;
            path_index = slash_pos;
        }


        VFS_Node* child = find_child_node(currentNode, nodeName);

        currentNode = child;
        if (slash_pos == -1) {
            if (currentNode) {
                VFS_Handle_impl* handle = reserve_handle();
                if (!handle) {
                    goto exit;
                }
                handle->vfs_node.node = currentNode;
                returnValue = (VFS_Handle)handle;
            } else {
                // If not backed by DISK device then create virtual file?
            }
            break;
        }
    }

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


void VFS_close(VFS_Handle _handle) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    // handle->node = NULL;
    // @TODO Free handle
}
u64 fat__sane_mtime(const fat__DirectoryEntry* entry);


bool VFS_remove(const char* cpath) {
    bool returnValue = false;
    UNLOCK_INT(&g_vfs_lock);

    
    
    VFS_Node* currentNode = g_vfs_root;

    int path_index = 0;

    cstring path = PTR_CSTR(cpath);

    while (currentNode) {

        // We do not allow multiple slashes to force consistency.
        // Neither do we allow .. or .
        if (path.ptr[path_index] != '/') {
            // Corrupt path
            goto exit;
        }
        
        if (currentNode->mounted_diskDevice != DISK_NULL_DEVICE) {
            // Search mounted device.
            DiskInfo info = {0};
            DISK_get_info(currentNode->mounted_diskDevice, &info);
            printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
            
            cstring subpath = PTR_CSTR(path.ptr + path_index);
            
            u64 start_lba;
            u64 end_lba;
            bool foundPart = find_partition(currentNode->mounted_diskDevice, currentNode->mounted_partitionIndex, &start_lba, &end_lba);
            if (!foundPart) {
                goto exit;
            }
            
            returnValue = fat_remove(currentNode->mounted_diskDevice, start_lba, end_lba, subpath);
            goto exit;
        }

        path_index++;

        cstring nodeName = {0};

        int slash_pos = find_slash(path, path_index);
        if (slash_pos == -1) {
            // No slash
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = path.len - path_index;
            path_index = path.len;
        } else {
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = slash_pos - path_index;
            path_index = slash_pos;
        }



        if (slash_pos != -1) {
            VFS_Node* child = find_child_node(currentNode, nodeName);
            currentNode = child;
        } else {
            returnValue = remove_child_node(currentNode, nodeName);
            goto exit;
        }
    }

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

// VFS_Node* resolveNode(const char* path, VFS_FileSystem** fileSystem) {

// }

// bool VFS_rename(const char* old_path, const char* new_path) {
 
//     bool returnValue = false;
//     UNLOCK_INT(&g_vfs_lock);


//     VFS_Node* old_node = find_node(old_path);
    
//     VFS_Node* new_node = find_node(new_path);

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



//     VFS_Node* currentNode = g_vfs_root;

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
//             VFS_Node* child = find_child_node(currentNode, nodeName);
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

void VFS_info(VFS_Handle _handle, VFS_HandleInfo* info) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    
    if (handle->type == VFS_HANDLE_FAT) {
        int res;
        char stackBuffer[512];
        int buffer_head = 0;
        int sectorSize = 512;
        VFS_FileObject* fileObject = handle->fat.fileObject;

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
    
    if (handle->type == VFS_HANDLE_FAT) {
        return read_fat(handle, offset, size, buffer);
    } else {
        return 0;
    }
}

u64 VFS_write(VFS_Handle _handle, u64 offset, u64 size, void* buffer) {
    return 0;
}




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

