#pragma once

#include "elos/vfs.h"
#include "elos/disk.h"

// typedef struct VFS_VirtualNode VFS_VirtualNode;
typedef struct VFS_Mount VFS_Mount;

// struct VFS_VirtualNode {
//     char name[63]; // Allocate in string table?
//     u8 name_len;

//     DiskDevice mounted_diskDevice;
//     int        mounted_partitionIndex;
    
//     VFS_VirtualNode* parent;
//     VFS_VirtualNode* child;
//     VFS_VirtualNode* sibling;
// };

struct VFS_Mount {
    char name[63]; // 62 is maximum which seems reasonable: "/boot/dajioda/jjeajoeo/andaaod/jöamödanld/aenlajepajepajepajeF"
    u8   name_len;
    
    DiskDevice diskDevice;
    int        partitionIndex;
    u64        start_lba;
    u64        end_lba;
};


// typedef struct {
//     DiskDevice diskDevice;
//     int        partitionIndex;
//     u64        start_lba;
//     u64        end_lba;
// } VFS_FileSystem;

typedef struct {
    // disk and partition
    // DiskDevice device;
    // u64 start_lba; // @TODO If we move partitions then this breaks. But if we change partitions we may shrink or ruin FAT anyway so we should probably flush all file objects.

    // Specific to FAT
    u32 clusterIndex;
    // Below must be updated when we rename/move file.
    u32 direntrySector; // Relative to start of partition (start_lba + direntrySector is relative to whole disk).
    u32 direntryIndex;

    // VFS_VirtualNode* node;
    VFS_Mount*  mount;
} VFS_FileObject;


typedef struct {
    VFS_OpenFlags    flags;
    VFS_FileObject*  fileObject;
    // VFS_VirtualNode* node;
} VFS_Handle_impl;



VFS_Mount* resolveMount(const char* cpath, int* subIndex);
VFS_FileObject* resolveFileObject(const char* path);


#define TO_HANDLE(P) ((VFS_Handle)P)
#define FROM_HANDLE(P) ((VFS_Handle_impl*)P)
