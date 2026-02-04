#include "fs.h"
#include "types.h"
#include "vfs_mount.h"

#define FS_MAX_TYPES 8

static fs_type_t* fs_types[FS_MAX_TYPES];
static uint32_t fs_type_count = 0;

void fs_init(void) {
    fs_type_count = 0;
}

int fs_register(fs_type_t* type) {
    if (!type || fs_type_count >= FS_MAX_TYPES) {
        return 0;
    }
    fs_types[fs_type_count++] = type;
    return 1;
}

int fs_mount(const char* name, block_device_t* dev, fs_node_t* mountpoint) {
    if (!name || !mountpoint) {
        return 0;
    }
    for (uint32_t i = 0; i < fs_type_count; ++i) {
        if (!fs_types[i] || !fs_types[i]->name) {
            continue;
        }
        const char* type_name = fs_types[i]->name;
        uint32_t j = 0;
        while (type_name[j] && name[j] && type_name[j] == name[j]) {
            j++;
        }
        if (type_name[j] == 0 && name[j] == 0) {
            if (!fs_types[i]->mount) {
                return 0;
            }
            if (!fs_types[i]->mount(dev, mountpoint)) {
                return 0;
            }
            return vfs_mount("/", mountpoint);
        }
    }
    return 0;
}
