#include "types.h"

typedef struct {
    uint32_t tcp_open;
    uint32_t udp_open;
    uint32_t conns;
} net_transport_state_t;

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t state;
} net_conn_t;

void net_transport_init(void);
net_transport_state_t* net_transport_state(void);
uint32_t net_transport_open(uint32_t src, uint32_t dst, uint32_t src_port, uint32_t dst_port);
uint32_t net_transport_close(uint32_t index);
