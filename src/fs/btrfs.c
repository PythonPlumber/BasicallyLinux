#include "fs.h"
#include "fs_types.h"
#include "types.h"

static int btrfs_mount(block_device_t* dev, fs_node_t* root) {
    if (!dev || !root) {
        return 0;
    }
    return 1;
}

void btrfs_init(void) {
    static fs_type_t btrfs_type = { "btrfs", btrfs_mount };
    fs_register(&btrfs_type);
}
