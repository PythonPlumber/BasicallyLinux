#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"
#include "cpu.h"

#if defined(__i386__)
#include "arch/x86/cpu.h"
#endif

typedef registers_t* (*isr_t)(registers_t* regs);

void register_interrupt_handler(uint8_t n, isr_t handler);
void idt_init(void);

#endif
