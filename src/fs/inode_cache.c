#include "fs/inode_cache.h"
#include "types.h"

#define INODE_CACHE_MAX 32

static inode_entry_t cache[INODE_CACHE_MAX];

void inode_cache_init(void) {
    for (uint32_t i = 0; i < INODE_CACHE_MAX; ++i) {
        cache[i].inode_id = 0;
        cache[i].refcount = 0;
        cache[i].mode = 0;
        cache[i].uid = 0;
        cache[i].gid = 0;
        cache[i].size = 0;
    }
}

inode_entry_t* inode_cache_get(uint32_t inode_id) {
    if (inode_id == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < INODE_CACHE_MAX; ++i) {
        if (cache[i].inode_id == inode_id) {
            cache[i].refcount++;
            return &cache[i];
        }
    }
    for (uint32_t i = 0; i < INODE_CACHE_MAX; ++i) {
        if (cache[i].inode_id == 0) {
            cache[i].inode_id = inode_id;
            cache[i].refcount = 1;
            return &cache[i];
        }
    }
    return 0;
}

void inode_cache_put(uint32_t inode_id) {
    if (inode_id == 0) {
        return;
    }
    for (uint32_t i = 0; i < INODE_CACHE_MAX; ++i) {
        if (cache[i].inode_id == inode_id) {
            if (cache[i].refcount > 0) {
                cache[i].refcount--;
            }
            if (cache[i].refcount == 0) {
                cache[i].inode_id = 0;
                cache[i].mode = 0;
                cache[i].uid = 0;
                cache[i].gid = 0;
                cache[i].size = 0;
            }
            return;
        }
    }
}
