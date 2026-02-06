#include "mem/heap.h"
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
static spinlock_t heap_lock = 0;

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

static void merge_blocks(void) {
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free && current->next && current->next->free) {
            current->size += sizeof(heap_block_t) + current->next->size;
            current->next = current->next->next;
            // Don't advance 'current' yet, it might merge with the new next
        } else {
            current = current->next;
        }
    }
}

void* kmalloc(uint32_t size) {
    return kmalloc_aligned(size, 16);
}

void* kmalloc_aligned(uint32_t size, uint32_t align) {
    if (size == 0 || align == 0 || !heap_head) {
        return 0;
    }
    uint32_t flags = spin_lock_irqsave(&heap_lock);
    
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free) {
            uintptr_t addr = (uintptr_t)current + sizeof(heap_block_t);
            uintptr_t aligned_addr = (addr + align - 1) & ~(align - 1);
            uint32_t padding = (uint32_t)(aligned_addr - addr);
            
            if (current->size >= size + padding) {
                // If padding is large enough, we can split the front
                if (padding >= sizeof(heap_block_t) + 16) {
                    uint32_t old_size = current->size;
                    current->size = padding - sizeof(heap_block_t);
                    
                    heap_block_t* next = (heap_block_t*)((uint8_t*)current + padding);
                    next->size = old_size - padding;
                    next->free = 1;
                    next->next = current->next;
                    current->next = next;
                    
                    // Now 'next' is our block
                    current = next;
                    padding = 0;
                }
                
                // Now split the back if needed
                split_block(current, size + padding);
                current->free = 0;
                spin_unlock_irqrestore(&heap_lock, flags);
                return (void*)((uintptr_t)current + sizeof(heap_block_t) + padding);
            }
        }
        current = current->next;
    }
    spin_unlock_irqrestore(&heap_lock, flags);
    return 0;
}

void* aligned_alloc(uint32_t size, uint32_t align) {
    return kmalloc_aligned(size, align);
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    
    uint32_t flags = spin_lock_irqsave(&heap_lock);
    
    // Find the block that contains this pointer
    heap_block_t* current = heap_head;
    heap_block_t* block = 0;
    while (current) {
        uintptr_t start = (uintptr_t)current + sizeof(heap_block_t);
        uintptr_t end = start + current->size;
        if ((uintptr_t)ptr >= start && (uintptr_t)ptr < end) {
            block = current;
            break;
        }
        current = current->next;
    }
    
    if (block) {
        block->free = 1;
        // Optimization: only merge if neighbors are free
        merge_blocks();
    }
    
    spin_unlock_irqrestore(&heap_lock, flags);
}

// Custom kfree that handles aligned_alloc if we want, but kmalloc's kfree 
// expects the header. This is tricky. 
// Let's instead improve kmalloc to support alignment if we really need it.
// Or, for now, let's just make mmu.c use a better way or fix kmalloc.

// Actually, the simplest way for a kernel heap is to just search for an aligned block.
// Let's rewrite aligned_alloc properly.

uint32_t heap_total_bytes(void) {
    return heap_size_bytes;
}

uint32_t heap_free_bytes(void) {
    uint32_t total = 0;
    uint32_t flags = spin_lock_irqsave(&heap_lock);
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free) {
            total += current->size;
        }
        current = current->next;
    }
    spin_unlock_irqrestore(&heap_lock, flags);
    return total;
}

int is_heap_ptr(void* ptr) {
    return (uint8_t*)ptr >= heap_area && (uint8_t*)ptr < heap_area + HEAP_SIZE;
}

void kheap_stats(uint32_t* total_bytes, uint32_t* free_bytes) {
    if (total_bytes) {
        *total_bytes = heap_total_bytes();
    }
    if (free_bytes) {
        *free_bytes = heap_free_bytes();
    }
}
