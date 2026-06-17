#pragma once

#include "elos/vfs.h"
#include "elos/disk.h"

typedef struct VFS_Node VFS_Node;

struct VFS_Node {
    char name[63]; // Allocate in string table?
    u8 name_len;

    DiskDevice mounted_diskDevice;
    int        mounted_partitionIndex;

    VFS_Node* parent;
    VFS_Node* child;
    VFS_Node* sibling;
};

typedef struct {
    // disk and partition
    DiskDevice device;
    u64 start_lba; // @TODO If we move partitions then this breaks. But if we change partitions we may shrink or ruin FAT anyway so we should probably flush all file objects.

    // Specific to FAT
    u32 clusterIndex;
    // Below must be updated when we rename/move file.
    u32 direntrySector; // Relative to start of partition (start_lba + direntrySector is relative to whole disk).
    u32 direntryIndex;
} VFS_FileObject;

typedef enum {
    VFS_HANDLE_NONE,
    VFS_HANDLE_NODE,
    VFS_HANDLE_FAT,
} VFS_HandleType;

typedef struct {
    VFS_HandleType type;
    union {
        struct {
            VFS_Node* node;
            
        } vfs_node;
        struct {
            VFS_FileObject* fileObject;
        } fat;
    };


} VFS_Handle_impl;


typedef struct {
    DiskDevice diskDevice;
    int        partitionIndex;
    u64        start_lba;
    u64        end_lba;
} VFS_FileSystem;



VFS_Node* resolveNode(const char* path, VFS_FileSystem** fileSystem);

