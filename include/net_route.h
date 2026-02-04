#ifndef NET_ROUTE_H
#define NET_ROUTE_H

#include "types.h"

typedef struct {
    uint32_t v4_gateway;
    uint32_t routes;
} net_route_state_t;

typedef struct {
    uint32_t dst;
    uint32_t mask;
    uint32_t gateway;
} net_route_entry_t;

void net_route_init(void);
net_route_state_t* net_route_state(void);
uint32_t net_route_add(uint32_t dst, uint32_t mask, uint32_t gateway);
uint32_t net_route_lookup(uint32_t dst, uint32_t* gateway);

#endif
