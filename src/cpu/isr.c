#include "ai_model.h"
#include "debug.h"
#include "idt.h"
#include "kswapd.h"
#include "paging.h"
#include "ports.h"
#include "sched.h"
#include "serial.h"
#include "types.h"

#include "timer.h"

static isr_t interrupt_handlers[256];
static volatile uint32_t interrupt_depth = 0;

static uint16_t* const vga_buffer = (uint16_t*)0xB8000;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

static uint16_t vga_entry(uint8_t ch, uint8_t color) {
    return (uint16_t)ch | ((uint16_t)color << 8);
}

static void vga_clear(uint8_t color) {
    uint16_t blank = vga_entry(' ', color);
    for (uint32_t i = 0; i < 80 * 25; ++i) {
        vga_buffer[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void vga_put_char(uint8_t ch, uint8_t color) {
    if (ch == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        uint32_t idx = (uint32_t)cursor_y * 80 + cursor_x;
        vga_buffer[idx] = vga_entry(ch, color);
        cursor_x++;
    }
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= 25) {
        vga_clear(color);
    }
}

static void vga_write_string(const char* str, uint8_t color) {
    for (uint32_t i = 0; str[i] != 0; ++i) {
        vga_put_char((uint8_t)str[i], color);
    }
}

static void vga_write_hex32(uint32_t value, uint8_t color) {
    static const char hex[] = "0123456789ABCDEF";
    vga_write_string("0x", color);
    for (int i = 7; i >= 0; --i) {
        uint8_t nibble = (uint8_t)((value >> (i * 4)) & 0xF);
        vga_put_char((uint8_t)hex[nibble], color);
    }
}

static void vga_write_label_hex(const char* label, uint32_t value, uint8_t color) {
    vga_write_string(label, color);
    vga_write_hex32(value, color);
    vga_put_char('\n', color);
}

static void serial_write_label_hex(const char* label, uint32_t value) {
    serial_write(label);
    serial_write_hex32(value);
    serial_write("\n");
}

static const char* exception_messages[32] = {
    "Divide By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point",
    "Virtualization",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

static void print_registers(registers_t* regs) {
    uint8_t color = 0x0C;
    vga_write_label_hex("EAX: ", regs->eax, color);
    vga_write_label_hex("EBX: ", regs->ebx, color);
    vga_write_label_hex("ECX: ", regs->ecx, color);
    vga_write_label_hex("EDX: ", regs->edx, color);
    vga_write_label_hex("ESI: ", regs->esi, color);
    vga_write_label_hex("EDI: ", regs->edi, color);
    vga_write_label_hex("EBP: ", regs->ebp, color);
    vga_write_label_hex("ESP: ", regs->esp, color);
    vga_write_label_hex("EIP: ", regs->eip, color);
    vga_write_label_hex("CS:  ", regs->cs, color);
    vga_write_label_hex("EFL: ", regs->eflags, color);
    vga_write_label_hex("ERR: ", regs->err_code, color);
    serial_write_label_hex("EAX: ", regs->eax);
    serial_write_label_hex("EBX: ", regs->ebx);
    serial_write_label_hex("ECX: ", regs->ecx);
    serial_write_label_hex("EDX: ", regs->edx);
    serial_write_label_hex("ESI: ", regs->esi);
    serial_write_label_hex("EDI: ", regs->edi);
    serial_write_label_hex("EBP: ", regs->ebp);
    serial_write_label_hex("ESP: ", regs->esp);
    serial_write_label_hex("EIP: ", regs->eip);
    serial_write_label_hex("CS:  ", regs->cs);
    serial_write_label_hex("EFL: ", regs->eflags);
    serial_write_label_hex("ERR: ", regs->err_code);
}

static registers_t* exception_handler(registers_t* regs) {
    interrupt_depth++;
    if (interrupt_depth > 1) {
        vga_clear(0x0C);
        vga_write_string("Nested Exception\n", 0x0C);
        vga_write_label_hex("INT: ", regs->int_no, 0x0C);
        vga_write_label_hex("ESP: ", regs->esp, 0x0C);
        serial_write("Nested Exception\n");
        serial_write_label_hex("INT: ", regs->int_no);
        serial_write_label_hex("ESP: ", regs->esp);
        for (;;) { }
    }
    if (regs->esp < 0x1000) {
        vga_clear(0x0C);
        vga_write_string("Stack Fault\n", 0x0C);
        vga_write_label_hex("ESP: ", regs->esp, 0x0C);
        serial_write("Stack Fault\n");
        serial_write_label_hex("ESP: ", regs->esp);
        for (;;) { }
    }
    if ((regs->int_no & 31) == 14) {
        uint32_t addr = get_page_fault_addr();
        process_t* current = scheduler_current();
        if (vm_handle_page_fault(current, addr, regs->err_code)) {
            if (interrupt_depth > 0) {
                interrupt_depth--;
            }
            return regs;
        }
    }
    vga_clear(0x0C);
    vga_write_string("EXCEPTION: ", 0x0C);
    vga_write_string(exception_messages[regs->int_no & 31], 0x0C);
    vga_put_char('\n', 0x0C);
    vga_write_label_hex("INT: ", regs->int_no, 0x0C);
    serial_write("EXCEPTION: ");
    serial_write(exception_messages[regs->int_no & 31]);
    serial_write("\n");
    serial_write_label_hex("INT: ", regs->int_no);
    print_registers(regs);
    debug_backtrace_from(regs->ebp, 16);
    if ((regs->int_no & 31) == 14) {
        ai_explain_page_fault(regs->err_code);
    }
    for (;;) { }
}

static registers_t* irq0_handler(registers_t* regs) {
    static uint32_t ticks = 0;
    ticks++;
    if (ticks % 100 == 0) {
        serial_write_string("T"); // T for Tick
    }
    timer_handler();
    kswapd_tick();
    process_t* previous = 0;
    process_t* next = switch_task(regs, &previous);
    if (!next || next == previous) {
        return regs;
    }

    if (previous) {
        previous->esp = (uint32_t)regs;
    }

    if (next->page_directory) {
        load_cr3(next->page_directory);
    }

    return (registers_t*)next->esp;
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void isr_init(void) {
    for (uint32_t i = 0; i < 256; ++i) {
        interrupt_handlers[i] = 0;
    }
    for (uint8_t i = 0; i < 32; ++i) {
        register_interrupt_handler(i, exception_handler);
    }
    register_interrupt_handler(32, irq0_handler);
}

registers_t* isr_handler(registers_t* regs) {
    serial_write_string("DEBUG: ISR ");
    serial_write_hex32(regs->int_no);
    serial_write_string("\n");
    if (interrupt_handlers[regs->int_no]) {
        return interrupt_handlers[regs->int_no](regs);
    }
    return regs;
}

registers_t* irq_handler(registers_t* regs) {
    uint32_t int_no = regs->int_no;
    // serial_write_string("I"); // Too noisy? Let's use it for now
    if (interrupt_handlers[int_no]) {
        regs = interrupt_handlers[int_no](regs);
    }
    if (int_no >= 40) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
    return regs;
}
