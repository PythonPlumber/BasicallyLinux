#include "block.h"
#include "types.h"
#include "vfs.h"

typedef struct {
    const char* name;
    int (*mount)(block_device_t* dev, fs_node_t* root);
} fs_type_t;

void fs_init(void);
int fs_register(fs_type_t* type);
int fs_mount(const char* name, block_device_t* dev, fs_node_t* mountpoint);
