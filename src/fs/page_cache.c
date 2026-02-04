#include "page_cache.h"
#include "heap.h"
#include "kswapd.h"
#include "timer.h"
#include "types.h"
#include "util.h"

#define PAGE_CACHE_MAX 32

static page_cache_entry_t cache[PAGE_CACHE_MAX];
static uint32_t cache_count = 0;

void page_cache_init(void) {
    for (uint32_t i = 0; i < PAGE_CACHE_MAX; ++i) {
        cache[i].inode_id = 0;
        cache[i].page_index = 0;
        cache[i].data = 0;
        cache[i].dirty = 0;
        cache[i].last_used = 0;
        cache[i].hits = 0;
    }
    cache_count = 0;
    kswapd_register_reclaimer(page_cache_reclaim);
}

page_cache_entry_t* page_cache_get(uint32_t inode_id, uint32_t page_index) {
    if (inode_id == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < PAGE_CACHE_MAX; ++i) {
        if (cache[i].inode_id == inode_id && cache[i].page_index == page_index) {
            cache[i].last_used = timer_get_ticks();
            cache[i].hits++;
            return &cache[i];
        }
    }
    uint32_t free_slot = PAGE_CACHE_MAX;
    for (uint32_t i = 0; i < PAGE_CACHE_MAX; ++i) {
        if (cache[i].inode_id == 0) {
            free_slot = i;
            break;
        }
    }
    if (free_slot == PAGE_CACHE_MAX) {
        uint64_t best_age = 0;
        for (uint32_t i = 0; i < PAGE_CACHE_MAX; ++i) {
            if (cache[i].inode_id != 0 && !cache[i].dirty) {
                uint64_t age = cache[i].last_used;
                if (free_slot == PAGE_CACHE_MAX || age < best_age) {
                    free_slot = i;
                    best_age = age;
                }
            }
        }
    }
    if (free_slot < PAGE_CACHE_MAX) {
        if (cache[free_slot].data) {
            kfree(cache[free_slot].data);
        }
        cache[free_slot].inode_id = inode_id;
        cache[free_slot].page_index = page_index;
        cache[free_slot].data = (uint8_t*)kmalloc(4096);
        if (!cache[free_slot].data) {
            cache[free_slot].inode_id = 0;
            return 0;
        }
        memset(cache[free_slot].data, 0, 4096);
        cache[free_slot].dirty = 0;
        cache[free_slot].last_used = timer_get_ticks();
        cache[free_slot].hits = 1;
        if (cache_count < PAGE_CACHE_MAX) {
            cache_count++;
        }
        return &cache[free_slot];
    }
    return 0;
}

void page_cache_mark_dirty(page_cache_entry_t* entry) {
    if (!entry) {
        return;
    }
    entry->dirty = 1;
    entry->last_used = timer_get_ticks();
    entry->hits++;
}

uint32_t page_cache_writeback(uint32_t max_pages, uint32_t (*write_fn)(uint32_t inode_id, uint32_t page_index, const uint8_t* data)) {
    if (!write_fn || max_pages == 0) {
        return 0;
    }
    uint32_t written = 0;
    for (uint32_t i = 0; i < PAGE_CACHE_MAX && written < max_pages; ++i) {
        if (cache[i].inode_id != 0 && cache[i].dirty) {
            if (write_fn(cache[i].inode_id, cache[i].page_index, cache[i].data) != 0) {
                cache[i].dirty = 0;
                cache[i].last_used = timer_get_ticks();
                written++;
            }
        }
    }
    return written;
}

uint32_t page_cache_reclaim(uint32_t target_pages) {
    if (target_pages == 0) {
        return 0;
    }
    uint32_t reclaimed = 0;
    for (uint32_t i = 0; i < PAGE_CACHE_MAX && reclaimed < target_pages; ++i) {
        if (cache[i].inode_id != 0 && !cache[i].dirty) {
            if (cache[i].data) {
                kfree(cache[i].data);
            }
            cache[i].data = 0;
            cache[i].inode_id = 0;
            cache[i].page_index = 0;
            cache[i].last_used = 0;
            cache[i].hits = 0;
            if (cache_count > 0) {
                cache_count--;
            }
            reclaimed++;
        }
    }
    return reclaimed;
}

uint32_t page_cache_cached(void) {
    return cache_count;
}
