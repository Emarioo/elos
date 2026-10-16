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
    u32 permission_map;

    struct {
        u64  allocationLimit; // includes heap memory and shared memory
    } heap;
    
    struct {
        u32  processLimit; // includes heap memory and shared memory
        u32  currentProcessCount;
    } process;

    struct {
        u32  maxThreads;
        u32  defaultStackSize;
    } thread;
    
    struct {
        u64  storageLimit;
        u32  items_len;
        bool canMount;
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

// @TODO We need functionality to clear permissions of programs we don't use anymore.
//   System may issue a notification to user saying permission capacity is running low
//   consider removing some permissions. Or we may say these programs have not been
//   used for 5 months. Do you want to free their permissions.
//   We can allocate more permissions as we need them and resize the hash map implementation
//   but at some point you will get tons of unused permissions.
//   Perhaps user can inspect a system statistics page where permissions is a part of it
//   and it can see than 14 process permissions have not been used the last 5 months.
//   Then they can clear it with one button or inspect which they are and clear individually.

// Permissions* PERM_get_perms(const SHA256* hash);
// Permissions* PERM_create_perms(const SHA256* hash);
Permissions* PERM_get_or_create_perms(const SHA256* hash);

bool PERM_check(Permissions* permissions, ELOS_PermissionType type);

// u64 PERM_check_u64(Permissions* permissions, ELOS_PermissionType type, u64 value);
// u32 PERM_check_u32(Permissions* permissions, ELOS_PermissionType type, u32 value);

