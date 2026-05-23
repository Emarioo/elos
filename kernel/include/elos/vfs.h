/*

    Virtual File System

*/

#pragma once

#include "elos/disk.h"


//###############################
//          TYPES
//###############################


typedef enum {
    VFS_FLAG_NONE,
    VFS_FLAG_READ,
    VFS_FLAG_WRITE,
    VFS_FLAG_READ_WRITE,
    VFS_FLAG_CREATE, // create if missing
} VFS_OpenFlags;

typedef struct {
    u64  fileSize;
    u32  blockSize;
    bool isDirectory;
    bool readOnly;
    u64  lastWriteTime_us;
} VFS_HandleInfo;


typedef struct {
    char name[64];
    VFS_HandleInfo info;
} VFS_DirectoryEntry;

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


VFS_Handle VFS_open(const char* path, VFS_OpenFlags flags);
void VFS_close(VFS_Handle handle);
void VFS_info(VFS_Handle handle, VFS_HandleInfo* info);


/*
    Only for files
*/
u64 VFS_read(VFS_Handle handle, u64 offset, u64 size, void* buffer);
/*
    Only for files
*/
u64 VFS_write(VFS_Handle handle, u64 offset, u64 size, void* buffer);

/*
    Only for directories
*/
bool VFS_next(VFS_Handle handle, VFS_DirectoryEntry* out_entry);

