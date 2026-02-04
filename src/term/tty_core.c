#include "tty_core.h"
#include "types.h"

static tty_state_t state;

void tty_core_init(void) {
    state.columns = 80;
    state.rows = 25;
}

tty_state_t* tty_core_state(void) {
    return &state;
}
