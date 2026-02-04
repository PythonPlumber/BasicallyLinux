#include "net_transport.h"
#include "types.h"

static net_transport_state_t state;
static net_conn_t conns[64];

void net_transport_init(void) {
    state.tcp_open = 0;
    state.udp_open = 0;
    state.conns = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        conns[i].src = 0;
        conns[i].dst = 0;
        conns[i].src_port = 0;
        conns[i].dst_port = 0;
        conns[i].state = 0;
    }
}

net_transport_state_t* net_transport_state(void) {
    return &state;
}

uint32_t net_transport_open(uint32_t src, uint32_t dst, uint32_t src_port, uint32_t dst_port) {
    if (state.conns >= 64) {
        return 0;
    }
    uint32_t idx = state.conns;
    conns[idx].src = src;
    conns[idx].dst = dst;
    conns[idx].src_port = src_port;
    conns[idx].dst_port = dst_port;
    conns[idx].state = 1;
    state.conns++;
    state.tcp_open++;
    return idx + 1;
}

uint32_t net_transport_close(uint32_t index) {
    if (index == 0 || index > state.conns) {
        return 0;
    }
    uint32_t idx = index - 1;
    if (conns[idx].state == 0) {
        return 0;
    }
    conns[idx].state = 0;
    if (state.tcp_open > 0) {
        state.tcp_open--;
    }
    return 1;
}
