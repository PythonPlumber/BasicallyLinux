#include "types.h"

typedef struct {
    uint32_t nodes;
} sys_view_state_t;

void sys_view_init(void);
sys_view_state_t sys_view_state(void);
