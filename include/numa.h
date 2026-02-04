#include "types.h"

typedef struct {
    uint32_t id;
    uint32_t base;
    uint32_t size;
    uint32_t free_pages;
} numa_node_t;

void numa_init(uint32_t total_bytes);
uint32_t numa_node_count(void);
const numa_node_t* numa_get_node(uint32_t id);
uint32_t numa_alloc_on_node(uint32_t node_id);
void numa_free_on_node(uint32_t node_id, void* addr);
uint32_t numa_add_node(uint32_t base, uint32_t size);
void numa_set_distance(uint32_t a, uint32_t b, uint32_t distance);
uint32_t numa_distance(uint32_t a, uint32_t b);
uint32_t numa_preferred_node(uint32_t cpu_id);
