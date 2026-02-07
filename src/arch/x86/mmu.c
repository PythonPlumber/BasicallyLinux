#include "arch/x86/mmu.h"
#include "cpu.h"
#include "types.h"
#include "util.h"
#include "mem/pmm.h"
#include "mem/heap.h"
#include "util.h"
#include "drivers/serial.h"
#include "arch/x86/hw_detect.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_PS 0x80

#define VM_KERNEL_BASE 0xC0000000

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[512][1024] __attribute__((aligned(4096)));
static uint32_t page_table_count = 0;
static spinlock_t paging_lock = 0;
static spinlock_t temp_map_lock = 0;
static uint32_t temp_map_flags = 0;

void* mmu_map_temp(uintptr_t phys) {
    temp_map_flags = spin_lock_irqsave(&temp_map_lock);
    uintptr_t temp_virt = 0xFFBFF000;
    mmu_map_page(temp_virt, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);
    return (void*)temp_virt;
}

void mmu_unmap_temp(void) {
    uintptr_t temp_virt = 0xFFBFF000;
    mmu_unmap_page(temp_virt);
    spin_unlock_irqrestore(&temp_map_lock, temp_map_flags);
}

void* mmu_map_temp2(uintptr_t phys) {
    // Note: This assumes temp_map_lock is ALREADY held by mmu_map_temp
    uintptr_t temp_virt = 0xFFBFE000;
    mmu_map_page(temp_virt, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE);
    return (void*)temp_virt;
}

void mmu_unmap_temp2(void) {
    uintptr_t temp_virt = 0xFFBFE000;
    mmu_unmap_page(temp_virt);
}

static uint32_t* alloc_table(void) {
    uint32_t flags = spin_lock_irqsave(&paging_lock);
    if (page_table_count >= 512) {
        spin_unlock_irqrestore(&paging_lock, flags);
        return 0;
    }
    uint32_t* table = page_tables[page_table_count++];
    spin_unlock_irqrestore(&paging_lock, flags);
    return table;
}

static uint32_t* alloc_table_any(void) {
    uint32_t* table = alloc_table();
    if (!table) {
        // Try PMM first
        uint32_t phys = pmm_alloc_block();
        if (phys) {
            table = (uint32_t*)phys; // Early identity mapping
        } else {
            // Fallback to heap
            table = (uint32_t*)aligned_alloc(4096, 4096);
        }
        if (!table) return 0;
    }
    memset(table, 0, 4096);
    return table;
}

void mmu_enable_pse(void) {
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x10;
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
}

void mmu_enable_paging(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

void mmu_init(void) {
    for (uint32_t i = 0; i < 1024; ++i) {
        page_directory[i] = PAGE_RW;
    }

    // Identity map 512MB (128 * 4MB) for kernel/drivers
    for (uint32_t t = 0; t < 128; ++t) {
        uint32_t* table = alloc_table();
        if (!table) break;
        for (uint32_t i = 0; i < 1024; ++i) {
            table[i] = (t * 0x400000 + i * 0x1000) | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[t] = ((uint32_t)table) | PAGE_PRESENT | PAGE_RW;
    }

    // Identity map critical regions (LAPIC, IOAPIC)
    mmu_map_page_dir((uintptr_t*)page_directory, 0xFEC00000, 0xFEC00000, PAGE_PRESENT | PAGE_RW);
    mmu_map_page_dir((uintptr_t*)page_directory, 0xFEE00000, 0xFEE00000, PAGE_PRESENT | PAGE_RW);
    mmu_map_page_dir((uintptr_t*)page_directory, 0xFF000000, 0xFF000000, PAGE_PRESENT | PAGE_RW);

    // Identity map ACPI tables
    uint32_t rsdt = hw_acpi_get_rsdt_addr();
    if (rsdt) {
        acpi_sdt_header_t* rsdt_hdr = (acpi_sdt_header_t*)rsdt;
        // Map all pages covered by RSDT
        for (uint32_t p = rsdt & ~0xFFF; p < rsdt + rsdt_hdr->length; p += 4096) {
            mmu_map_page_dir((uintptr_t*)page_directory, p, p, PAGE_PRESENT | PAGE_RW);
        }
        
        uint32_t count = hw_acpi_get_rsdt_entries();
        if (count > 256) count = 256; // Sanity check
        
        for (uint32_t i = 0; i < count; i++) {
            uint32_t entry = hw_acpi_get_rsdt_entry(i);
            if (entry) {
                acpi_sdt_header_t* hdr = (acpi_sdt_header_t*)entry;
                // Map all pages covered by this table
                for (uint32_t p = entry & ~0xFFF; p < entry + hdr->length; p += 4096) {
                    mmu_map_page_dir((uintptr_t*)page_directory, p, p, PAGE_PRESENT | PAGE_RW);
                }
            }
        }
    }

    mmu_switch_space((uintptr_t)page_directory);
    
    mmu_enable_pse();
    mmu_enable_paging();
}

void mmu_map_page_dir(uintptr_t* dir, uintptr_t virt, uintptr_t phys, uint32_t flags) {
    if (!dir) return;
    uint32_t irq_flags = spin_lock_irqsave(&paging_lock);
    
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t* table;

    if (dir[pd_index] & PAGE_PRESENT) {
        table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
    } else {
        table = alloc_table_any();
        if (!table) {
            spin_unlock_irqrestore(&paging_lock, irq_flags);
            return;
        }
        dir[pd_index] = (uint32_t)table | PAGE_PRESENT | PAGE_RW | (flags & PAGE_USER);
    }

    table[pt_index] = phys | flags | PAGE_PRESENT;
    mmu_tlb_flush(virt);
    spin_unlock_irqrestore(&paging_lock, irq_flags);
}

void mmu_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags) {
    mmu_map_page_dir((uintptr_t*)page_directory, virt, phys, flags);
}

void mmu_map_page_4mb(uintptr_t virt, uintptr_t phys, uint32_t flags) {
    uint32_t irq_flags = spin_lock_irqsave(&paging_lock);
    uintptr_t aligned_virt = virt & 0xFFC00000;
    uintptr_t aligned_phys = phys & 0xFFC00000;
    uint32_t pd_index = aligned_virt >> 22;
    page_directory[pd_index] = aligned_phys | flags | PAGE_PRESENT | PAGE_PS;
    mmu_tlb_flush(aligned_virt);
    spin_unlock_irqrestore(&paging_lock, irq_flags);
}

void mmu_unmap_page_dir(uintptr_t* dir, uintptr_t virt) {
    if (!dir) return;
    uint32_t irq_flags = spin_lock_irqsave(&paging_lock);
    
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    if (!(dir[pd_index] & PAGE_PRESENT)) {
        spin_unlock_irqrestore(&paging_lock, irq_flags);
        return;
    }
    
    if (dir[pd_index] & PAGE_PS) {
        dir[pd_index] = PAGE_RW;
    } else {
        uint32_t* table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
        table[pt_index] = 0;
    }
    
    mmu_tlb_flush(virt);
    spin_unlock_irqrestore(&paging_lock, irq_flags);
}

void mmu_unmap_page(uintptr_t virt) {
    mmu_unmap_page_dir((uintptr_t*)page_directory, virt);
}

void mmu_switch_space(uintptr_t dir_phys) {
    asm volatile("mov %0, %%cr3" :: "r"(dir_phys) : "memory");
}

void mmu_tlb_flush(uintptr_t virt) {
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void mmu_tlb_flush_all(void) {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

uintptr_t* mmu_get_current_space(void) {
    uintptr_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return (uintptr_t*)cr3;
}

uintptr_t* mmu_get_kernel_space(void) {
    return (uintptr_t*)page_directory;
}

uintptr_t mmu_get_phys_dir(uintptr_t* dir, uintptr_t virt) {
    if (!dir) return 0;
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    if (!(dir[pd_index] & PAGE_PRESENT)) return 0;
    
    if (dir[pd_index] & PAGE_PS) {
        return (dir[pd_index] & 0xFFC00000) | (virt & 0x3FFFFF);
    }
    
    uint32_t* table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
    return (table[pt_index] & 0xFFFFF000) | (virt & 0xFFF);
}

uintptr_t mmu_get_phys(uintptr_t virt) {
    return mmu_get_phys_dir((uintptr_t*)mmu_get_current_space(), virt);
}

uint32_t mmu_get_flags_dir(uintptr_t* dir, uintptr_t virt) {
    if (!dir) return 0;
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    if (!(dir[pd_index] & PAGE_PRESENT)) return 0;
    
    uint32_t entry;
    if (dir[pd_index] & PAGE_PS) {
        entry = dir[pd_index];
    } else {
        uint32_t* table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
        entry = table[pt_index];
    }

    uint32_t flags = 0;
    if (entry & PAGE_PRESENT) flags |= PAGE_FLAG_PRESENT;
    if (entry & PAGE_RW) flags |= PAGE_FLAG_WRITE;
    if (entry & PAGE_USER) flags |= PAGE_FLAG_USER;
    // Map other flags if needed
    return flags;
}

uintptr_t* mmu_create_space(void) {
    uint32_t* dir = (uint32_t*)aligned_alloc(4096, 4096);
    if (!dir) return 0;
    memset(dir, 0, 4096);
    
    // Copy the entire kernel/identity mapping (first 512MB + everything from VM_KERNEL_BASE)
    // First 512MB (identity mapped for drivers/early boot)
    for (uint32_t i = 0; i < 128; ++i) {
        dir[i] = page_directory[i];
    }
    
    // High memory mapping (from VM_KERNEL_BASE)
    uint32_t kernel_index = VM_KERNEL_BASE >> 22;
    for (uint32_t i = kernel_index; i < 1024; ++i) {
        dir[i] = page_directory[i];
    }
    return (uintptr_t*)dir;
}

void mmu_destroy_space(uintptr_t* dir) {
    if (!dir) return;
    
    // kernel_index is 768 (0xC0000000 >> 22)
    for (uint32_t i = 0; i < 768; ++i) {
        if (dir[i] & PAGE_PRESENT) {
            // Skip the identity mapped first 512MB if they match global directory
            if (i < 128 && dir[i] == page_directory[i]) {
                continue;
            }
            
            uint32_t* table = (uint32_t*)(dir[i] & 0xFFFFF000);
            
            // Don't free static page tables
            if ((uintptr_t)table >= (uintptr_t)page_tables && 
                (uintptr_t)table < (uintptr_t)page_tables + sizeof(page_tables)) {
                continue;
            }
            
            // Distinguish between tables allocated via PMM or heap
            if (is_heap_ptr(table)) {
                kfree(table);
            } else {
                pmm_free_block((uint32_t)table);
            }
        }
    }
    kfree(dir);
}

uintptr_t mmu_get_fault_addr(void) {
    uintptr_t addr;
    asm volatile("mov %%cr2, %0" : "=r"(addr));
    return addr;
}
