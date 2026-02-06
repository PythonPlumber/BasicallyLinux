#include "sys/sys_view.h"
#include "types.h"

static sys_view_state_t state;

void sys_view_init(void) {
    state.nodes = 0;
}

sys_view_state_t sys_view_state(void) {
    return state;
}
