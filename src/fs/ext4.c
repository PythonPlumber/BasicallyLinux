#include "fs.h"
#include "fs_types.h"
#include "types.h"

static int ext4_mount(block_device_t* dev, fs_node_t* root) {
    if (!dev || !root) {
        return 0;
    }
    return 1;
}

void ext4_init(void) {
    static fs_type_t ext4_type = { "ext4", ext4_mount };
    fs_register(&ext4_type);
}
