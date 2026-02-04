#include "types.h"

typedef struct {
    uint32_t loaded;
} dyn_loader_state_t;

void dyn_loader_init(void);
dyn_loader_state_t dyn_loader_state(void);
