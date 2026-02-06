#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "arch/x86/cpu.h"

// Generic register state structure
// This will be defined by each architecture
typedef struct registers registers_t;

// CPU features
int cpu_has_feature(uint32_t feature);
#define CPU_FEATURE_SSE   1
#define CPU_FEATURE_SSE41 2
#define CPU_FEATURE_FPU   3

// Feature enablement
void cpu_enable_feature(uint32_t feature);

// CPU information
void cpu_get_vendor(char* out, uint32_t max);

// System control
void cpu_reboot(void);

// Architecture-specific initialization
void arch_init(void);

// Interrupt control
void interrupts_enable(void);
void interrupts_disable(void);
int interrupts_enabled(void);

// CPU identification
uint32_t cpu_get_id(void);
uint32_t cpu_get_count(void);

#include "arch/x86/mmu.h"

// TLB management
static inline void tlb_flush(uintptr_t addr) { mmu_tlb_flush(addr); }
static inline void tlb_flush_all(void) { mmu_tlb_flush_all(); }

// Paging control
static inline void paging_switch_directory(uintptr_t dir_phys) { mmu_switch_space(dir_phys); }

// Halt the CPU
void cpu_halt(void);
void cpu_pause(void);

// Port I/O (x86 specific, but can be stubs elsewhere)
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t val);
uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t val);
uint32_t inl(uint16_t port);
void outl(uint16_t port, uint32_t val);

#endif
