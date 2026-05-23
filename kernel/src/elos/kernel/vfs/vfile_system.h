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
            DiskDevice device;
            VFS_HandleInfo info;
            u32 clusterIndex;
            u64 start_lba;
            // char name[64];
        } fat;
        // @TODO Access/open flags
    };


} VFS_Handle_impl;

