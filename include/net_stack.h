#include "types.h"

void net_stack_init(void);
uint32_t net_stack_ready(void);
uint32_t net_stack_send(uint32_t src, uint32_t dst, uint32_t port, const uint8_t* data, uint32_t size);
uint32_t net_stack_recv(uint8_t* out, uint32_t size);
