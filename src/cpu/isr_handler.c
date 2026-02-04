#include "idt.h"

// Redirect to the implementation in isr.c
void register_interrupt_handler(uint8_t n, isr_t handler);

// This file exists to satisfy dependencies on isr_handler.c
// and can be expanded for specific handler logic if needed.
