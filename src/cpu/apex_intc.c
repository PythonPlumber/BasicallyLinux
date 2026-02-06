#include "cpu/apex_intc.h"
#include "types.h"

static uint32_t routes[256];

void apex_intc_init(void) {
    for (uint32_t i = 0; i < 256; ++i) {
        routes[i] = 0;
    }
}

void apex_intc_route(uint32_t irq, uint32_t cpu) {
    if (irq >= 256) {
        return;
    }
    routes[irq] = cpu;
}
