#pragma once

#include "elos/vfs.h"
#include "elos/disk.h"

#include "elos/kernel/vfs/fat.h" // we need FAT_ID


#define TO_HANDLE(P) ((VFS_Handle)P)
#define FROM_HANDLE(P) ((VFS_Handle_impl*)P)

typedef enum {
    FILESYSTEM_NONE,
    FILESYSTEM_FAT,
    FILESYSTEM_EXT2,
} FileSystemKind;

typedef struct VFS_Mount VFS_Mount;

struct VFS_Mount {
    char name[63]; // 62 is maximum which seems reasonable: "/boot/dajioda/jjeajoeo/andaaod/jöamödanld/aenlajepajepajepajeF"
    u8   name_len;
    
    DiskDevice diskDevice;
    int        partitionIndex;
    u64        start_lba;
    u64        end_lba;

    FileSystemKind fileSystemKind;
};

typedef struct VFS_FileObject VFS_FileObject;
struct VFS_FileObject {
    VFS_Mount*  mount;

    // Specific to FAT
    u32 clusterIndex;
    // Below must be updated when we rename/move file.
    u32 direntrySector; // Relative to start of partition (start_lba + direntrySector is relative to whole disk).
    u32 direntryIndex;
    bool isRootDirectory;
};


typedef struct {
    VFS_OpenFlags    flags;
    VFS_Mount*       mount;
    FAT_ID           fatID;

    VFS_FileObject*  fileObject;
} VFS_Handle_impl;



VFS_Mount* resolveMount(const char* cpath, int* subIndex);
VFS_FileObject* resolveFileObject(const char* path);

VFS_Mount* reserve_mount();
void unreserve_mount(VFS_Mount* mount);

VFS_Handle_impl* reserve_handle();
void unreserve_handle(VFS_Handle_impl* handle);

// Returns -1 if buffer is too small or has invalid characters
int normalizePath(char* buffer, int bufferSize, const char* path);
