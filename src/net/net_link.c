#include "net_link.h"
#include "types.h"

static net_link_t primary_link;

void net_link_init(void) {
    for (uint32_t i = 0; i < 6; ++i) {
        primary_link.addr[i] = 0;
    }
    primary_link.up = 0;
}

net_link_t* net_link_primary(void) {
    return &primary_link;
}
