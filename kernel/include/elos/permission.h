#pragma once

#include "elos/common/types.h"
#include "elos/elos.h"
#include "elos/common/sha256.h"

typedef enum {
    PERM_FILESYSTEM_READ  = 0x1,
    PERM_FILESYSTEM_WRITE = 0x2,
    PERM_FILESYSTEM_EXEC  = 0x4,
} PermissionFilesystemItem_Flags;

typedef struct {
    uint16_t flags;
    uint16_t path_len;
    char     path[];
} PermissionFilesystemItem;

typedef struct {
    u64 permission_map;

    struct {
        u64  allocationLimit; // includes heap memory and shared memory
    } heap;
    
    struct {
        u32  processLimit; // includes heap memory and shared memory
    } process;

    struct {
        u32  maxThreads;
    } thread;
    
    struct {
        u64  storageLimit;
        u32  items_len;
        PermissionFilesystemItem* items;
    } filesystem;
    
    struct {
        // byte rate limit
        // connection limit
        // endpoint restrictions
    } network;

} Permissions;

typedef struct {
    u64 current_allocationSize;
    u32 current_processCount;
    u32 current_threadCount;
    u64 current_storageSize;
} Permissions_Instance;

Permissions* PERM_get_perms(SHA256 hash);

bool PERM_check(Permissions* permissions, ELOS_PermissionType type);

u64 PERM_check_u64(Permissions* permissions, ELOS_PermissionType type, u64 value);
u32 PERM_check_u32(Permissions* permissions, ELOS_PermissionType type, u32 value);

