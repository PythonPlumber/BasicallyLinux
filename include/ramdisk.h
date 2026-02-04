#ifndef RAMDISK_H
#define RAMDISK_H

#include "types.h"
#include "vfs.h"

fs_node_t* ramdisk_init(uint32_t module_start, uint32_t module_size);

#endif
