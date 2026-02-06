#include "timer.h"
#include "ports.h"

void pit_init(uint32_t frequency) {
    if (frequency == 0) frequency = 1;
    uint32_t divisor = 1193180 / frequency;
    if (divisor == 0) divisor = 1;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
