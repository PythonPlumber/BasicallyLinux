#ifndef IDT_H
#define IDT_H

#include "types.h"
#include "cpu.h"

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

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t type, uint8_t dpl, uint8_t present);
void idt_init(void);
void idt_load_current(void);
void idt_load(uint32_t idt_ptr_addr);
void pic_remap(void);

#endif
