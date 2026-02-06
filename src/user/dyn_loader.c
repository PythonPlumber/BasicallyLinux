#include "user/dyn_loader.h"
#include "types.h"

static dyn_loader_state_t state;

void dyn_loader_init(void) {
    state.loaded = 0;
}

dyn_loader_state_t dyn_loader_state(void) {
    return state;
}
