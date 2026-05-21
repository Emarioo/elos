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
} VFS_OpenFlags;

typedef struct {
    u64  fileSize;
    u32  blockSize;
    bool isDirectory;
    bool readOnly;
} VFS_HandleInfo;

typedef void* VFS_Handle;

#define VFS_NULL_HANDLE (NULL)


//###############################
//       FUNCTIONS
//###############################


bool VFS_mount(const char* path, DiskDevice* device);

bool VFS_mkdir(const char* path);

VFS_Handle VFS_open(const char* path, VFS_OpenFlags flags);

void VFS_close(VFS_Handle handle);

void VFS_info(VFS_Handle handle, VFS_HandleInfo* info);

