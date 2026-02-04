#include "types.h"
#include "vfs.h"

int vfs_mount(const char* path, fs_node_t* root);
fs_node_t* vfs_mount_root(const char* path);
