
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
static int        g_mounts_cap;
static int        g_mounts_len;

static VFS_Handle_impl* g_handles;
static int              g_handles_cap;
static int              g_handles_len;

bool find_partition(DiskDevice device, int partitionIndex, u64* start_lba, u64* end_lba);



VFS_Mount* reserve_mount() {
    VFS_Mount* returnValue = NULL;

    if (!g_mounts) {
        g_mounts_cap = 1000;
        g_mounts = PMEM_alloc(sizeof(VFS_Mount) * g_mounts_cap);
        if (!g_mounts) {
            goto exit;
        }
    }

    if (g_mounts_len >= g_mounts_cap) {
        goto exit;
    }

    VFS_Mount* node = &g_mounts[g_mounts_len];
    g_mounts_len++;
    memset(node, 0, sizeof(*node));

    returnValue = node;

exit:
    return returnValue;
}


void unreserve_mount(VFS_Mount* mount) {
    VFS_Mount* lastMount = &g_mounts[g_mounts_len];
    if (lastMount != mount) {
        printf("CAN'T UNRESERVE NODE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_mounts_len--;
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

    if (g_handles_len >= g_handles_cap) {
        goto exit;
    }

    VFS_Handle_impl* handle = &g_handles[g_handles_len];
    g_handles_len++;
    memset(handle, 0, sizeof(*handle));

    returnValue = handle;

exit:
    return returnValue;
}


void unreserve_handle(VFS_Handle_impl* handle) {
    VFS_Handle_impl* lastHandle = &g_handles[g_handles_len];
    if (lastHandle != handle) {
        printf("CAN'T UNRESERVE HANDLE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_handles_len--;
}

// Returns -1 if buffer is too small or has invalid characters
int normalizePath(char* buffer, int bufferSize, const char* path) {

    /*  Rules
        /media/./usb0/  -> /media/usb0/
        /media/usb0/../ -> /media/
        /media//usb0/   -> media/usb0/
    */

    #define CHECK(N) if (output_len + (N) > bufferSize) return -1;

    if (path[0] != '/') {
        return -1;
    }

    int output_len = 0;
    int head = 0;
    int prevOutput_len = 0;
    int startHead = 0;
    while (1) {
        char chr = path[head];
        if (chr != '/' && chr != '\0') {
            head++;
            continue;
        }
        
        // Parse component

        int tmpOutputLength = output_len;
        int componentLength = head - startHead;
        if (componentLength == 0) {
            if (chr == '\0') {
                break;
            }
            if (output_len > 0 && buffer[output_len-1] == '/') {
                // skip consecutive /
                // remember previous component and don't treat // as the
                // previous component
                tmpOutputLength = prevOutput_len;
            } else {
                CHECK(1)
                buffer[output_len] = chr;
                output_len++;
            }
        } else if (componentLength == 1 && path[startHead] == '.') {
            // emit nothing
        } else if (componentLength == 2 && path[startHead] == '.' && path[startHead+1] == '.') {
            output_len = prevOutput_len;
        } else {
            CHECK(componentLength+1)
            memcpy(buffer + output_len, path + startHead, componentLength+1);
            output_len += componentLength+1;
        }

        if (chr == '\0') {
            break;
        }

        head++;
        prevOutput_len = tmpOutputLength;
        startHead = head;
    }

    if (output_len == 0) {
        return -1;
    } else if (output_len > 1 && buffer[output_len-1] == '/') {
        // remove trailing slash
        output_len--;
    }

    buffer[output_len] = '\0';
    return output_len;
    #undef CHECK
}

void test_normalizePath() {

    char buffer[256];
    int res = 0;
    #define CHECK(PATH) \
        res = normalizePath(buffer, sizeof(buffer), PATH); \
        if (res == -1) { \
            buffer[0] = 0; \
        } \
        printf("%2d %14s <- %s\n", res,  buffer, PATH); 

    CHECK(".")
    CHECK("./")
    CHECK("/.")
    CHECK("/..")
    CHECK("//")
    CHECK("//a//b//c//")
    CHECK("/.media")
    CHECK("/media.")
    CHECK("/media./usb")
    CHECK("/media./usb/.")
    CHECK("/media./usb/..")
    CHECK("/media/./usb")
    CHECK("/media/../usb/")
    CHECK("/media//../usb/")
    /*
-1                 <- .
-1                 <- ./
 1               / <- /.
-1                 <- /..
 1               / <- //
 6          /a/b/c <- //a//b//c//
 8         /.media <- /.media
 8         /media. <- /media.
12     /media./usb <- /media./usb
11     /media./usb <- /media./usb/.
 7         /media. <- /media./usb/..
11      /media/usb <- /media/./usb
 4            /usb <- /media/../usb/
 4            /usb <- /media//../usb/
    */
    #undef CHECK

}


#define GET_NORMALIZED_PATH(out_PATH, PATH) \
        char normalized##PATH[256]; \
        int res##PATH = normalizePath(normalized##PATH, sizeof(normalized##PATH), PATH); \
        if (res##PATH == -1) goto exit; \
        char* out_PATH = normalized##PATH;

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





bool VFS_mkdir(const char* _cpath) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    
    cstring subpath = PTR_CSTR(cpath + subIndex);

    returnValue = fat_mkdir(mount, subpath);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool VFS_mount(const char* _cpath, DiskDevice device, int partitionIndex) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);
    
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


VFS_Handle VFS_open(const char* _cpath, VFS_OpenFlags flags) {
    VFS_Handle returnValue = VFS_NULL_HANDLE;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

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
        
        res = DISK_read(fileObject->mount->diskDevice, (fileObject->mount->start_lba + fileObject->direntrySector) * sectorSize, sectorSize, direntryBlock);
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
    u64 returnValue = 0;
    LOCK_INT(&g_vfs_lock);

    VFS_Handle_impl* handle = (VFS_Handle)_handle;

    if (handle->fileObject) {
        returnValue = read_fat(handle, offset, size, buffer);
    }

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

u64 VFS_write(VFS_Handle _handle, u64 offset, u64 size, const void* buffer) {
    u64 returnValue = 0;
    LOCK_INT(&g_vfs_lock);

    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    
    if (handle->fileObject) {
        returnValue = write_fat(handle, offset, size, buffer);
    }

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}



void VFS_close(VFS_Handle _handle) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;

    // This should update lastModified timestamp.
    // handle->node = NULL;
    // @TODO Free handle
}


VFS_Mount* resolveMount(const char* cpath, int* sub_index) {
    VFS_Mount* largestFit = NULL;
    int        largestFit_length = 0;

    // @TODO Optimize by sorting mounts by name length.
    for (int i=0;i<g_mounts_len;i++) {
        VFS_Mount* mount = &g_mounts[i];
        
        bool eq = !strncmp(mount->name, cpath, mount->name_len);
        if (!eq)  continue;
        bool delimiter = (cpath[mount->name_len] == '\0' || cpath[mount->name_len] == '/' || mount->name[mount->name_len-1] == '/');
        if (!delimiter)  continue;
        
        if (mount->name_len > largestFit_length) {
            largestFit = mount;
            largestFit_length = mount->name_len;
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
    // printf("Searching mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
    
    cstring subpath = PTR_CSTR(cpath + subIndex);

    VFS_FileObject* obj = search_fat(mount, subpath);
    if (!obj) {
        goto exit;
    }

    returnValue = obj;
    
exit:
    return returnValue;
}


bool VFS_readdir(const char* _cpath, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* buffer) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    /*
    @TODO If directory has mounts then we won't get those since they
       exist above file system on disk. We need to inject mounted directories.
       Cookie also has to represent this now. Maybe we have a layered cookie.
       Do we iterate mounts first or last?

     We need to match look for mounts. If we have /media/usb0 and /media/usb1 then
      we need special handling for /media which is neither a file system directory nor mount.
      we need to prefix match other mounts. /media/extra/usb8 should also match giving us extra from /media.

    We use layered cookies where iterator has it's own cookie numbering and here we add another layer.
    For example if 63 bit is cleared we iterate mounts. If it is set we iterate entries in the directory.

    We do this instead of adding directory entries for each mount. This keeps mount system separate from file system,
    a path prefix resolution system.
    */

    VFS_FileObject* obj = resolveFileObject(cpath);
    if (!obj) {
        goto exit;
    }

    returnValue = iter_fat(obj->mount, obj, cookie, entryCount, buffer);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;

}

bool VFS_remove(const char* _cpath) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    
    cstring subpath = PTR_CSTR(cpath + subIndex);
            
    returnValue = fat_remove(mount, subpath);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

bool VFS_rename(const char* _old_path, const char* _new_path) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(old_path, _old_path);
    GET_NORMALIZED_PATH(new_path, _new_path);

    int subIndex;
    VFS_Mount* oldMount = resolveMount(old_path, &subIndex);
    if (!oldMount) {
        goto exit;
    }

    int subIndex2;
    VFS_Mount* newMount = resolveMount(new_path, &subIndex);
    if (!newMount) {
        goto exit;
    }

    if (oldMount != newMount) {
        // @TODO Handle different mounts?
        // Copy operation first?
        // Then delete operation?
        goto exit;
    }

    // @TODO Move file?

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool VFS_copy(const char* _old_path, const char* _new_path) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);


    GET_NORMALIZED_PATH(old_path, _old_path);
    GET_NORMALIZED_PATH(new_path, _new_path);

    int subIndex;
    VFS_Mount* oldMount = resolveMount(old_path, &subIndex);
    if (!oldMount) {
        goto exit;
    }

    int subIndex2;
    VFS_Mount* newMount = resolveMount(new_path, &subIndex);
    if (!newMount) {
        goto exit;
    }

    if (oldMount != newMount) {
        // @TODO Handle different mounts.
        goto exit;
    }

    // @TODO Copy data?

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
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


//######################
//    Debug functions
//######################


void VFS_dump_mounts(FN_VFS_print printCallback, void* userData) {
    LOCK_INT(&g_vfs_lock);

    char tempBuffer[256];
    for (int i=0;i<g_mounts_len;i++) {
        VFS_Mount* mount = &g_mounts[i];

        DiskInfo info;
        DISK_get_info(mount->diskDevice, &info);
        
        int length = snprintf(tempBuffer, sizeof(tempBuffer),
            "%d: '%s' disk='%s' partIdx=%d [%zx : %zx]\n", i, mount->name,
            info.name, mount->partitionIndex, mount->start_lba, mount->end_lba);

        // A little dangerous to have a callback inside lock interrupt section.
        // Callback may itself lock VFS if it writes to a log file.
        printCallback(tempBuffer, length, userData);
    }

    UNLOCK_INT(&g_vfs_lock);
}

