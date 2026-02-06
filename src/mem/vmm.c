#include "paging.h"
#include "arch/x86/mmu.h"
#include "mem/pmm.h"
#include "mem/heap.h"
#include "process.h"
#include "util.h"
#include "drivers/serial.h"

#define VM_PAGE_SIZE 4096u
#define SHARED_MAX 8u
#define SHARED_MAX_PAGES 16u

static uint32_t shared_pages[SHARED_MAX][SHARED_MAX_PAGES];
static uint32_t shared_counts[SHARED_MAX];
static uint32_t shared_refs[SHARED_MAX];

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t align_down(uint32_t value, uint32_t align) {
    return value & ~(align - 1);
}

void vmm_init(void) {
    for (uint32_t i = 0; i < SHARED_MAX; ++i) {
        shared_counts[i] = 0;
        shared_refs[i] = 0;
    }
}

static uint32_t vm_page_flags(uint32_t flags) {
    uint32_t pf = PAGE_FLAG_PRESENT;
    if (flags & VM_WRITE) pf |= PAGE_FLAG_WRITE;
    if (flags & VM_USER) pf |= PAGE_FLAG_USER;
    return pf;
}

int vm_map_region(process_t* proc, uint32_t start, uint32_t size, uint32_t flags) {
    if (!proc || size == 0) return 0;
    if (proc->region_count >= PROCESS_MAX_REGIONS) return 0;

    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t end = align_up(start + size, VM_PAGE_SIZE);
    
    if (flags & VM_USER) {
        if (base < VM_USER_BASE || end > VM_USER_LIMIT) return 0;
    }

    vm_region_t* region = &proc->regions[proc->region_count++];
    region->start = base;
    region->end = end;
    region->flags = flags;
    region->shared_id = 0;

    if (flags & (VM_DEMAND | VM_GUARD)) return 1;

    uint32_t mmu_flags = vm_page_flags(flags);
    uintptr_t* dir = (uintptr_t*)proc->page_directory;
    if (!dir) dir = mmu_get_current_space();

    for (uint32_t addr = base; addr < end; addr += VM_PAGE_SIZE) {
        uint32_t phys = pmm_alloc_block();
        if (!phys) return 0;
        
        // Safely zero the page using a temporary mapping.
        void* ptr = mmu_map_temp(phys);
        memset(ptr, 0, VM_PAGE_SIZE);
        mmu_unmap_temp();
        
        mmu_map_page_dir(dir, addr, phys, mmu_flags);
    }
    return 1;
}

int vm_unmap_region(process_t* proc, uint32_t start, uint32_t size) {
    if (!proc || size == 0) return 0;
    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t end = align_up(start + size, VM_PAGE_SIZE);

    for (uint32_t i = 0; i < proc->region_count; ++i) {
        vm_region_t* region = &proc->regions[i];
        if (region->start == base && region->end == end) {
            if (!(region->flags & (VM_GUARD | VM_DEMAND | VM_SHARED))) {
                uintptr_t* dir = (uintptr_t*)proc->page_directory;
                if (!dir) dir = mmu_get_current_space();
                for (uint32_t addr = base; addr < end; addr += VM_PAGE_SIZE) {
                    uintptr_t phys = mmu_get_phys_dir(dir, addr);
                    if (phys) {
                        pmm_free_block((uint32_t)phys);
                        mmu_unmap_page_dir(dir, addr);
                    }
                }
            }
            proc->regions[i] = proc->regions[proc->region_count - 1];
            proc->region_count--;
            return 1;
        }
    }
    return 0;
}

int vm_handle_page_fault(process_t* proc, uint32_t addr, uint32_t err_code) {
    if (!proc) return 0;
    uint32_t fault_addr = align_down(addr, VM_PAGE_SIZE);

    for (uint32_t i = 0; i < proc->region_count; ++i) {
        vm_region_t* region = &proc->regions[i];
        if (fault_addr >= region->start && fault_addr < region->end) {
            if (region->flags & VM_GUARD) return 0;
            
            if (region->flags & VM_SHARED) {
                uint32_t page_index = (fault_addr - region->start) / VM_PAGE_SIZE;
                if (region->shared_id >= SHARED_MAX || page_index >= shared_counts[region->shared_id]) {
                    return 0;
                }
                uintptr_t* dir = (uintptr_t*)proc->page_directory;
                if (!dir) dir = mmu_get_current_space();
                mmu_map_page_dir(dir, fault_addr, shared_pages[region->shared_id][page_index], vm_page_flags(region->flags));
                return 1;
            }

            if (region->flags & VM_COW) {
                // Handle Copy-on-Write
                uintptr_t* dir = (uintptr_t*)proc->page_directory;
                if (!dir) dir = mmu_get_current_space();
                uintptr_t old_phys = mmu_get_phys_dir(dir, fault_addr);
                uintptr_t new_phys = pmm_alloc_block();
                if (!new_phys) return 0;
                
                void* dst = mmu_map_temp(new_phys);
                void* src = mmu_map_temp2(old_phys);
                memcpy(dst, src, VM_PAGE_SIZE);
                mmu_unmap_temp2();
                mmu_unmap_temp();

                uint32_t flags = (region->flags & ~VM_COW) | VM_WRITE;
                mmu_map_page_dir(dir, fault_addr, new_phys, vm_page_flags(flags));
                region->flags = flags;
                return 1;
            }

            if (region->flags & VM_DEMAND) {
                uintptr_t* dir = (uintptr_t*)proc->page_directory;
                if (!dir) dir = mmu_get_current_space();
                uintptr_t phys = pmm_alloc_block();
                if (!phys) return 0;
                
                void* ptr = mmu_map_temp(phys);
                memset(ptr, 0, VM_PAGE_SIZE);
                mmu_unmap_temp();

                mmu_map_page_dir(dir, fault_addr, phys, vm_page_flags(region->flags));
                return 1;
            }
        }
    }
    return 0;
}

int vm_create_shared(uint32_t pages) {
    if (pages == 0 || pages > SHARED_MAX_PAGES) return -1;
    for (uint32_t i = 0; i < SHARED_MAX; ++i) {
        if (shared_counts[i] == 0) {
            for (uint32_t p = 0; p < pages; ++p) {
                uint32_t phys = pmm_alloc_block();
                if (!phys) return -1;
                shared_pages[i][p] = phys;
            }
            shared_counts[i] = pages;
            shared_refs[i] = 0;
            return (int)i;
        }
    }
    return -1;
}

int vm_map_shared(process_t* proc, uint32_t shared_id, uint32_t start) {
    if (!proc || shared_id >= SHARED_MAX || shared_counts[shared_id] == 0) return 0;
    
    uint32_t size = shared_counts[shared_id] * VM_PAGE_SIZE;
    uint32_t flags = VM_READ | VM_WRITE | VM_USER | VM_SHARED;
    
    vm_region_t* region = &proc->regions[proc->region_count++];
    region->start = start;
    region->end = start + size;
    region->flags = flags;
    region->shared_id = shared_id;
    
    uintptr_t* dir = (uintptr_t*)proc->page_directory;
    if (!dir) dir = mmu_get_current_space();
    
    for (uint32_t i = 0; i < shared_counts[shared_id]; ++i) {
        mmu_map_page_dir(dir, start + i * VM_PAGE_SIZE, shared_pages[shared_id][i], vm_page_flags(flags));
    }
    shared_refs[shared_id]++;
    return 1;
}

int paging_clone_cow_range(uint32_t* parent, uint32_t* child, uint32_t start, uint32_t end) {
    if (!parent || !child) return 0;
    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t limit = align_up(end, VM_PAGE_SIZE);
    
    for (uint32_t addr = base; addr < limit; addr += VM_PAGE_SIZE) {
        uintptr_t phys = mmu_get_phys_dir((uintptr_t*)parent, addr);
        if (!phys) continue;
        
        uint32_t flags = mmu_get_flags_dir((uintptr_t*)parent, addr);
        // Clear write flag to trigger COW on next write
        flags &= ~PAGE_FLAG_WRITE;
        
        mmu_map_page_dir((uintptr_t*)child, addr, phys, flags);
        mmu_map_page_dir((uintptr_t*)parent, addr, phys, flags);
    }
    return 1;
}

void vm_init_process(process_t* proc) {
    if (!proc) return;
    proc->region_count = 0;
    for (uint32_t i = 0; i < PROCESS_MAX_REGIONS; ++i) {
        proc->regions[i].start = 0;
        proc->regions[i].end = 0;
        proc->regions[i].flags = 0;
        proc->regions[i].shared_id = 0;
    }
}
