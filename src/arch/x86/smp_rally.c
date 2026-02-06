#include "smp.h"
#include "arch/x86/smp_rally.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "paging.h"
#include "sched.h"
#include "serial.h"
#include "types.h"
#include "util.h"
#include "cpu.h"
#include "arch/x86/cpu.h"

#define TRAMPOLINE_ADDR 0x1000

extern uint8_t smp_trampoline_start[];
extern uint8_t smp_trampoline_end[];
extern uint32_t smp_ap_stack;

static uint32_t cpu_count = 1;
static uint32_t online_mask = 1;
static uint8_t ap_stacks[32][4096] __attribute__((aligned(16)));

void lapic_timer_init(uint32_t freq) {
    uint32_t* lapic = (uint32_t*)LAPIC_BASE;
    
    // Divide by 16
    lapic[LAPIC_TMRDIV/4] = 0x3;
    
    // Periodic mode, vector 32 (0x20)
    lapic[LAPIC_LVT_TMR/4] = 32 | 0x20000;
    
    // Set initial count
    lapic[LAPIC_TMRINIT/4] = 100000000 / freq; // Approximate for 100MHz bus
}

void smp_ap_entry(void) {
    uint32_t cpu_id = (*(volatile uint32_t*)(LAPIC_BASE + LAPIC_ID)) >> 24;
    
    // Load page directory
    load_cr3(paging_get_directory());
    
    // Enable PSE first for 4MB pages
    enable_pse();
    
    // Enable paging
    paging_enable();
    
    // Load IDT
    idt_load_current();
    
    // Initialize LAPIC timer for this CPU
    lapic_timer_init(100); // 100Hz
    
    serial_write_string("DEBUG: AP ");
    serial_write_hex32(cpu_id);
    serial_write_string(" online and timer started\n");
    
    // Mark as online
    __sync_fetch_and_or(&online_mask, (1u << cpu_id));
    
    // Enable interrupts
    asm volatile("sti");
    
    // Enter the scheduler loop
    scheduler_loop();
}

static void lapic_wait(void) {
    while (*(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_LOW) & 0x1000) {
        asm volatile("pause");
    }
}

static void lapic_send_ipi(uint32_t apic_id, uint32_t val) {
    *(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_HIGH) = apic_id << 24;
    *(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_LOW) = val;
    lapic_wait();
}

void smp_rally_init(void) {
    cpu_count = 1;
    online_mask = 1;

    uint32_t madt_addr = acpi_find_table("APIC");
    if (!madt_addr) {
        serial_write_string("DEBUG: MADT not found, SMP disabled\n");
        return;
    }

    acpi_madt_t* madt = (acpi_madt_t*)madt_addr;
    uint32_t length = madt->header.length;
    uint32_t current = madt_addr + sizeof(acpi_madt_t);
    uint32_t end = madt_addr + length;

    // Copy trampoline to 0x1000
    uint32_t trampoline_size = (uint32_t)smp_trampoline_end - (uint32_t)smp_trampoline_start;
    memcpy((void*)TRAMPOLINE_ADDR, smp_trampoline_start, trampoline_size);
    serial_write_string("DEBUG: Trampoline copied to 0x1000\n");

    cpu_count = 0;
    uint32_t bsp_id = (*(volatile uint32_t*)(LAPIC_BASE + LAPIC_ID)) >> 24;

    while (current < end) {
        acpi_madt_entry_t* entry = (acpi_madt_entry_t*)current;
        if (entry->length == 0) break;

        if (entry->type == 0) { // Local APIC
            acpi_madt_local_apic_t* lapic = (acpi_madt_local_apic_t*)current;
            if (lapic->flags & 1) { // Enabled
                uint32_t apic_id = lapic->apic_id;
                cpu_count++;
                
                if (apic_id != bsp_id) {
                    serial_write_string("DEBUG: Booting AP ");
                    serial_write_hex32(apic_id);
                    serial_write_string("\n");
                    
                    // Set stack for this AP in the trampoline copy at 0x1000
                    uint32_t stack_ptr_offset = (uint32_t)&smp_ap_stack - (uint32_t)smp_trampoline_start;
                    uint32_t* ap_stack_loc = (uint32_t*)(TRAMPOLINE_ADDR + stack_ptr_offset);
                    *ap_stack_loc = (uint32_t)&ap_stacks[apic_id][4096];
                    
                    // INIT IPI
                    lapic_send_ipi(apic_id, 0x00000500);
                    timer_sleep(10); // Wait 10ms
                    
                    // SIPI IPI (Vector 0x01 -> Address 0x1000)
                    lapic_send_ipi(apic_id, 0x00000600 | (TRAMPOLINE_ADDR >> 12));
                    timer_sleep(1); // Wait 1ms

                    // Wait for AP to mark itself online
                    uint32_t timeout = 10000000;
                    while (!((*(volatile uint32_t*)&online_mask) & (1u << apic_id)) && timeout-- > 0) {
                        asm volatile("pause");
                    }

                    if (timeout > 0) {
                        serial_write_string("DEBUG: AP ");
                        serial_write_hex32(apic_id);
                        serial_write_string(" booted successfully\n");
                    } else {
                        serial_write_string("DEBUG: AP ");
                        serial_write_hex32(apic_id);
                        serial_write_string(" boot TIMEOUT\n");
                    }
                }
            }
        }
        current += entry->length;
    }

    if (cpu_count == 0) cpu_count = 1;
    serial_write_string("DEBUG: SMP Init complete, CPUs: ");
    serial_write_hex32(cpu_count);
    serial_write_string("\n");
}

uint32_t smp_get_cpu_count(void) {
    return cpu_count;
}

uint32_t smp_get_online_mask(void) {
    return online_mask;
}

void smp_send_ipi(uint32_t target_cpu, uint32_t val) {
    lapic_send_ipi(target_cpu, val);
}

void smp_broadcast_ipi(uint32_t val) {
    // 0x000C0000 = All excluding self
    *(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_HIGH) = 0;
    *(volatile uint32_t*)(LAPIC_BASE + LAPIC_ICR_LOW) = 0x000C0000 | val;
    lapic_wait();
}

void smp_init(void) {
    smp_rally_init();
}
