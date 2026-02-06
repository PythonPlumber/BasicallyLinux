#ifndef PAGING_H
#define PAGING_H

#include "process.h"
#include "types.h"
#include "arch/x86/mmu.h"

// Generic VMM functions (implemented in vmm.c)
void vmm_init(void);
int vm_map_region(process_t* proc, uint32_t start, uint32_t size, uint32_t flags);
int vm_unmap_region(process_t* proc, uint32_t start, uint32_t size);
int vm_handle_page_fault(process_t* proc, uint32_t addr, uint32_t err_code);
int vm_create_shared(uint32_t pages);
int vm_map_shared(process_t* proc, uint32_t shared_id, uint32_t start);
void vm_init_process(process_t* proc);
int paging_clone_cow_range(uint32_t* parent, uint32_t* child, uint32_t start, uint32_t end);

// Legacy/Compatibility wrappers (should eventually be replaced by mmu.h calls)
static inline void paging_init(void) { mmu_init(); vmm_init(); }
static inline void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) { mmu_map_page(virtual_addr, physical_addr, flags); }
static inline void map_page_dir(uint32_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) { mmu_map_page_dir((uintptr_t*)dir, virtual_addr, physical_addr, flags); }
static inline void map_page_4mb(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) { mmu_map_page_4mb(virtual_addr, physical_addr, flags); }
static inline void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) { mmu_map_page(virtual_addr, physical_addr, flags); }
static inline void vmm_unmap_page(uint32_t virtual_addr) { mmu_unmap_page(virtual_addr); }
static inline void unmap_page_dir(uint32_t* dir, uint32_t virtual_addr) { mmu_unmap_page_dir((uintptr_t*)dir, virtual_addr); }
static inline uint32_t* paging_get_directory(void) { return (uint32_t*)mmu_get_current_space(); }
static inline uint32_t* paging_create_directory(void) { return (uint32_t*)mmu_create_space(); }
static inline void paging_destroy_directory(uint32_t* dir) { mmu_destroy_space((uintptr_t*)dir); }
static inline void paging_switch_directory(uintptr_t dir_phys) { mmu_switch_space(dir_phys); }

// Architecture-specific (x86) - should be moved to arch headers eventually
static inline void load_cr3(uint32_t* dir) { mmu_switch_space((uintptr_t)dir); }
static inline uint32_t get_page_fault_addr(void) { return (uint32_t)mmu_get_fault_addr(); }
static inline int is_addr_valid(uint32_t addr) { return mmu_get_phys(addr) != 0; }

#define VM_READ 0x1u
#define VM_WRITE 0x2u
#define VM_EXEC 0x4u
#define VM_USER 0x8u
#define VM_GUARD 0x10u
#define VM_SHARED 0x20u
#define VM_COW 0x40u
#define VM_DEMAND 0x80u
#define VM_USER_BASE 0x00001000u
#define VM_USER_LIMIT 0xBFFFFFFFu
#define VM_KERNEL_BASE 0xC0000000u

#endif
