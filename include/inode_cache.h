#ifndef INODE_CACHE_H
#define INODE_CACHE_H

#include "types.h"

typedef struct {
    uint32_t inode_id;
    uint32_t refcount;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
} inode_entry_t;

void inode_cache_init(void);
inode_entry_t* inode_cache_get(uint32_t inode_id);
void inode_cache_put(uint32_t inode_id);

#endif
