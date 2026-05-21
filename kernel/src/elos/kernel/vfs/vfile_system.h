#pragma once

#include "elos/vfs.h"
#include "elos/disk.h"

typedef struct VFS_Node VFS_Node;

struct VFS_Node {
    char name[63]; // Allocate in string table?
    u8 name_len;

    DiskDevice mounted_diskDevice;

    VFS_Node* parent;
    VFS_Node* child;
    VFS_Node* sibling;

};


typedef struct {
    VFS_Node* node;

    // @TODO Access/open flags

} VFS_Handle_impl;

