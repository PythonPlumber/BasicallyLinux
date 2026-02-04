#include "types.h"

typedef struct {
    uint32_t v4_addr;
    uint8_t v6_addr[16];
} net_ip_state_t;

void net_ip_init(void);
net_ip_state_t* net_ip_state(void);
