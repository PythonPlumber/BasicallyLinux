#include "types.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

typedef registers_t* (*isr_t)(registers_t* regs);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t type, uint8_t dpl, uint8_t present);
void idt_init(void);
void idt_load(uint32_t idt_ptr_addr);
void register_interrupt_handler(uint8_t n, isr_t handler);
void isr_init(void);
uint64_t timer_get_ticks(void);
void pic_remap(void);
