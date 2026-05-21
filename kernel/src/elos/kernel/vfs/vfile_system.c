
#include "elos/vfs.h"
#include "elos/kernel/vfs/vfile_system.h"

#include "elos/common/string.h"
#include "elos/common/types.h"
#include "elos/physical_memory.h"
#include "elos/cpu.h"

#include "elos/kernel_console.h"

#define printf(...) KCON_printf(__VA_ARGS__)

static volatile u32 g_vfs_lock;

static VFS_Node* g_nodes;
static int g_nodes_cap;
static int g_next_nodeIndex;

static VFS_Handle_impl* g_handles;
static int g_handles_cap;
static int g_next_handleIndex;

static VFS_Node* g_vfs_root;

VFS_Node* reserve_node() {
    VFS_Node* returnValue = NULL;

    if (!g_nodes) {
        g_nodes_cap = 1000;
        g_nodes = PMEM_alloc(sizeof(VFS_Node) * g_nodes_cap);
        if (!g_nodes) {
            goto exit;
        }
    }

    if (g_next_nodeIndex >= g_nodes_cap) {
        goto exit;
    }

    VFS_Node* node = &g_nodes[g_next_nodeIndex];
    g_next_nodeIndex++;
    memset(node, 0, sizeof(*node));

    returnValue = node;

exit:
    return returnValue;
}


void unreserve_node(VFS_Node* node) {
    VFS_Node* lastNode = &g_nodes[g_next_nodeIndex];
    if (lastNode != node) {
        printf("CAN'T UNRESERVE NODE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_next_nodeIndex--;
}


VFS_Handle_impl* reserve_handle() {
    VFS_Handle_impl* returnValue = NULL;

    if (!g_handles) {
        g_handles = PMEM_alloc(sizeof(VFS_Handle_impl) * g_handles_cap);
        if (!g_handles) {
            goto exit;
        }
    }

    if (g_next_handleIndex >= g_handles_cap) {
        goto exit;
    }

    VFS_Handle_impl* handle = &g_handles[g_next_handleIndex];
    g_next_handleIndex++;
    memset(handle, 0, sizeof(*handle));

    returnValue = handle;

exit:
    return returnValue;
}


void unreserve_handle(VFS_Handle_impl* handle) {
    VFS_Handle_impl* lastHandle = &g_handles[g_next_handleIndex];
    if (lastHandle != handle) {
        printf("CAN'T UNRESERVE HANDLE THAT WASNT LAST\n");
        kernel_bug();
        return;
    }
    g_next_handleIndex--;
}



int find_slash(const cstring path, int offset) {
    int head = 0;
    const char* ptr = path.ptr + offset;
    int len = path.len - offset;
    while (head < len && ptr[head] != '/' && ptr[head] != '\0') {
        head++;
    }
    if (head >= len)
        return -1;
    return head + offset;
}


bool equal_node_name(VFS_Node* node, const cstring nodeName) {
    return node->name_len == nodeName.len && !memcmp(node->name, nodeName.ptr, nodeName.len);
}

#define get_node_name_length(NODE) ((NODE)->name_len)
#define get_max_node_name_length(NODE) (sizeof((NODE)->name))

VFS_Node* find_child_node(VFS_Node* node, const cstring nodeName) {
    VFS_Node* child = node->child;

    while (child) {
        if (equal_node_name(child, nodeName)) {
            return child;
        }
        child = child->sibling;
    }

    return NULL;
}


bool VFS_mkdir_internal(const char* cpath, VFS_Node** lastVisitedNode);

bool VFS_mkdir(const char* cpath) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);
    
    returnValue = VFS_mkdir_internal(cpath, NULL);

    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


bool VFS_mount(const char* cpath, DiskDevice* device) {
    bool returnValue = false;
    LOCK_INT(&g_vfs_lock);

    VFS_Node* mountNode = NULL;
    bool res = VFS_mkdir_internal(cpath, &mountNode);
    if (!res) {
        goto exit;
    }

    mountNode->mounted_diskDevice = device;
    returnValue = true;

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}

bool VFS_mkdir_internal(const char* cpath, VFS_Node** lastVisitedNode) {
    bool returnValue = false;

    if (!g_vfs_root) {
        VFS_Node* node = reserve_node();
        if (!node) {
            goto exit;
        }
        g_vfs_root = node;
    }

    VFS_Node* currentNode = g_vfs_root;

    int path_index = 0;

    cstring path = PTR_CSTR(cpath);

    while (currentNode) {

        if (path.ptr[path_index] != '/') {
            // Corrupt path
            goto exit;
        }
        path_index++;

        cstring nodeName = {0};

        int slash_pos = find_slash(path, path_index);
        if (slash_pos == -1) {
            // No slash
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = path.len - path_index;
        } else {
            nodeName.ptr = path.ptr + slash_pos;
            nodeName.len = slash_pos - path_index;
        }

        VFS_Node* child = find_child_node(currentNode, nodeName);
        if (!child) {
            // CREATE DIRECTORY/NODE
            child = reserve_node();
            
            if (nodeName.len > get_max_node_name_length(child)) {
                unreserve_node(child);
                goto exit;
            }

            memcpy(child->name, nodeName.ptr, nodeName.len);
            child->name_len = nodeName.len;
            child->parent = currentNode;
            
            // Put node first. This new node we just created will
            // probably be used more frequently.
            child->sibling = currentNode->child;
            currentNode->child = child;

        }

        if (lastVisitedNode)
            *lastVisitedNode = child;
        currentNode = child;
        if (slash_pos == -1) {
            break;
        }
    }

    returnValue = true;
    
exit:
    return returnValue;
}

VFS_Handle VFS_open(const char* cpath, VFS_OpenFlags flags) {
    VFS_Handle returnValue = VFS_NULL_HANDLE;
    UNLOCK_INT(&g_vfs_lock);

    
    VFS_Node* currentNode = g_vfs_root;

    int path_index = 0;

    cstring path = PTR_CSTR(cpath);

    while (currentNode) {

        if (path.ptr[path_index] != '/') {
            // Corrupt path
            goto exit;
        }
        path_index++;

        cstring nodeName = {0};

        int slash_pos = find_slash(path, path_index);
        if (slash_pos == -1) {
            // No slash
            nodeName.ptr = path.ptr + path_index;
            nodeName.len = path.len - path_index;
        } else {
            nodeName.ptr = path.ptr + slash_pos;
            nodeName.len = slash_pos - path_index;
        }

        if (currentNode->mounted_diskDevice != DISK_NULL_DEVICE) {
            // Search mounted device.
            DiskInfo info = {0};
            DISK_get_info(currentNode->mounted_diskDevice, &info);
            // @TODO Implement
            printf("Search mounted device %s (%d MB)\n", info.name, info.diskSize/0x100000);
            goto exit;
        }

        VFS_Node* child = find_child_node(currentNode, nodeName);

        currentNode = child;
        if (slash_pos == -1) {
            if (currentNode) {
                VFS_Handle_impl* handle = reserve_handle();
                if (!handle) {
                    goto exit;
                }
                handle->node = currentNode;
                returnValue = (VFS_Handle)handle;
            } else {
                // If not backed by DISK device then create virtual file?
            }
            break;
        }
    }

exit:
    UNLOCK_INT(&g_vfs_lock);
    return returnValue;
}


void VFS_close(VFS_Handle _handle) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    handle->node = NULL;
    // @TODO Free handle
}

void VFS_info(VFS_Handle _handle, VFS_HandleInfo* info) {
    VFS_Handle_impl* handle = (VFS_Handle)_handle;
    VFS_Node* node = handle->node;
    // @TODO Implement
    memset(info, 0, sizeof(*info));
}


