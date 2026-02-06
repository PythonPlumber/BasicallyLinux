#include "cpu.h"
#include "arch/x86/cpu.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "interrupts.h"
#include "syscall.h"
#include "mmu.h"
#include "timer.h"

void arch_init(void) {
    gdt_init();
    intc_init();
    syscall_init();
    mmu_init();
    vmm_init();
    mmu_tlb_flush_all();
    pit_init(100);
}

void interrupts_enable(void) {
    asm volatile("sti");
}

void interrupts_disable(void) {
    asm volatile("cli");
}

int interrupts_enabled(void) {
    uint32_t eflags;
    asm volatile("pushfl; popl %0" : "=r"(eflags));
    return (eflags & 0x200) != 0;
}

uint32_t cpu_get_id(void) {
    volatile uint32_t* lapic = (volatile uint32_t*)LAPIC_BASE;
    // Check if LAPIC is enabled and present by reading SVR
    if (lapic[LAPIC_SVR/4] & 0x100) {
        return lapic[LAPIC_ID/4] >> 24;
    }
    return 0;
}

void cpu_halt(void) {
    asm volatile("hlt");
}

void cpu_pause(void) {
    asm volatile("pause");
}

int cpu_has_feature(uint32_t feature) {
    uint32_t eax, ebx, ecx, edx;
    switch (feature) {
        case CPU_FEATURE_SSE:
            asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
            return (edx >> 25) & 1u;
        case CPU_FEATURE_SSE41:
            asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
            return (ecx >> 19) & 1u;
        default:
            return 0;
    }
}

void cpu_enable_feature(uint32_t feature) {
    if (feature == CPU_FEATURE_SSE || feature == CPU_FEATURE_FPU) {
        uint32_t cr0;
        asm volatile("mov %%cr0, %0" : "=r"(cr0));
        cr0 &= ~0x4u;
        cr0 |= 0x2u;
        asm volatile("mov %0, %%cr0" : : "r"(cr0));
        
        if (feature == CPU_FEATURE_SSE) {
            uint32_t cr4;
            asm volatile("mov %%cr4, %0" : "=r"(cr4));
            cr4 |= 0x600u;
            asm volatile("mov %0, %%cr4" : : "r"(cr4));
        }
    }
}

void cpu_get_vendor(char* out, uint32_t max) {
    if (!out || max < 13) {
        return;
    }
    uint32_t a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    ((uint32_t*)out)[0] = b;
    ((uint32_t*)out)[1] = d;
    ((uint32_t*)out)[2] = c;
    out[12] = 0;
}

void cpu_reboot(void) {
    interrupts_disable();
    // 0x64 is keyboard controller command port, 0xFE is reset CPU command
    outb(0x64, 0xFE);
    for (;;) {
        cpu_halt();
    }
}

// Port I/O implementations
uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
