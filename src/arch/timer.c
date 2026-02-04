#include <stdint.h>
#include "timer.h"

// This counter increments every time the PIT timer fires
static uint64_t global_ticks = 0;

uint64_t timer_get_ticks() {
    return global_ticks;
}

// This is called by your interrupt handler in isr.c
void timer_handler(void) {
    global_ticks++;
}

// Basic PIT initialization (100Hz)
void timer_init(void) {
    // You can add outb() commands here later to set the frequency
}
