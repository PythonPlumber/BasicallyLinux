#include "pmm.h"
#include "types.h"
#include "util.h"

#define PMM_BLOCK_SIZE 4096

static uint32_t* pmm_bitmap = 0;
static uint32_t pmm_max_blocks = 0;
static uint32_t pmm_used_block_count = 0;
static uint32_t pmm_base = 0;
static uint32_t pmm_next_search = 0;
static spinlock_t pmm_lock = 0;

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t align_down(uint32_t value, uint32_t align) {
    return value & ~(align - 1);
}

static void bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1u << (bit % 32));
}

static void bitmap_clear(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1u << (bit % 32));
}

static uint32_t bitmap_test(uint32_t bit) {
    return (pmm_bitmap[bit / 32] >> (bit % 32)) & 1u;
}

void pmm_init(uint32_t start_addr, uint32_t max_size) {
    uint32_t flags = spin_lock_irqsave(&pmm_lock);
    
    // We'll manage memory from 0 up to max_size
    pmm_base = 0;
    pmm_max_blocks = max_size / PMM_BLOCK_SIZE;
    uint32_t bitmap_bytes = ((pmm_max_blocks + 31) / 32) * 4;
    uint32_t bitmap_start = align_up(start_addr, 4);

    pmm_bitmap = (uint32_t*)bitmap_start;
    pmm_used_block_count = pmm_max_blocks; // Initially all used
    pmm_next_search = 0;
    
    // Set all bits to 1 (all used)
    memset(pmm_bitmap, 0xFF, bitmap_bytes);

    // Reserve the bitmap region itself
    uint32_t reserve_start = align_down(bitmap_start, PMM_BLOCK_SIZE);
    uint32_t reserve_end = align_up(bitmap_start + bitmap_bytes, PMM_BLOCK_SIZE);
    
    // Note: We don't free anything yet. kernel_main will call pmm_add_region.
    
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_add_region(uint32_t addr, uint32_t size) {
    uint32_t flags = spin_lock_irqsave(&pmm_lock);
    if (!pmm_bitmap) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }

    uint32_t start = align_up(addr, PMM_BLOCK_SIZE);
    uint32_t end = align_down(addr + size, PMM_BLOCK_SIZE);
    
    if (start >= end) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }

    uint32_t first = start / PMM_BLOCK_SIZE;
    uint32_t last = end / PMM_BLOCK_SIZE;
    
    if (last > pmm_max_blocks) last = pmm_max_blocks;

    for (uint32_t i = first; i < last; ++i) {
        if (bitmap_test(i)) {
            bitmap_clear(i);
            pmm_used_block_count--;
        }
    }
    
    spin_unlock_irqrestore(&pmm_lock, flags);
}

uint32_t pmm_alloc_block(void) {
    uint32_t flags = spin_lock_irqsave(&pmm_lock);
    if (!pmm_bitmap || pmm_used_block_count >= pmm_max_blocks) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return 0;
    }
    
    // Search from next_search hint
    uint32_t start_idx = pmm_next_search;
    uint32_t entries = (pmm_max_blocks + 31) / 32;
    uint32_t total_bits = pmm_max_blocks;
    
    // Optimization: Skip full words
    for (uint32_t i = start_idx / 32; i < entries; ++i) {
        if (pmm_bitmap[i] != 0xFFFFFFFFu) {
            for (uint32_t bit = 0; bit < 32; ++bit) {
                uint32_t index = i * 32 + bit;
                if (index >= total_bits) break;
                
                // Ensure we don't look before our hint if we are in the first word
                if (index < start_idx) continue;

                if (!bitmap_test(index)) {
                    bitmap_set(index);
                    pmm_used_block_count++;
                    pmm_next_search = index + 1;
                    spin_unlock_irqrestore(&pmm_lock, flags);
                    return pmm_base + index * PMM_BLOCK_SIZE;
                }
            }
        }
    }
    
    // If wrapped around or failed, try from beginning if we didn't start at 0
    if (start_idx > 0) {
        for (uint32_t i = 0; i <= start_idx / 32; ++i) {
             if (pmm_bitmap[i] != 0xFFFFFFFFu) {
                for (uint32_t bit = 0; bit < 32; ++bit) {
                    uint32_t index = i * 32 + bit;
                    if (index >= start_idx) break; // Reached where we started
                    
                    if (!bitmap_test(index)) {
                        bitmap_set(index);
                        pmm_used_block_count++;
                        pmm_next_search = index + 1;
                        spin_unlock_irqrestore(&pmm_lock, flags);
                        return pmm_base + index * PMM_BLOCK_SIZE;
                    }
                }
             }
        }
    }
    
    spin_unlock_irqrestore(&pmm_lock, flags);
    return 0;
}

void pmm_free_block(uint32_t address) {
    uint32_t flags = spin_lock_irqsave(&pmm_lock);
    if (!pmm_bitmap) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    
    uint32_t index = address / PMM_BLOCK_SIZE;
    if (index >= pmm_max_blocks) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    if (bitmap_test(index)) {
        bitmap_clear(index);
        if (pmm_used_block_count > 0) {
            pmm_used_block_count--;
        }
        if (index < pmm_next_search) {
            pmm_next_search = index;
        }
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_reserve_region(uint32_t addr, uint32_t size) {
    uint32_t flags = spin_lock_irqsave(&pmm_lock);
    if (!pmm_bitmap) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }

    uint32_t start = align_down(addr, PMM_BLOCK_SIZE);
    uint32_t end = align_up(addr + size, PMM_BLOCK_SIZE);
    
    uint32_t first = start / PMM_BLOCK_SIZE;
    uint32_t last = end / PMM_BLOCK_SIZE;
    
    if (last > pmm_max_blocks) last = pmm_max_blocks;

    for (uint32_t i = first; i < last; ++i) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            pmm_used_block_count++;
        }
    }
    
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_free_region(uint32_t addr, uint32_t size) {
    uint32_t flags = spin_lock_irqsave(&pmm_lock);
    if (!pmm_bitmap || size == 0) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    uint32_t start = align_down(addr, PMM_BLOCK_SIZE);
    uint32_t end = align_up(addr + size, PMM_BLOCK_SIZE);
    if (end <= start) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    if (end <= pmm_base) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }
    if (start < pmm_base) {
        start = pmm_base;
    }
    uint32_t first = (start - pmm_base) / PMM_BLOCK_SIZE;
    uint32_t last = (end - pmm_base) / PMM_BLOCK_SIZE;
    if (last > pmm_max_blocks) {
        last = pmm_max_blocks;
    }
    for (uint32_t i = first; i < last; ++i) {
        if (bitmap_test(i)) {
            bitmap_clear(i);
            if (pmm_used_block_count > 0) {
                pmm_used_block_count--;
            }
            if (i < pmm_next_search) {
                pmm_next_search = i;
            }
        }
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
}

uint32_t pmm_total_blocks(void) {
    return pmm_max_blocks;
}

uint32_t pmm_used_blocks(void) {
    return pmm_used_block_count;
}

uint32_t pmm_block_size(void) {
    return PMM_BLOCK_SIZE;
}

uint32_t get_ram_size(void) {
    return pmm_max_blocks * PMM_BLOCK_SIZE;
}

uint32_t mem_usage_pct(void) {
    if (pmm_max_blocks == 0) {
        return 0;
    }
    uint64_t used = (uint64_t)pmm_used_block_count * 100u;
    return (uint32_t)(used / pmm_max_blocks);
}
