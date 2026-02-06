#include "term/pty_mux.h"
#include "types.h"

static pty_mux_state_t state;

void pty_mux_init(void) {
    state.count = 0;
}

pty_mux_state_t pty_mux_state(void) {
    return state;
}
