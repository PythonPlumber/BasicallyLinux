#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"
#include "cpu.h"

// Generic interrupt handler type
typedef registers_t* (*interrupt_handler_t)(registers_t* regs);

// Initialize the interrupt controller for the current CPU
void intc_init(void);

// Register a handler for a specific interrupt vector
void interrupts_register_handler(uint32_t vector, interrupt_handler_t handler);

// Send End of Interrupt (EOI) to the controller
void interrupts_send_eoi(uint32_t vector);

// Enable/Disable a specific IRQ line
void interrupts_enable_irq(uint32_t irq);
void interrupts_disable_irq(uint32_t irq);

// Trigger a software interrupt/IPI
void interrupts_send_ipi(uint32_t target_cpu, uint32_t vector);

// Standard interrupt vectors (architecture-dependent mapping)
#define INT_TIMER    32
#define INT_KEYBOARD 33
#define INT_SYSCALL  128

#endif
