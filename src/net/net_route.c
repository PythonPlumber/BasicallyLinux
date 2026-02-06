#include "net/net_route.h"
#include "types.h"

static net_route_state_t state;
static net_route_entry_t routes[64];

void net_route_init(void) {
    state.v4_gateway = 0;
    state.routes = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        routes[i].dst = 0;
        routes[i].mask = 0;
        routes[i].gateway = 0;
    }
}

net_route_state_t* net_route_state(void) {
    return &state;
}

uint32_t net_route_add(uint32_t dst, uint32_t mask, uint32_t gateway) {
    if (state.routes >= 64) {
        return 0;
    }
    routes[state.routes].dst = dst;
    routes[state.routes].mask = mask;
    routes[state.routes].gateway = gateway;
    state.routes++;
    return 1;
}

uint32_t net_route_lookup(uint32_t dst, uint32_t* gateway) {
    uint32_t best_mask = 0;
    uint32_t best_gateway = state.v4_gateway;
    for (uint32_t i = 0; i < state.routes; ++i) {
        if ((dst & routes[i].mask) == (routes[i].dst & routes[i].mask)) {
            if (routes[i].mask >= best_mask) {
                best_mask = routes[i].mask;
                best_gateway = routes[i].gateway;
            }
        }
    }
    if (gateway) {
        *gateway = best_gateway;
    }
    return best_gateway != 0;
}
