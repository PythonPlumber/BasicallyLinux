#include "types.h"

typedef struct kmem_cache kmem_cache_t;

void slab_init(void);
kmem_cache_t* kmem_cache_create(uint32_t object_size, uint32_t align);
void kmem_cache_destroy(kmem_cache_t* cache);
void* kmem_cache_alloc(kmem_cache_t* cache);
void kmem_cache_free(kmem_cache_t* cache, void* obj);
