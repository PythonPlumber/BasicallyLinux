#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void pit_init(uint32_t frequency);
uint64_t timer_get_ticks(void);

#endif

