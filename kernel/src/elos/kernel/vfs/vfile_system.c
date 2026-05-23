
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

void VFS_info(VFS_Handle _handle, VFS_HandleInfo* info) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    
    if (handle->type == VFS_HANDLE_FAT) {
        memcpy(info, &handle->fat.info, sizeof(*info));
    } else {
        memset(info, 0, sizeof(*info));
    }
}

u64 read_fat(DiskDevice device, u64 start_lba, int clusterIndex, u64 offset, u64 size, void* buffer);

u64 VFS_read(VFS_Handle _handle, u64 offset, u64 size, void* buffer) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    
    if (handle->type == VFS_HANDLE_FAT) {
        return read_fat(handle->fat.device, handle->fat.start_lba, handle->fat.clusterIndex, offset, size, buffer);
    } else {
        return 0;
    }
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


typedef struct {
    int fat_version;
    int sectors_per_fat;
    int fat_entries_per_sector; // may not align to a whole sector (because of FAT12)

    int sector_size;

    fat__BPB* bpb;
    fat16__EBPB* ebpb16;
    fat32__EBPB* ebpb32;

    u64 start_lba;
    // u64 end_lba;
    DiskDevice device;
} FATContext;




#define fat__FAT12 12
#define fat__FAT16 16
#define fat__FAT32 32
int fat__detect_type(fat__BPB* bpb);



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


/*
    Date and time code is AI generated
*/

static bool IsLeapYear(int year)
{
    return ((year % 4) == 0) &&
           (((year % 100) != 0) || ((year % 400) == 0));
}

static int DaysBeforeMonth(int month, bool leap)
{
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

u64 FAT_ToUnixMicroseconds(u16 fatDate,
                           u16 fatTime,
                           u8 creationTenths)
{
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
    return FAT_ToUnixMicroseconds(entry->modified_date, entry->modified_time, 0);
}




bool fat__string_equal(const fat__DirectoryEntry* entry, const cstring name) {
    char entryName[20];
    int  entryName_len = fat__sane_cstring(entry, entryName);

    if (entryName_len != name.len) {
        return false;
    }

    for (int i=0;i<entryName_len;i++) {
        char chr0 = entryName[i];
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



VFS_Handle_impl* search_fat(DiskDevice device, const cstring path, u64 start_lba, u64 end_lba) {
    int res;

    #define SECTOR_SIZE 512

    u8 stackBuffer[512];
    int buffer_head = 0;

    fat__BPB* bootBlock = (fat__BPB*)(stackBuffer + buffer_head);
    buffer_head += SECTOR_SIZE;

    res = DISK_read(device, start_lba * SECTOR_SIZE, SECTOR_SIZE, stackBuffer);
    if (res == 0) {
        return NULL;
    }

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
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


    int current_cluster;
    int sector_start;
    int sector_end;
    int sector_index;

    if (context->fat_version == fat__FAT32) {
        current_cluster = context->ebpb32->root_cluster;
        sector_start = fat__cluster_to_sector_offset(context, current_cluster);
        sector_end = sector_start + context->bpb->sectors_per_cluster;
        sector_index = 0;
    } else {
        current_cluster = -1;
        sector_index = 0;
        sector_start = context->bpb->reserved_sectors + context->sectors_per_fat * bootBlock->fat_count;
        sector_end = sector_start + context->bpb->root_dir_entries * sizeof(fat__DirectoryEntry) / context->sector_size;
    }
    

    char tempSector[512];
    int entry_number = 0;

    int path_index = 0;
    cstring subname = { 0 };
    int slash_pos = -1;

    while (1) {
        
        if (subname.ptr != path.ptr + path_index) {
            if (path.ptr[path_index] != '/') {
                printf("Expecting slash!\n");
                return NULL;
            }
            path_index++;
            
            int slash_pos = find_slash(path, path_index);
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
                // Can't handle
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
            

            bool same = fat__string_equal(entry, subname);

            if (!same) {
                continue;
            }

            if (slash_pos == -1) {
                printf("Found file/directory %s\n", subname);

                VFS_Handle_impl* handle = reserve_handle();

                handle->type = VFS_HANDLE_FAT;
                
                handle->fat.device = device;
                
                // snprintf(handle->fat.name, sizeof(handle->fat.name), "%s", subname.ptr);
                handle->fat.start_lba = start_lba;

                handle->fat.info.isDirectory = entry->attributes & fat__DIRECTORY;
                handle->fat.info.readOnly = entry->attributes & fat__READ_ONLY;
                handle->fat.info.fileSize = entry->file_size;
                handle->fat.info.blockSize = bootBlock->bytes_per_sector;
                handle->fat.info.lastWriteTime_us = fat__sane_mtime(entry);
                handle->fat.clusterIndex = entry->cluster_low | (entry->cluster_high << 16);
      
                return handle;
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
            sector_end = sector_start + bootBlock->sectors_per_cluster;

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


int fat__read_fat(FATContext* context, int cluster);

u64 read_fat(DiskDevice device, u64 start_lba, int clusterIndex, u64 offset, u64 size, void* buffer) {
    int res;

    #define SECTOR_SIZE 512

    u8 stackBuffer[512];
    int buffer_head = 0;

    fat__BPB* bootBlock = (fat__BPB*)(stackBuffer + buffer_head);
    buffer_head += SECTOR_SIZE;

    res = DISK_read(device, start_lba * SECTOR_SIZE, SECTOR_SIZE, stackBuffer);
    if (res == 0) {
        return 0;
    }

    FATContext _ctx = {0};
    FATContext* context = &_ctx;
    
    context->fat_version = fat__detect_type(bootBlock);
    context->bpb = bootBlock;
    context->ebpb16 = (void*)((char*)bootBlock + 36);
    context->ebpb32 = (void*)((char*)bootBlock + 36);
    context->sector_size = 512;
    context->start_lba = start_lba;
    context->device = device;

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

    // Finally time to write data
    uint32_t cluster = clusterIndex;
    int advance_clusters = offset / (bootBlock->bytes_per_sector * bootBlock->sectors_per_cluster);

    while (advance_clusters) {
        advance_clusters--;
        uint32_t next_cluster = fat__read_fat(context, cluster);
        if (next_cluster == fat__END_OF_FILE) {
            printf("Can't read beyond file\n");
            return 0;
        } else {
            cluster = next_cluster;
        }
    }

    // @TODO Write multiple sectors at once, one whole cluster since it's contiguous. 

    char data_sector[512];

    int sector_index = 0;
    uint64_t buffer_offset = 0;
    while (buffer_offset < size) {

        if (sector_index >= context->bpb->sectors_per_cluster) {
            uint32_t next_cluster = fat__read_fat(context, cluster);
            if (next_cluster == -1)
                return buffer_offset;
            if (next_cluster == fat__END_OF_FILE) {
                // No more to read, return what we did read.
                return buffer_offset;
            } else {
                cluster = next_cluster;
                sector_index = 0;
            }
        }

        int sector_offset = fat__cluster_to_sector_offset(context, cluster) + sector_index;

        int alignment = (context->sector_size + (offset - buffer_offset) % context->sector_size ) % context->sector_size;

        // @TODO Don't read more than file size.

        if ((alignment != 0) || (buffer_offset + context->sector_size > size)) {
            // writing a partial sector.
            // We must read the sector
            // memcpy in our partial data to write then
            // do a full sector write
            res = DISK_read(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, data_sector);
            if (!res) return buffer_offset;

            int part_size = context->sector_size - alignment;
            if (part_size > size - buffer_offset) {
                part_size = size - buffer_offset;
            }
            memcpy((char*)buffer + buffer_offset, data_sector + alignment, part_size);

            // res = context->write_sectors(data_sector, context->lba_start + sector_offset, 1, context->user_data);
            // if (res) return res;

            buffer_offset += part_size;
            sector_index++;
        } else {
            res = DISK_read(device, (context->start_lba + sector_offset) * context->sector_size, context->sector_size, (char*)buffer + buffer_offset);
            if (!res) return buffer_offset;

            buffer_offset += context->sector_size;
            sector_index++;
        }
    }

    // if (offset + size > found_entry->file_size)
    //     found_entry->file_size = offset + size;

    // int sector_offset = fat__cluster_to_sector_offset(internal, dir_cluster) + entries_sector_index;
    // res = context->write_sectors(temp_sector, context->lba_start + sector_offset, 1, context->user_data);
    // if (res) return res;

    // res = fat__copy_fat(internal);
    // if (res) return res;

    return buffer_offset;
}


int fat__read_fat(FATContext* context, int cluster) {
    int res;
    // fat__Context* context = internal->context;
    
    uint32_t value = -1;
    // if (cluster <= 1) {
    //     log("Cluster is invalid/reserved value: %d\n", cluster);
    //     fat__trigger_break();
    // }

    char tempBuffer[2*512];

    if (context->fat_version == fat__FAT16 || context->fat_version == fat__FAT32) {
        // Clean cluster number divide, FAT entry can't span across sectors.
        int sector_index = context->bpb->reserved_sectors + cluster / context->fat_entries_per_sector;
        res = DISK_read(context->device, (context->start_lba + sector_index) * context->sector_size, context->sector_size, tempBuffer);
        if (res) return -1;

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
        if (res) return -1;

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