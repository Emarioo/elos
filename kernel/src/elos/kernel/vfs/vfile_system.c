
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

bool debug_vfs;


#define debug(...) ( !debug_vfs ? 0 : KCON_printf(__VA_ARGS__) )

#define printf(...) KCON_printf(__VA_ARGS__)

static volatile u32 g_vfs_lock;

static VFS_Mount* g_mounts;
static int        g_mounts_cap;
static int        g_mounts_len;

static VFS_Handle_impl* g_handles;
static int              g_handles_cap;
static int              g_handles_len;



#define GET_NORMALIZED_PATH(out_PATH, PATH) \
        char normalized##PATH[256]; \
        int res##PATH = normalizePath(normalized##PATH, sizeof(normalized##PATH), PATH); \
        if (res##PATH == -1) goto exit; \
        char* out_PATH = normalized##PATH;


cstring get_component(const cstring path, int index);
cstring get_basename(const cstring path);
FAT_ID fat_walk(VFS_Mount* mount, const cstring subpath, FAT_ID* parent_dir);

bool VFS_mkdir(const char* _cpath) {
    if (_cpath[0] == '/' && _cpath[1] == '\0') {
        // Cannot create root directory.
        return false;
    }

    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    debug("VFS_mkdir %s\n", cpath);

    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    
    cstring subpath = PTR_CSTR(cpath + subIndex);

    switch (mount->fileSystemKind) {
        case FILESYSTEM_FAT: {
            
            int componentIndex = 0;
            FAT_ID directory = fat_get_root(mount);
            while (directory != FAT_ID_NULL) {
                cstring subname = get_component(subpath, componentIndex);
                if (subname.len == 0) {
                    // no more components, end of path, no more directory to create.
                    break;
                }
                FAT_ID foundEntry = fat_lookup(mount, directory, subname);
                if (foundEntry == FAT_ID_NULL) {
                    // make directory
                    FAT_ID newEntry = fat_make_entry(mount, directory, subname, fat__DIRECTORY);
                    if (newEntry == FAT_ID_NULL) {
                        // Could not make entry
                        goto exit;
                    }
                    directory = newEntry;
                } else {
                    directory = foundEntry;
                }
                componentIndex++;
            }

            returnValue = true;

        } break;
        default: {
            goto exit;
        } break;
    }

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool VFS_mount(const char* _cpath, DiskDevice device, int partitionIndex) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    debug("VFS_mount %s\n", cpath);
    
    u64 start_lba;
    u64 end_lba;
    bool foundPart = gpt_find_partition(device, partitionIndex, &start_lba, &end_lba);
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

    bool yes = fat_is_fat(mount);
    if (yes) {
        mount->fileSystemKind = FILESYSTEM_FAT;
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

volatile void stap_ok() {}

VFS_Handle vfs_open(const char* _cpath, VFS_OpenFlags flags) {
    VFS_Handle returnValue = VFS_NULL_HANDLE;
    // LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    debug("VFS_open %s\n", cpath);

    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    cstring subpath = PTR_CSTR(cpath + subIndex);

    if (flags & VFS_FLAG_CREATE) {
        stap_ok();

        FAT_ID dir;
        FAT_ID file = fat_walk(mount, subpath, &dir);
        if (dir == FAT_ID_NULL) {
            goto exit;
        }
        if (file != FAT_ID_NULL) {
            fat_remove_entry(mount, file);
        }

        cstring basename = get_basename(subpath);
        file = fat_make_entry(mount, dir, basename, 0);
        if (file == FAT_ID_NULL) {
            goto exit;
        }

        VFS_Handle_impl* handle = reserve_handle();
        handle->flags = flags;
        handle->mount = mount;
        handle->fatID = file;
        returnValue = TO_HANDLE(handle);
    } else {
        FAT_ID dir;
        FAT_ID file = fat_walk(mount, subpath, &dir);
        if (file == FAT_ID_NULL) {
            goto exit;
        }

        VFS_Handle_impl* handle = reserve_handle();
        handle->flags = flags;
        handle->mount = mount;
        handle->fatID = file;
        returnValue = TO_HANDLE(handle);
    }

exit:
    // UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

VFS_Handle VFS_open(const char* _cpath, VFS_OpenFlags flags) {
    VFS_Handle returnValue = VFS_NULL_HANDLE;
    LOCK_INT(&g_vfs_lock);

    returnValue = vfs_open(_cpath, flags);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool vfs_info(VFS_Handle _handle, VFS_HandleInfo* info) {
    bool returnValue = false;
    // LOCK_INT(&g_vfs_lock);

    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    memset(info, 0, sizeof(*info));
    returnValue = fat_info(handle->mount, handle->fatID, info);

exit:
    // UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

bool VFS_info(VFS_Handle _handle, VFS_HandleInfo* info) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    returnValue = vfs_info(_handle, info);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

u64 vfs_read(VFS_Handle _handle, u64 offset, u64 size, void* buffer) {
    u64 returnValue = 0;
    // LOCK_INT(&g_vfs_lock);

    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    returnValue = fat_read(handle->mount, handle->fatID, offset, size, buffer);

exit:
    // UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

u64 VFS_read(VFS_Handle _handle, u64 offset, u64 size, void* buffer) {
    u64 returnValue = 0;
    LOCK_INT(&g_vfs_lock);

    returnValue = vfs_read(_handle, offset, size, buffer);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

u64 vfs_write(VFS_Handle _handle, u64 offset, u64 size, const void* buffer) {
    u64 returnValue = 0;
    // LOCK_INT(&g_vfs_lock);

    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    returnValue = fat_write(handle->mount, handle->fatID, offset, size, buffer);

exit:
    // UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

u64 VFS_write(VFS_Handle _handle, u64 offset, u64 size, const void* buffer) {
    u64 returnValue = 0;
    LOCK_INT(&g_vfs_lock);

    returnValue = vfs_write(_handle, offset, size, buffer);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


void vfs_close(VFS_Handle _handle) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;

    // This should update lastModified timestamp.
    // handle->node = NULL;
    // @TODO Free handle
}

void VFS_close(VFS_Handle _handle) {
    LOCK_INT(&g_vfs_lock);

    vfs_close(_handle);

exit:
    UNLOCK_INT(&g_vfs_lock);
}


bool VFS_readdir(const char* _cpath, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* buffer) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    /*
    If directory has mounts then we won't get those since they
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

    if (*entryCount == 0) {
        goto exit;
    }

    debug("VFS_readdir %s 0x%zx %zu 0x%zx\n", cpath, *cookie, *entryCount, buffer);

    u32  consumedEntries = 0;
    u32  maxEntries = *entryCount;
    bool checkMounts = 0 == (*cookie & ((u64)1<<63));

    if (checkMounts) {
        /*  If we have
                /              <- mount
                /pkg/prism
                /boot          <- mount
                /media/usb0    <- mount

            Then we should get
                iterdir("/")
                    pkg boot media
                iterdir("pkg")
                    prism
                iterdir("boot")
                    efi initrd.img
                iterdir("media")
                    usb0

            Note that we can never list the root mount itself.
            We can list things in the root mount of course.
        */

        int mountIndex = *cookie & (u64)0xFFFFFFFF;

        int cpath_len = strlen(cpath);

        for (;mountIndex<g_mounts_len;mountIndex++) {
            VFS_Mount* mount = &g_mounts[mountIndex];

            bool delimiter = (mount->name_len > cpath_len && (mount->name[cpath_len] == '/' || mount->name[cpath_len-1] == '/'));
            if (!delimiter) {
                continue;
            }
            bool eq = !strncmp(mount->name, cpath, cpath_len);
            if (!eq) {
                continue;
            }

            // Cases to handle
            //      /boot
            //      /media/usb
            //      /media/usb/disk
            int mountSubname_index = cpath_len == 1 ? cpath_len : cpath_len + 1;
            int head = mountSubname_index;
            while (head < mount->name_len) {
                char chr = mount->name[head];
                if (chr == '/') {
                    break;
                }
                head++;
            }

            ELOS_DirectoryEntry* entry = &buffer[consumedEntries];
            consumedEntries++;
            entry->isDirectory = true;
            entry->isReadOnly = false;
            entry->fileSize = 0;
            entry->lastWriteTime_us = 0;
            entry->name_len = snprintf(entry->name, sizeof(entry->name), "%.*s", head - mountSubname_index, mount->name + mountSubname_index);

            if (consumedEntries >= maxEntries) {
                break;
            }
        }

        if (consumedEntries == maxEntries) {
            *entryCount = consumedEntries;
            *cookie = mountIndex;
            returnValue = true;
            goto exit;
        }

        *entryCount = consumedEntries;
        *cookie = (u64)1<<63;
    }

    
    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    cstring subpath = PTR_CSTR(cpath + subIndex);

    FAT_ID dir = fat_walk(mount, subpath, NULL);
    if (dir == FAT_ID_NULL) {
        goto exit;
    }

    u64 fat_entryCount = maxEntries - consumedEntries;
    u64 fat_cookie = *cookie & (u64)0xFFFFFFFF;
    returnValue = fat_iterate(mount, dir, &fat_cookie, &fat_entryCount, buffer + consumedEntries);

    *entryCount = consumedEntries + fat_entryCount;
    *cookie = (u64)1<<63 | fat_cookie;

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;

}

bool vfs_remove(const char* _cpath) {
    bool returnValue = false;
    // LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(cpath, _cpath);

    debug("VFS_remove %s\n", cpath);

    int subIndex;
    VFS_Mount* mount = resolveMount(cpath, &subIndex);
    if (!mount) {
        goto exit;
    }
    cstring subpath = PTR_CSTR(cpath + subIndex);

    FAT_ID file = fat_walk(mount, subpath, NULL);
    if (file == FAT_ID_NULL) {
        goto exit;
    }
    
    returnValue = fat_remove_entry(mount, file);

exit:
    // UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

bool VFS_remove(const char* _cpath) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    returnValue = vfs_remove(_cpath);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool vfs_copy(const char* _old_path, const char* _new_path) {
    bool returnValue = false;


    GET_NORMALIZED_PATH(old_path, _old_path);
    GET_NORMALIZED_PATH(new_path, _new_path);

    debug("VFS_copy %s %s\n", old_path, new_path);


    // @TODO For the same mount the specific file system code should
    //    provide a file copy function. It can more efficiently copy
    //    disk sectors. Below will work just fine for now.


    VFS_Handle old_handle = VFS_NULL_HANDLE;
    VFS_Handle new_handle = VFS_NULL_HANDLE;
    size_t bufferSize     = 4 * PAGE_SIZE;
    void* buffer          = NULL;


    old_handle = vfs_open(old_path, VFS_FLAG_READ_ONLY);
    if (old_handle == VFS_NULL_HANDLE) {
        goto exit;
    }
    new_handle = vfs_open(new_path, VFS_FLAG_CREATE);
    if (new_handle == VFS_NULL_HANDLE) {
        goto exit;
    }

    VFS_HandleInfo oldInfo;
    bool yes = vfs_info(old_handle, &oldInfo);
    if (!yes) {
        goto exit;
    }

    buffer = PMEM_alloc_phys(bufferSize, PMEM_FLAG_IDENTITY_MAPPED);
    if (!buffer) {
        goto exit;
    }

    size_t offset = 0;
    while (offset < oldInfo.fileSize) {
        // printf("Write 0x%zx\n", offset);
        u64 bytesToTransfer = oldInfo.fileSize - offset;
        if (bytesToTransfer > bufferSize)
            bytesToTransfer = bufferSize;
        size_t readBytes = vfs_read(old_handle, offset, bytesToTransfer, buffer);
        if (readBytes != bytesToTransfer) {
            goto exit;
        }
        size_t writtenBytes = vfs_write(new_handle, offset, bytesToTransfer, buffer);
        if (writtenBytes != bytesToTransfer) {
            goto exit;
        }
        offset += bytesToTransfer;
    }

    returnValue = true;
    goto exit;

exit:
    if (old_handle) {
        vfs_close(old_handle);
    }
    if (new_handle) {
        vfs_close(new_handle);
    }
    if (buffer) {
        PMEM_free(buffer);
    }
    return returnValue;
}

bool VFS_copy(const char* _old_path, const char* _new_path) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    returnValue = vfs_copy(_old_path, _new_path);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

bool VFS_rename(const char* _old_path, const char* _new_path) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    GET_NORMALIZED_PATH(old_path, _old_path);
    GET_NORMALIZED_PATH(new_path, _new_path);
    
    debug("VFS_rename %s %s\n", old_path, new_path);

    bool yes;
    yes = vfs_copy(_old_path, _new_path);
    if (!yes) {
        goto exit;
    }

    yes = vfs_remove(_old_path);
    if (!yes) {
        goto exit;
    }

    // @TODO Implement file rename in file system.
    //   We don't right now so we can implement file systems without
    //   implementing rename and copy operations since they just use VFS.
    // cstring old_subpath = PTR_CSTR(old_path + old_subIndex);
    // cstring new_subpath = PTR_CSTR(new_path + new_subIndex);
    // returnValue = fat_rename(oldMount, old_subpath, new_subpath);

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
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


//############################
//     UTILITY FUNCTIONS
//############################




cstring get_component(const cstring path, int index) {
    if (path.ptr[0] != '/') {
        return (cstring){0};
    }

    int currentIndex = 0;
    int head = 0;
    while (head < path.len) {
        char chr = path.ptr[head];
        head++;
        if (chr == '/') {
            if (currentIndex == index) {
                break;
            }
            currentIndex++;
        }
    }

    if (currentIndex != index) {
        return (cstring){0};
    }
    int slashHead = head;
    while (slashHead < path.len) {
        char chr = path.ptr[slashHead];
        if (chr == '/') {
            break;
        }
        slashHead++;
    }
    return (cstring){ .ptr = path.ptr + head, .len = slashHead - head };
}

cstring get_basename(const cstring path) {
    int head = path.len - 1;
    while (head >= 0) {
        char chr = path.ptr[head];
        if (chr == '/') {
            break;
        }
        head--;
    }
    head++;
    return (cstring){ .ptr = path.ptr + head, .len = path.len - head };
}

FAT_ID fat_walk(VFS_Mount* mount, const cstring subpath, FAT_ID* parent_dir) {
    int componentIndex = 0;
    FAT_ID parent = FAT_ID_NULL;
    FAT_ID child = fat_get_root(mount);
        
    while (child != FAT_ID_NULL) {
        cstring subname = get_component(subpath, componentIndex);
        if (subname.len == 0) {
            // no more components, end of path, no more directory to create.
            break;
        }
        parent = child;
        child = fat_lookup(mount, parent, subname);
        if (child == FAT_ID_NULL) {
            break;
        }
        componentIndex++;
    }

exit:
    if (parent_dir)
        *parent_dir = parent;
    return child;
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
    VFS_Mount* lastMount = &g_mounts[g_mounts_len-1];
    KERNEL_PANIC(lastMount == mount, "CAN'T UNRESERVE NODE THAT WASNT LAST\n");
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
    KERNEL_PANIC(lastHandle == handle, "CAN'T UNRESERVE HANDLE THAT WASNT LAST\n");
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
