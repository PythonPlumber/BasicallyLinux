#ifndef APEX_INTC_H
#define APEX_INTC_H

#include "types.h"

void apex_intc_init(void);
void apex_intc_route(uint32_t irq, uint32_t cpu);

#endif
