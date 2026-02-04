#include "types.h"

typedef struct {
    uint8_t addr[6];
    uint32_t up;
} net_link_t;

void net_link_init(void);
net_link_t* net_link_primary(void);
