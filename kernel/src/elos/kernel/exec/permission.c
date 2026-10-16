
#include "elos/permission.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/cpu.h"
#include "elos/physical_memory.h"
#include "elos/kernel_console.h"

#define printf(...) KCON_printf(__VA_ARGS__)

#define MiB 0x10000
#define KiB 0x400

Permissions* g_permissions;
int          g_permissions_len;
int          g_permissions_max;

static void init_permissions() {
    g_permissions_max = 1000;
    g_permissions_len = 0;
    g_permissions = PMEM_alloc(sizeof(Permissions) * g_permissions_max);
    if (!g_permissions) {
        printf("perm: Kernel could not allocate buffer for permissions\n");
    }
}

static void PERM_init_default_permissions(Permissions* permissions) {
    memset(permissions, 0, sizeof(*permissions));
    permissions->permission_map = 0;
    permissions->heap.allocationLimit  = 4 * MiB;
    permissions->process.processLimit     = 16;
    permissions->thread.maxThreads       = 4;
    permissions->thread.defaultStackSize = 256 * KiB;
}

volatile u32 g_permission_lock;

#define HASH_SLOT_UNUSED   0xFFFFFFFF
#define HASH_SLOT_FREE     0xFFFFFFFE
#define HASH_SLOT_RESERVED 0xFFFFFFFD

u32     g_hashes_cap;
SHA256* g_hashes;
u32*    g_indexes; // index to permissions array

void init_hash_table() {
    g_hashes_cap = 8192;
    g_hashes  = PMEM_alloc(sizeof(SHA256) * g_hashes_cap);
    g_indexes = PMEM_alloc(sizeof(u32) * g_hashes_cap);
    KERNEL_PANIC(g_hashes, "could not allocate perm hash table");
    KERNEL_PANIC(g_indexes, "could not allocate perm hash table");

    // Set all slots as unused
    memset(g_indexes, 0xFF, sizeof(u32) * g_hashes_cap);

    // @TODO Save permissions and hashes to disk.
    //   The user's permissions should be saved between runs.
}

#define INDEX_FROM_PERM(perm) (((size_t)perm - (size_t)g_permissions)/sizeof(Permissions))

// returns -1 if could not find or not insert.
// permissions is non-null to insert.
u32 insert_or_get_hash(const SHA256* hash, Permissions* permissions, bool* foundExisting) {
    if (!g_hashes) {
        init_hash_table();
    }

    if (foundExisting) {
        *foundExisting = false;
    }

    // @TODO Distribution on this might not be great.
    u32 mini_hash = *(u32*)&hash->bytes[0];
    u32 slot_index = mini_hash % g_hashes_cap; // @TODO Bitwise and instead of modulo when cap is exponent of 2.
    u32 start_slot_index = slot_index;
    u32 first_free_slot_index = -1;
    
    while (1) {
        SHA256* slot  = &g_hashes[slot_index];
        u32     index = g_indexes[slot_index];

        if (index == HASH_SLOT_UNUSED) {
            if (permissions) {
                if (first_free_slot_index != -1) {
                    g_hashes[first_free_slot_index] = *hash;
                    g_indexes[first_free_slot_index] = INDEX_FROM_PERM(permissions);
                    return first_free_slot_index;
                } else {
                    *slot = *hash;
                    g_indexes[slot_index] = INDEX_FROM_PERM(permissions);
                    return slot_index;
                }
            }
            return -1;
        }

        if (index != HASH_SLOT_FREE) {
            int cmp = sha256_compare(&hash->bytes, &slot->bytes);
            if (cmp == 0) {
                if (foundExisting) {
                    *foundExisting = true;
                }
                return slot_index;
            }
        } else {
            if (first_free_slot_index == -1) {
                first_free_slot_index = slot_index;
            }
        }
        
        slot_index = (slot_index + 1) % g_hashes_cap;

        if (slot_index == start_slot_index) {
            break;
        }
    }

    return -1;
}

Permissions* PERM_get_or_create_perms(const SHA256* hash) {
    Permissions* returnValue = NULL;
    LOCK_INT(&g_permission_lock);

    if (!g_permissions) {
        init_permissions();
    }

    if (g_permissions_len >= g_permissions_max) {
        printf("perm: Out of permission structs (%d), kernel needs to implement resize.\n", g_permissions_len);
        goto exit;
    }

    Permissions* permissions = &g_permissions[g_permissions_len];

    bool foundExisting = false;
    u32 inserted_slot = insert_or_get_hash(hash, permissions, &foundExisting);
    if (inserted_slot == -1) {
        // Happens if not enough space
        goto exit;
    }

    if (!foundExisting) {
        g_permissions_len++;
        PERM_init_default_permissions(permissions);
    } else {
        u32 perm_index = g_indexes[inserted_slot];
        permissions = &g_permissions[perm_index];
    }

    returnValue = permissions;

exit:
    UNLOCK_INT(&g_permission_lock);
    return returnValue;
}

bool PERM_check(Permissions* permissions, ELOS_PermissionType type) {
    switch (type) {
        case ELOS_PERM_PROCESS_SPAWN: {
            return (permissions->permission_map & (1 << ELOS_PERM_PROCESS_SPAWN))
                && permissions->process.currentProcessCount + 1 >= permissions->process.processLimit;
        } break;
        case ELOS_PERM_DISK:
        case ELOS_PERM_USER_EVENT:
        case ELOS_PERM_AUDIO:
        case ELOS_PERM_MONITOR:
        {
            return (permissions->permission_map & (1 << ELOS_PERM_PROCESS_SPAWN));
        } break;
        default:
    }
    return false;
}
