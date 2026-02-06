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

void timer_wait_ticks(uint64_t ticks) {
    uint64_t end = global_ticks + ticks;
    while (global_ticks < end) {
        asm volatile("pause");
    }
}

void timer_sleep(uint32_t ms) {
    // 100Hz = 10ms per tick
    uint64_t ticks = ms / 10;
    if (ticks == 0 && ms > 0) ticks = 1;
    timer_wait_ticks(ticks);
}
