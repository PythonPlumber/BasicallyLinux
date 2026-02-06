#include "net/net_guard.h"
#include "net/net_ip.h"
#include "net/net_link.h"
#include "net/net_route.h"
#include "net/net_stack.h"
#include "net/net_transport.h"
#include "types.h"

static uint32_t net_ready = 0;
static uint8_t rx_buffer[65536];
static uint32_t rx_size = 0;
static spinlock_t net_lock = 0;

void net_stack_init(void) {
    net_link_init();
    net_ip_init();
    net_transport_init();
    net_route_init();
    net_guard_init();
    net_lock = 0;
    net_ready = 1;
}

uint32_t net_stack_ready(void) {
    return net_ready;
}

uint32_t net_stack_send(uint32_t src, uint32_t dst, uint32_t port, const uint8_t* data, uint32_t size) {
    if (!net_ready || !data || size == 0) {
        return 0;
    }
    if (!net_guard_check(src, dst, port)) {
        return 0;
    }
    if (size > sizeof(rx_buffer)) {
        return 0; // Packet too large
    }
    
    uint32_t flags = spin_lock_irqsave(&net_lock);
    
    uint32_t gateway = 0;
    net_route_lookup(dst, &gateway);
    
    for (uint32_t i = 0; i < size; ++i) {
        rx_buffer[i] = data[i];
    }
    rx_size = size;
    
    spin_unlock_irqrestore(&net_lock, flags);
    return size;
}

uint32_t net_stack_recv(uint8_t* out, uint32_t size) {
    if (!net_ready || !out || size == 0) {
        return 0;
    }
    
    uint32_t flags = spin_lock_irqsave(&net_lock);
    
    if (rx_size == 0) {
        spin_unlock_irqrestore(&net_lock, flags);
        return 0;
    }
    
    if (size > rx_size) {
        size = rx_size;
    }
    for (uint32_t i = 0; i < size; ++i) {
        out[i] = rx_buffer[i];
    }
    rx_size = 0;
    
    spin_unlock_irqrestore(&net_lock, flags);
    return size;
}
