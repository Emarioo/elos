/*

    Virtual File System

*/

#pragma once

#include "elos/disk.h"
#include "elos/syscalls.h" // to get ELOS_DirectoryEntry


//###############################
//          TYPES
//###############################


typedef enum {
    VFS_FLAG_READ_ONLY = 0x1,
    VFS_FLAG_CREATE = 0x2, // create if missing
} VFS_OpenFlags;

typedef struct {
    u64  fileSize;
    u32  blockSize;
    bool isDirectory;
    bool readOnly;
    u64  lastWriteTime_us;
} VFS_HandleInfo;



typedef void* VFS_Handle;

#define VFS_NULL_HANDLE (NULL)


//###############################
//       FUNCTIONS
//###############################


bool VFS_mount(const char* path, DiskDevice device, int partitionIndex);
bool VFS_mkdir(const char* path);
/*
    If targeted path is a mount then it is unmounted.
    The content of the mount is not deleted.
*/
bool VFS_remove(const char* path);
bool VFS_rename(const char* old_path, const char* new_path);
bool VFS_copy(const char* old_path, const char* new_path);

// @TODO symlinks

VFS_Handle VFS_open(const char* path, VFS_OpenFlags flags);
void VFS_close(VFS_Handle handle);
bool VFS_info(VFS_Handle handle, VFS_HandleInfo* info);

/*
    Only for files
*/
u64 VFS_read(VFS_Handle handle, u64 offset, u64 size, void* buffer);
/*
    Only for files
*/
u64 VFS_write(VFS_Handle handle, u64 offset, u64 size, const void* buffer);

/*
    @param path        Should refer to a directory.
    @param cookie      A pointer to a cookie which holds internal state. Set to 0 first time and do not touch on subsequent calls.
    @param entryCount  The max entries specified by 'buffer' on input. On output how many entries was put into the buffer.
                       If the count is less than the max entries then the function finished. True is still returned to indicate
                       that the function didn't fail.
    @param buffer      A buffer to directory entries to fill with information.

    @return True on success, false on failure. We should provide enum for more detailed errors.
*/
bool VFS_readdir(const char* path, u64* cookie, u64* entryCount, ELOS_DirectoryEntry* buffer);


//##########################
//    Debug functions
//##########################

typedef void(*FN_VFS_print)(const char* buffer, size_t size, void* userData);

void VFS_dump_mounts(FN_VFS_print printCallback, void* userData);
