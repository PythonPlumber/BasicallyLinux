#include "types.h"

typedef struct {
    uint32_t inode_id;
    uint32_t page_index;
    uint8_t* data;
    uint32_t dirty;
    uint64_t last_used;
    uint32_t hits;
} page_cache_entry_t;

void page_cache_init(void);
page_cache_entry_t* page_cache_get(uint32_t inode_id, uint32_t page_index);
void page_cache_mark_dirty(page_cache_entry_t* entry);
uint32_t page_cache_writeback(uint32_t max_pages, uint32_t (*write_fn)(uint32_t inode_id, uint32_t page_index, const uint8_t* data));
uint32_t page_cache_reclaim(uint32_t target_pages);
uint32_t page_cache_cached(void);
