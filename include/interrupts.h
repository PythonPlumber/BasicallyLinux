#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

typedef registers_t* (*isr_t)(registers_t* regs);

void register_interrupt_handler(uint8_t n, isr_t handler);
void idt_init(void);

#endif
