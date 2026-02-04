#include "devnodes.h"
#include "types.h"

static devnodes_state_t state;

void devnodes_init(void) {
    state.count = 0;
}

devnodes_state_t devnodes_state(void) {
    return state;
}
