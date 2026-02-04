#include "sys_config.h"
#include "types.h"

static sys_config_state_t state;

void sys_config_init(void) {
    state.entries = 0;
}

sys_config_state_t sys_config_state(void) {
    return state;
}
