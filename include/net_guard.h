#include "types.h"

typedef struct {
    uint32_t rules;
    uint32_t dropped;
} net_guard_state_t;

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t port;
    uint32_t allow;
} net_guard_rule_t;

void net_guard_init(void);
net_guard_state_t* net_guard_state(void);
uint32_t net_guard_add_rule(uint32_t src, uint32_t dst, uint32_t port, uint32_t allow);
uint32_t net_guard_check(uint32_t src, uint32_t dst, uint32_t port);
