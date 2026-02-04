#include "types.h"
#include "timer.h"
#include "serial.h"

// This counter increments every time the PIT timer fires
static uint64_t global_ticks = 0;

uint64_t timer_get_ticks() {
    return global_ticks;
}

// This is called by your interrupt handler in isr.c
void timer_handler(void) {
    global_ticks++;
    if (global_ticks % 100 == 0) {
        serial_write_string("DEBUG: Timer tick 100\n");
    }
}

// Basic PIT initialization (100Hz)
void timer_init(void) {
    pit_init(100);
}
