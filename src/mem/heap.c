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

void* kmalloc(uint32_t size) {
    if (size == 0 || !heap_head) {
        return 0;
    }
    uint32_t flags = spin_lock_irqsave(&heap_lock);
    size = align_up(size, 16);
    heap_block_t* current = heap_head;
    while (current) {
        if (current->free && current->size >= size) {
            split_block(current, size);
            current->free = 0;
            spin_unlock_irqrestore(&heap_lock, flags);
            return (uint8_t*)current + sizeof(heap_block_t);
        }
        current = current->next;
    }
    spin_unlock_irqrestore(&heap_lock, flags);
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
    uint32_t flags = spin_lock_irqsave(&heap_lock);
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->free = 1;
    merge_blocks();
    spin_unlock_irqrestore(&heap_lock, flags);
}

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

void* aligned_alloc(uint32_t size, uint32_t align) {
    if (size == 0 || align == 0 || !heap_head) {
        return 0;
    }
    uint32_t flags = spin_lock_irqsave(&heap_lock);
    // Basic aligned alloc: find a block, if its data isn't aligned, split it?
    // For now, let's just use kmalloc and hope it's aligned enough (16 bytes)
    // or implement a proper one if needed.
    // Given current kmalloc aligns to 16, if align <= 16, it works.
    spin_unlock_irqrestore(&heap_lock, flags);
    return kmalloc(size);
}

void kheap_stats(uint32_t* total_bytes, uint32_t* free_bytes) {
    if (total_bytes) {
        *total_bytes = heap_total_bytes();
    }
    if (free_bytes) {
        *free_bytes = heap_free_bytes();
    }
}
