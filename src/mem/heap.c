#include "heap.h"
#include "types.h"
#include "util.h"

#define HEAP_SIZE (1024 * 1024)

typedef struct heap_block {
    uint32_t size;
    uint32_t free;
    struct heap_block* next;
} heap_block_t;

static uint8_t heap_area[HEAP_SIZE];
static heap_block_t* heap_head = 0;
static uint32_t heap_size_bytes = 0;

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

void heap_init(void) {
    uint32_t base = align_up((uint32_t)heap_area, 16);
    heap_head = (heap_block_t*)base;
    heap_size_bytes = HEAP_SIZE - (base - (uint32_t)heap_area);
    heap_head->size = heap_size_bytes - sizeof(heap_block_t);
    heap_head->free = 1;
    heap_head->next = 0;
}

static void split_block(heap_block_t* block, uint32_t size) {
    uint32_t total = size + sizeof(heap_block_t);
    if (block->size <= total + sizeof(heap_block_t)) {
        return;
    }
    uint8_t* base = (uint8_t*)block;
    heap_block_t* next = (heap_block_t*)(base + total);
    next->size = block->size - total;
    next->free = 1;
    next->next = block->next;
    block->size = size;
    block->next = next;
}

void* kmalloc(uint32_t size) {
    if (size == 0 || !heap_head) {
        return 0;
    }
    size = align_up(size, 16);
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free && current->size >= size) {
            split_block(current, size);
            current->free = 0;
            return (uint8_t*)current + sizeof(heap_block_t);
        }
        current = current->next;
    }
    return 0;
}

static void merge_blocks(void) {
    heap_block_t* current = heap_head;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(heap_block_t) + current->next->size;
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->free = 1;
    merge_blocks();
}

uint32_t heap_total_bytes(void) {
    return heap_size_bytes;
}

uint32_t heap_free_bytes(void) {
    uint32_t total = 0;
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free) {
            total += current->size;
        }
        current = current->next;
    }
    return total;
}

void* aligned_alloc(uint32_t size, uint32_t align) {
    if (size == 0 || align == 0 || !heap_head) {
        return 0;
    }
    if (align < 16) {
        align = 16;
    }
    if ((align & (align - 1)) != 0) {
        return 0;
    }

    size = align_up(size, 16);
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free) {
            uint32_t data_ptr = (uint32_t)current + sizeof(heap_block_t);
            uint32_t aligned_ptr = align_up(data_ptr, align);
            uint32_t padding = aligned_ptr - data_ptr;
            
            uint32_t required_size = size + padding;
            
            if (current->size >= required_size) {
                // If there's padding, we MUST split the block so that the 
                // returned pointer has its header immediately before it.
                if (padding > 0) {
                     while (padding < sizeof(heap_block_t)) {
                         // Not enough space for a header in the padding.
                         // We need more padding to fit a header.
                         // Instead, we move to the NEXT aligned address.
                         aligned_ptr += align;
                         padding = aligned_ptr - data_ptr;
                         required_size = size + padding;
                         
                         if (current->size < required_size) {
                             goto next_block;
                         }
                     }
                    
                    // Now padding >= sizeof(heap_block_t).
                    // We can split.
                    uint32_t old_size = current->size;
                    current->size = padding - sizeof(heap_block_t);
                    
                    heap_block_t* next = (heap_block_t*)((uint8_t*)current + padding);
                    next->size = old_size - padding;
                    next->free = 1;
                    next->next = current->next;
                    current->next = next;
                    
                    // The block we want is 'next'
                    current = next;
                }
                
                // Now split at the end if needed
                split_block(current, size);
                current->free = 0;
                
                return (void*)((uint8_t*)current + sizeof(heap_block_t));
            }
        }
next_block:
        current = current->next;
    }
    return 0;
}

void kheap_stats(uint32_t* total_bytes, uint32_t* free_bytes) {
    if (total_bytes) {
        *total_bytes = heap_total_bytes();
    }
    if (free_bytes) {
        *free_bytes = heap_free_bytes();
    }
}
