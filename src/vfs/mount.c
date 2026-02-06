#include "vfs/vfs_mount.h"
#include "types.h"

#define VFS_MAX_MOUNTS 8

typedef struct {
    const char* path;
    fs_node_t* root;
} vfs_mount_t;

static vfs_mount_t mounts[VFS_MAX_MOUNTS];
static uint32_t mount_count = 0;

int vfs_mount(const char* path, fs_node_t* root) {
    if (!path || !root || mount_count >= VFS_MAX_MOUNTS) {
        return 0;
    }
    mounts[mount_count].path = path;
    mounts[mount_count].root = root;
    mount_count++;
    return 1;
}

fs_node_t* vfs_mount_root(const char* path) {
    if (!path) {
        return 0;
    }
    for (uint32_t i = 0; i < mount_count; ++i) {
        const char* mount_path = mounts[i].path;
        uint32_t j = 0;
        while (mount_path[j] && path[j] && mount_path[j] == path[j]) {
            j++;
        }
        if (mount_path[j] == 0) {
            return mounts[i].root;
        }
    }
    return 0;
}
