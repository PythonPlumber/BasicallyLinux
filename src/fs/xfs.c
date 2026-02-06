#include "fs/fs.h"
#include "fs/fs_types.h"
#include "types.h"

static int xfs_mount(block_device_t* dev, fs_node_t* root) {
    if (!dev || !root) {
        return 0;
    }
    return 1;
}

void xfs_init(void) {
    static fs_type_t xfs_type = { "xfs", xfs_mount };
    fs_register(&xfs_type);
}
