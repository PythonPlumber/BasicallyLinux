#include "types.h"
#include "usb_nexus.h"

static uint32_t port_count = 0;

void usb_nexus_init(void) {
    port_count = 0;
}

uint32_t usb_nexus_ports(void) {
    return port_count;
}
