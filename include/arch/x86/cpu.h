#ifndef ARCH_X86_CPU_H
#define ARCH_X86_CPU_H

#include "types.h"

struct registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

#define LAPIC_BASE 0xFEE00000
#define LAPIC_ID   0x0020
#define LAPIC_EOI  0x00B0
#define LAPIC_SVR  0x00F0
#define LAPIC_ICR_LOW  0x0300
#define LAPIC_ICR_HIGH 0x0310
#define LAPIC_LVT_TMR  0x0320
#define LAPIC_TMRINIT  0x0380
#define LAPIC_TMRCURR  0x0390
#define LAPIC_TMRDIV   0x03E0

#endif
