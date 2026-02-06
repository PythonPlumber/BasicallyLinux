#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void pit_init(uint32_t frequency);
uint64_t timer_get_ticks(void);
void timer_handler(void);
void timer_init(void);
void timer_sleep(uint32_t ms);
void timer_wait_ticks(uint64_t ticks);

#endif

