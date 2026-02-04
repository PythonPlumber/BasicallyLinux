#include "types.h"

typedef struct {
    uint32_t entries;
} proc_view_state_t;

void proc_view_init(void);
proc_view_state_t proc_view_state(void);
