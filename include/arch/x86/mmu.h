#ifndef MMU_H
#define MMU_H

#include "types.h"

// Page flags
#define PAGE_FLAG_PRESENT  (1u << 0)
#define PAGE_FLAG_WRITE    (1u << 1)
#define PAGE_FLAG_USER     (1u << 2)
#define PAGE_FLAG_NOCACHE  (1u << 3)
#define PAGE_FLAG_WRITETHROUGH (1u << 4)
#define PAGE_FLAG_GLOBAL   (1u << 5)

// Page size
#define PAGE_SIZE 4096

// Architecture-specific paging initialization
void mmu_init(void);

// Map a page
void mmu_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);
void mmu_map_page_dir(uintptr_t* dir, uintptr_t virt, uintptr_t phys, uint32_t flags);
void mmu_map_page_4mb(uintptr_t virt, uintptr_t phys, uint32_t flags);

// Unmap a page
void mmu_unmap_page(uintptr_t virt);
void mmu_unmap_page_dir(uintptr_t* dir, uintptr_t virt);

// Switch address space
void mmu_switch_space(uintptr_t space_phys);

// Get current address space
uintptr_t* mmu_get_current_space(void);
uintptr_t* mmu_get_kernel_space(void);

// Get physical address from virtual
uintptr_t mmu_get_phys(uintptr_t virt);
uintptr_t mmu_get_phys_dir(uintptr_t* dir, uintptr_t virt);

// Get page flags
uint32_t mmu_get_flags_dir(uintptr_t* dir, uintptr_t virt);

// Address space management
uintptr_t* mmu_create_space(void);
void mmu_destroy_space(uintptr_t* dir);

// Fault handling
uintptr_t mmu_get_fault_addr(void);

// Temporary mapping for physical memory access
void* mmu_map_temp(uintptr_t phys);
void mmu_unmap_temp(void);
void* mmu_map_temp2(uintptr_t phys);
void mmu_unmap_temp2(void);

// TLB management
void mmu_tlb_flush(uintptr_t addr);
void mmu_tlb_flush_all(void);

// x86 Specific (move to arch/x86/mmu.h if exists)
void mmu_enable_pse(void);
void mmu_enable_paging(void);

#endif
