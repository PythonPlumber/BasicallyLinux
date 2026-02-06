#include "net/net_ip.h"
#include "types.h"

static net_ip_state_t state;

void net_ip_init(void) {
    state.v4_addr = 0;
    for (uint32_t i = 0; i < 16; ++i) {
        state.v6_addr[i] = 0;
    }
}

net_ip_state_t* net_ip_state(void) {
    return &state;
}
