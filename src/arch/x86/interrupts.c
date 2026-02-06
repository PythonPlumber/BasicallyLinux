#include "interrupts.h"
#include "arch/x86/idt.h"
#include "arch/x86/cpu.h"
#include "arch/x86/ports.h"
#include "serial.h"
#include "debug.h"
#include "kswapd.h"
#include "paging.h"
#include "sched.h"
#include "timer.h"

static interrupt_handler_t interrupt_handlers[256];
static volatile uint32_t interrupt_depth = 0;

static const char* exception_messages[32] = {
    "Divide By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating Point", "Alignment Check", "Machine Check", "SIMD Floating Point",
    "Virtualization", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

static registers_t* irq0_handler(registers_t* regs) {
    uint32_t cpu = cpu_get_id();
    
    if (cpu == 0) {
        static uint32_t ticks = 0;
        ticks++;
        if (ticks % 100 == 0) {
            serial_write_string("T");
        }
        timer_handler();
        kswapd_tick();
    }

    process_t* previous = 0;
    process_t* next = switch_task(regs, &previous);
    if (!next || next == previous) {
        return regs;
    }

    if (previous) {
        previous->esp = (uint32_t)regs;
    }

    if (next->page_directory) {
        mmu_switch_space((uintptr_t)next->page_directory);
    }

    return (registers_t*)next->esp;
}

void intc_init(void) {
    idt_init();
    for (uint32_t i = 0; i < 256; ++i) {
        interrupt_handlers[i] = 0;
    }
    interrupts_register_handler(32, irq0_handler);
}

void interrupts_register_handler(uint32_t vector, interrupt_handler_t handler) {
    if (vector < 256) {
        interrupt_handlers[vector] = handler;
    }
}

void interrupts_send_eoi(uint32_t vector) {
    if (vector >= 32) {
        // Check if LAPIC is enabled before sending EOI
        volatile uint32_t* lapic = (volatile uint32_t*)LAPIC_BASE;
        if (lapic[LAPIC_SVR/4] & 0x100) {
            lapic[LAPIC_EOI/4] = 0;
        }
        if (vector >= 32 && vector <= 47) {
            if (vector >= 40) outb(0xA0, 0x20);
            outb(0x20, 0x20);
        }
    }
}

void interrupts_enable_irq(uint32_t irq) {
    if (irq < 8) {
        uint8_t mask = inb(0x21);
        mask &= ~(1 << irq);
        outb(0x21, mask);
    } else if (irq < 16) {
        uint8_t mask = inb(0xA1);
        mask &= ~(1 << (irq - 8));
        outb(0xA1, mask);
    }
}

void interrupts_disable_irq(uint32_t irq) {
    if (irq < 8) {
        uint8_t mask = inb(0x21);
        mask |= (1 << irq);
        outb(0x21, mask);
    } else if (irq < 16) {
        uint8_t mask = inb(0xA1);
        mask |= (1 << (irq - 8));
        outb(0xA1, mask);
    }
}

void interrupts_send_ipi(uint32_t target_cpu, uint32_t vector) {
    *(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_HIGH) = target_cpu << 24;
    *(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_LOW) = 0x4000 | (vector & 0xFF);
}

static void dump_registers(registers_t* regs) {
    serial_write_string("EXCEPTION: ");
    serial_write_string(exception_messages[regs->int_no & 31]);
    serial_write_string("\n");
    serial_write_label_hex("EIP: ", regs->eip);
    serial_write_label_hex("EAX: ", regs->eax);
    serial_write_label_hex("EBX: ", regs->ebx);
    serial_write_label_hex("ECX: ", regs->ecx);
    serial_write_label_hex("EDX: ", regs->edx);
    serial_write_label_hex("ESP: ", regs->esp);
    serial_write_label_hex("EBP: ", regs->ebp);
    serial_write_label_hex("EFL: ", regs->eflags);
    serial_write_label_hex("ERR: ", regs->err_code);
}

registers_t* x86_interrupt_handler(registers_t* regs) {
    uint32_t vector = regs->int_no;
    
    interrupt_depth++;

    if (vector < 32) {
        // Exception handling
        if (vector == 14) { // Page Fault
            uintptr_t fault_addr = mmu_get_fault_addr();
            if (vm_handle_page_fault(scheduler_current(), fault_addr, regs->err_code)) {
                interrupt_depth--;
                return regs;
            }
        }
        
        dump_registers(regs);
        debug_backtrace_from(regs->ebp, 16);
        cpu_halt();
        for(;;);
    }
    
    if (vector >= 32) {
        interrupts_send_eoi(vector);
    }

    if (interrupt_handlers[vector]) {
        registers_t* new_regs = interrupt_handlers[vector](regs);
        interrupt_depth--;
        return new_regs;
    }
    
    interrupt_depth--;
    return regs;
}
