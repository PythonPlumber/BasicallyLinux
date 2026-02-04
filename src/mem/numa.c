#include "numa.h"
#include "pmm.h"
#include "types.h"

#define NUMA_MAX_NODES 4

static numa_node_t nodes[NUMA_MAX_NODES];
static uint32_t node_count = 0;
static uint32_t distances[NUMA_MAX_NODES][NUMA_MAX_NODES];

void numa_init(uint32_t total_bytes) {
    node_count = 1;
    nodes[0].id = 0;
    nodes[0].base = 0;
    nodes[0].size = total_bytes;
    nodes[0].free_pages = pmm_total_blocks() - pmm_used_blocks();
    for (uint32_t i = 0; i < NUMA_MAX_NODES; ++i) {
        for (uint32_t j = 0; j < NUMA_MAX_NODES; ++j) {
            distances[i][j] = (i == j) ? 10 : 20;
        }
    }
}

uint32_t numa_node_count(void) {
    return node_count;
}

const numa_node_t* numa_get_node(uint32_t id) {
    if (id >= node_count) {
        return 0;
    }
    return &nodes[id];
}

uint32_t numa_alloc_on_node(uint32_t node_id) {
    if (node_id >= node_count) {
        return 0;
    }
    uint32_t phys = pmm_alloc_block();
    if (phys) {
        if (nodes[node_id].free_pages > 0) {
            nodes[node_id].free_pages--;
        }
    }
    return phys;
}

void numa_free_on_node(uint32_t node_id, void* addr) {
    if (node_id >= node_count) {
        return;
    }
    pmm_free_block(addr);
    nodes[node_id].free_pages++;
}

uint32_t numa_add_node(uint32_t base, uint32_t size) {
    if (node_count >= NUMA_MAX_NODES) {
        return 0;
    }
    uint32_t id = node_count;
    nodes[id].id = id;
    nodes[id].base = base;
    nodes[id].size = size;
    nodes[id].free_pages = size / pmm_block_size();
    for (uint32_t i = 0; i < NUMA_MAX_NODES; ++i) {
        distances[id][i] = (id == i) ? 10 : 20;
        distances[i][id] = (id == i) ? 10 : 20;
    }
    node_count++;
    return id;
}

void numa_set_distance(uint32_t a, uint32_t b, uint32_t distance) {
    if (a >= node_count || b >= node_count) {
        return;
    }
    distances[a][b] = distance;
    distances[b][a] = distance;
}

uint32_t numa_distance(uint32_t a, uint32_t b) {
    if (a >= node_count || b >= node_count) {
        return 0;
    }
    return distances[a][b];
}

uint32_t numa_preferred_node(uint32_t cpu_id) {
    if (node_count == 0) {
        return 0;
    }
    return cpu_id % node_count;
}
