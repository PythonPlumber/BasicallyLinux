#include "net/net_guard.h"
#include "types.h"

static net_guard_state_t state;
static net_guard_rule_t rules[64];

void net_guard_init(void) {
    state.rules = 0;
    state.dropped = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        rules[i].src = 0;
        rules[i].dst = 0;
        rules[i].port = 0;
        rules[i].allow = 0;
    }
}

net_guard_state_t* net_guard_state(void) {
    return &state;
}

uint32_t net_guard_add_rule(uint32_t src, uint32_t dst, uint32_t port, uint32_t allow) {
    if (state.rules >= 64) {
        return 0;
    }
    rules[state.rules].src = src;
    rules[state.rules].dst = dst;
    rules[state.rules].port = port;
    rules[state.rules].allow = allow ? 1u : 0u;
    state.rules++;
    return 1;
}

uint32_t net_guard_check(uint32_t src, uint32_t dst, uint32_t port) {
    uint32_t decision = 1;
    for (uint32_t i = 0; i < state.rules; ++i) {
        if ((rules[i].src == 0 || rules[i].src == src) &&
            (rules[i].dst == 0 || rules[i].dst == dst) &&
            (rules[i].port == 0 || rules[i].port == port)) {
            decision = rules[i].allow;
        }
    }
    if (!decision) {
        state.dropped++;
    }
    return decision;
}
