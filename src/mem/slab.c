#include "mem/slab.h"
#include "mem/heap.h"
#include "mem/kswapd.h"
#include "types.h"
#include "util.h"

typedef struct slab {
    void* memory;
    void* free_list;
    uint32_t free_count;
    struct slab* next;
} slab_t;

struct kmem_cache {
    uint32_t object_size;
    uint32_t align;
    uint32_t objects_per_slab;
    slab_t* slabs;
    struct kmem_cache* next;
};

static kmem_cache_t* cache_list = 0;

static uint32_t slab_object_stride(uint32_t size, uint32_t align) {
    uint32_t value = size;
    if (align && (value & (align - 1)) != 0) {
        value = (value + align - 1) & ~(align - 1);
    }
    if (value < sizeof(void*)) {
        value = sizeof(void*);
    }
    return value;
}

static slab_t* slab_create(kmem_cache_t* cache) {
    slab_t* slab = (slab_t*)kmalloc(sizeof(slab_t));
    if (!slab) {
        return 0;
    }
    slab->memory = aligned_alloc(4096, 4096);
    if (!slab->memory) {
        kfree(slab);
        return 0;
    }
    uint32_t stride = slab_object_stride(cache->object_size, cache->align);
    slab->free_count = cache->objects_per_slab;
    slab->free_list = 0;
    uint8_t* base = (uint8_t*)slab->memory;
    for (uint32_t i = 0; i < cache->objects_per_slab; ++i) {
        void* obj = base + i * stride;
        *(void**)obj = slab->free_list;
        slab->free_list = obj;
    }
    slab->next = cache->slabs;
    cache->slabs = slab;
    return slab;
}

static void* slab_alloc_from(slab_t* slab) {
    if (!slab || !slab->free_list) {
        return 0;
    }
    void* obj = slab->free_list;
    slab->free_list = *(void**)obj;
    slab->free_count--;
    return obj;
}

static uint32_t slab_reclaim(uint32_t target_pages) {
    uint32_t freed = 0;
    kmem_cache_t* cache = cache_list;
    while (cache && freed < target_pages) {
        slab_t* prev = 0;
        slab_t* slab = cache->slabs;
        while (slab && freed < target_pages) {
            if (slab->free_count == cache->objects_per_slab) {
                if (prev) {
                    prev->next = slab->next;
                } else {
                    cache->slabs = slab->next;
                }
                kfree(slab->memory);
                slab_t* to_free = slab;
                slab = slab->next;
                kfree(to_free);
                freed++;
                continue;
            }
            prev = slab;
            slab = slab->next;
        }
        cache = cache->next;
    }
    return freed;
}

void slab_init(void) {
    cache_list = 0;
    kswapd_register_reclaimer(slab_reclaim);
}

kmem_cache_t* kmem_cache_create(uint32_t object_size, uint32_t align) {
    if (object_size == 0) {
        return 0;
    }
    if (align == 0) {
        align = 16;
    }
    kmem_cache_t* cache = (kmem_cache_t*)kmalloc(sizeof(kmem_cache_t));
    if (!cache) {
        return 0;
    }
    cache->object_size = object_size;
    cache->align = align;
    uint32_t stride = slab_object_stride(object_size, align);
    cache->objects_per_slab = 4096 / stride;
    if (cache->objects_per_slab == 0) {
        kfree(cache);
        return 0;
    }
    cache->slabs = 0;
    cache->next = cache_list;
    cache_list = cache;
    return cache;
}

void kmem_cache_destroy(kmem_cache_t* cache) {
    if (!cache) {
        return;
    }
    slab_t* slab = cache->slabs;
    while (slab) {
        slab_t* next = slab->next;
        kfree(slab->memory);
        kfree(slab);
        slab = next;
    }
    if (cache_list == cache) {
        cache_list = cache->next;
    } else {
        kmem_cache_t* it = cache_list;
        while (it && it->next != cache) {
            it = it->next;
        }
        if (it) {
            it->next = cache->next;
        }
    }
    kfree(cache);
}

void* kmem_cache_alloc(kmem_cache_t* cache) {
    if (!cache) {
        return 0;
    }
    slab_t* slab = cache->slabs;
    while (slab) {
        if (slab->free_count > 0) {
            return slab_alloc_from(slab);
        }
        slab = slab->next;
    }
    slab = slab_create(cache);
    return slab_alloc_from(slab);
}

void kmem_cache_free(kmem_cache_t* cache, void* obj) {
    if (!cache || !obj) {
        return;
    }
    slab_t* slab = cache->slabs;
    uint32_t stride = slab_object_stride(cache->object_size, cache->align);
    while (slab) {
        uint8_t* base = (uint8_t*)slab->memory;
        uint8_t* end = base + cache->objects_per_slab * stride;
        if ((uint8_t*)obj >= base && (uint8_t*)obj < end) {
            *(void**)obj = slab->free_list;
            slab->free_list = obj;
            slab->free_count++;
            return;
        }
        slab = slab->next;
    }
}
