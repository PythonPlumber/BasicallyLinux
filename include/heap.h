#ifndef HEAP_H
#define HEAP_H

#include "types.h"

void heap_init(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
uint32_t heap_total_bytes(void);
uint32_t heap_free_bytes(void);
void* aligned_alloc(uint32_t size, uint32_t align);
void kheap_stats(uint32_t* total_bytes, uint32_t* free_bytes);

#endif
