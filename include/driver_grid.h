#include "types.h"

typedef struct {
    const char* name;
    int (*probe)(void);
} driver_node_t;

void driver_grid_init(void);
int driver_grid_register(driver_node_t* node);
int driver_grid_probe_all(void);
