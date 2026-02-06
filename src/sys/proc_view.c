#include "sys/proc_view.h"
#include "types.h"

static proc_view_state_t state;

void proc_view_init(void) {
    state.entries = 0;
}

proc_view_state_t proc_view_state(void) {
    return state;
}
