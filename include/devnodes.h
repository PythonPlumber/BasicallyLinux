#include "types.h"

typedef struct {
    uint32_t count;
} devnodes_state_t;

void devnodes_init(void);
devnodes_state_t devnodes_state(void);
