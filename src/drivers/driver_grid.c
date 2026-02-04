#include "driver_grid.h"
#include "types.h"

#define DRIVER_GRID_MAX 32

static driver_node_t* nodes[DRIVER_GRID_MAX];
static uint32_t node_count = 0;

void driver_grid_init(void) {
    node_count = 0;
}

int driver_grid_register(driver_node_t* node) {
    if (!node || node_count >= DRIVER_GRID_MAX) {
        return 0;
    }
    nodes[node_count++] = node;
    return 1;
}

int driver_grid_probe_all(void) {
    int ok = 1;
    for (uint32_t i = 0; i < node_count; ++i) {
        if (nodes[i] && nodes[i]->probe) {
            if (!nodes[i]->probe()) {
                ok = 0;
            }
        }
    }
    return ok;
}
