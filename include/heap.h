#ifndef HEAP_H
#define HEAP_H

#include "types.h"

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
size_t heap_total_bytes(void);
size_t heap_free_bytes(void);
void* aligned_alloc(size_t size, size_t align);
void kheap_stats(size_t* total_bytes, size_t* free_bytes);

#endif
