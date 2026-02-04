#include "paging.h"
#include "heap.h"
#include "pmm.h"
#include "types.h"
#include "util.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_PS 0x80

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[16][1024] __attribute__((aligned(4096)));
static uint32_t page_table_count = 0;

#define VM_PAGE_SIZE 4096u
#define SHARED_MAX 8u
#define SHARED_MAX_PAGES 16u

static uint32_t shared_pages[SHARED_MAX][SHARED_MAX_PAGES];
static uint32_t shared_counts[SHARED_MAX];
static uint32_t shared_refs[SHARED_MAX];

static uint32_t* alloc_table(void) {
    if (page_table_count >= 16) {
        return 0;
    }
    return page_tables[page_table_count++];
}

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t align_down(uint32_t value, uint32_t align) {
    return value & ~(align - 1);
}

static uint32_t* alloc_table_any(void) {
    uint32_t* table = alloc_table();
    if (!table) {
        table = (uint32_t*)aligned_alloc(4096, 4096);
        if (!table) {
            return 0;
        }
    }
    memset(table, 0, 4096);
    return table;
}

static uint32_t* get_table_dir(uint32_t* dir, uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    if (!dir || !(dir[pd_index] & PAGE_PRESENT)) {
        return 0;
    }
    if (dir[pd_index] & PAGE_PS) {
        return 0;
    }
    return (uint32_t*)(dir[pd_index] & 0xFFFFF000);
}

void load_cr3(uint32_t* dir) {
    asm volatile("mov %0, %%cr3" : : "r"(dir));
}

void paging_enable(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

static void enable_pse(void) {
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x10;
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
}

void map_page_dir(uint32_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (!dir) {
        return;
    }
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    uint32_t* table;

    if (dir[pd_index] & PAGE_PRESENT) {
        table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
    } else {
        table = alloc_table_any();
        if (!table) {
            return;
        }
        dir[pd_index] = ((uint32_t)table) | flags | PAGE_PRESENT;
    }

    table[pt_index] = (physical_addr & 0xFFFFF000) | flags | PAGE_PRESENT;
}

void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    map_page_dir(page_directory, virtual_addr, physical_addr, flags);
}

void map_page_4mb(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    page_directory[pd_index] = (physical_addr & 0xFFC00000) | flags | PAGE_PRESENT | PAGE_PS;
}

void paging_init(void) {
    for (uint32_t i = 0; i < 1024; ++i) {
        page_directory[i] = PAGE_RW;
    }

    uint32_t* first_table = alloc_table();
    for (uint32_t i = 0; i < 1024; ++i) {
        first_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_RW;
    }
    page_directory[0] = ((uint32_t)first_table) | PAGE_PRESENT | PAGE_RW;

    load_cr3(page_directory);
    enable_pse();
    paging_enable();
}

uint32_t* paging_get_directory(void) {
    return page_directory;
}

void vmm_init(void) {
    paging_init();
}

void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    map_page(virtual_addr, physical_addr, flags);
}

void vmm_unmap_page(uint32_t virtual_addr) {
    unmap_page_dir(page_directory, virtual_addr);
}

void unmap_page_dir(uint32_t* dir, uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    if (!dir || !(dir[pd_index] & PAGE_PRESENT)) {
        return;
    }
    if (dir[pd_index] & PAGE_PS) {
        dir[pd_index] = PAGE_RW;
        return;
    }
    uint32_t* table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
    table[pt_index] = 0;
}

uint32_t get_page_fault_addr(void) {
    uint32_t addr;
    asm volatile("mov %%cr2, %0" : "=r"(addr));
    return addr;
}

int is_addr_valid(uint32_t addr) {
    return get_page_entry_dir(page_directory, addr) != 0;
}

uint32_t get_page_entry_dir(uint32_t* dir, uint32_t virtual_addr) {
    if (!dir) {
        return 0;
    }
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    if (!(dir[pd_index] & PAGE_PRESENT)) {
        return 0;
    }
    if (dir[pd_index] & PAGE_PS) {
        return dir[pd_index];
    }
    uint32_t* table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
    return table[pt_index];
}

void set_page_entry_dir(uint32_t* dir, uint32_t virtual_addr, uint32_t entry) {
    if (!dir) {
        return;
    }
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    if (!(dir[pd_index] & PAGE_PRESENT)) {
        return;
    }
    if (dir[pd_index] & PAGE_PS) {
        dir[pd_index] = entry;
        return;
    }
    uint32_t* table = (uint32_t*)(dir[pd_index] & 0xFFFFF000);
    table[pt_index] = entry;
}

uint32_t* paging_create_directory(void) {
    uint32_t* dir = (uint32_t*)aligned_alloc(4096, 4096);
    if (!dir) {
        return 0;
    }
    memset(dir, 0, 4096);
    uint32_t kernel_index = VM_KERNEL_BASE >> 22;
    for (uint32_t i = kernel_index; i < 1024; ++i) {
        dir[i] = page_directory[i];
    }
    return dir;
}

void paging_destroy_directory(uint32_t* dir) {
    (void)dir;
}

int paging_clone_cow_range(uint32_t* parent, uint32_t* child, uint32_t start, uint32_t end) {
    if (!parent || !child) {
        return 0;
    }
    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t limit = align_up(end, VM_PAGE_SIZE);
    for (uint32_t addr = base; addr < limit; addr += VM_PAGE_SIZE) {
        uint32_t entry = get_page_entry_dir(parent, addr);
        if (!(entry & PAGE_PRESENT)) {
            continue;
        }
        if (entry & PAGE_PS) {
            child[addr >> 22] = entry & ~PAGE_RW;
            parent[addr >> 22] = entry & ~PAGE_RW;
            continue;
        }
        uint32_t phys = entry & 0xFFFFF000;
        uint32_t flags = (entry & 0xFFF) & ~PAGE_RW;
        map_page_dir(child, addr, phys, flags);
        set_page_entry_dir(parent, addr, (entry & ~PAGE_RW));
    }
    return 1;
}

void vm_init_process(process_t* proc) {
    if (!proc) {
        return;
    }
    proc->region_count = 0;
    for (uint32_t i = 0; i < PROCESS_MAX_REGIONS; ++i) {
        proc->regions[i].start = 0;
        proc->regions[i].end = 0;
        proc->regions[i].flags = 0;
        proc->regions[i].shared_id = 0;
    }
}

static uint32_t vm_page_flags(uint32_t flags) {
    uint32_t pf = PAGE_PRESENT;
    if (flags & VM_WRITE) {
        pf |= PAGE_RW;
    }
    if (flags & VM_USER) {
        pf |= PAGE_USER;
    }
    return pf;
}

int vm_map_region(process_t* proc, uint32_t start, uint32_t size, uint32_t flags) {
    if (!proc || size == 0) {
        return 0;
    }
    if (proc->region_count >= PROCESS_MAX_REGIONS) {
        return 0;
    }
    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t end = align_up(start + size, VM_PAGE_SIZE);
    if (end <= base) {
        return 0;
    }
    if (flags & VM_USER) {
        if (base < VM_USER_BASE || end > VM_USER_LIMIT) {
            return 0;
        }
    }
    vm_region_t* region = &proc->regions[proc->region_count++];
    region->start = base;
    region->end = end;
    region->flags = flags;
    region->shared_id = 0;
    if (flags & VM_DEMAND) {
        return 1;
    }
    if (flags & VM_GUARD) {
        return 1;
    }
    uint32_t page_flags = vm_page_flags(flags);
    uint32_t* dir = proc->page_directory ? proc->page_directory : page_directory;
    for (uint32_t addr = base; addr < end; addr += VM_PAGE_SIZE) {
        uint32_t phys = pmm_alloc_block();
        if (!phys) {
            return 0;
        }
        map_page_dir(dir, addr, phys, page_flags);
    }
    return 1;
}

int vm_unmap_region(process_t* proc, uint32_t start, uint32_t size) {
    if (!proc || size == 0) {
        return 0;
    }
    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t end = align_up(start + size, VM_PAGE_SIZE);
    if (end <= base) {
        return 0;
    }
    for (uint32_t i = 0; i < proc->region_count; ++i) {
        vm_region_t* region = &proc->regions[i];
        if (region->start == base && region->end == end) {
            uint32_t flags = region->flags;
            if (!(flags & VM_GUARD) && !(flags & VM_DEMAND) && !(flags & VM_SHARED)) {
                uint32_t* dir = proc->page_directory ? proc->page_directory : page_directory;
                for (uint32_t addr = base; addr < end; addr += VM_PAGE_SIZE) {
                    uint32_t* table = get_table_dir(dir, addr);
                    if (!table) {
                        continue;
                    }
                    uint32_t pt_index = (addr >> 12) & 0x3FF;
                    uint32_t entry = table[pt_index];
                    if (entry & PAGE_PRESENT) {
                        uint32_t phys = entry & 0xFFFFF000;
                        pmm_free_block((void*)phys);
                    }
                    table[pt_index] = 0;
                }
            }
            proc->regions[i] = proc->regions[proc->region_count - 1];
            proc->region_count--;
            return 1;
        }
    }
    return 0;
}

int vm_create_shared(uint32_t pages) {
    if (pages == 0 || pages > SHARED_MAX_PAGES) {
        return -1;
    }
    for (uint32_t i = 0; i < SHARED_MAX; ++i) {
        if (shared_counts[i] == 0) {
            for (uint32_t p = 0; p < pages; ++p) {
                uint32_t phys = pmm_alloc_block();
                if (!phys) {
                    for (uint32_t j = 0; j < p; ++j) {
                        pmm_free_block((void*)shared_pages[i][j]);
                        shared_pages[i][j] = 0;
                    }
                    shared_counts[i] = 0;
                    shared_refs[i] = 0;
                    return -1;
                }
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
    if (!proc || shared_id >= SHARED_MAX) {
        return 0;
    }
    if (shared_counts[shared_id] == 0) {
        return 0;
    }
    uint32_t pages = shared_counts[shared_id];
    uint32_t base = align_down(start, VM_PAGE_SIZE);
    uint32_t end = base + pages * VM_PAGE_SIZE;
    if (proc->region_count >= PROCESS_MAX_REGIONS) {
        return 0;
    }
    vm_region_t* region = &proc->regions[proc->region_count++];
    region->start = base;
    region->end = end;
    region->flags = VM_READ | VM_WRITE | VM_SHARED;
    region->shared_id = shared_id;
    uint32_t page_flags = vm_page_flags(region->flags);
    uint32_t* dir = proc->page_directory ? proc->page_directory : page_directory;
    for (uint32_t p = 0; p < pages; ++p) {
        map_page_dir(dir, base + p * VM_PAGE_SIZE, shared_pages[shared_id][p], page_flags);
    }
    shared_refs[shared_id]++;
    return 1;
}

static int vm_region_contains(const vm_region_t* region, uint32_t addr) {
    return addr >= region->start && addr < region->end;
}

int vm_handle_page_fault(process_t* proc, uint32_t addr, uint32_t err_code) {
    if (!proc) {
        return 0;
    }
    uint32_t* dir = proc->page_directory ? proc->page_directory : page_directory;
    for (uint32_t i = 0; i < proc->region_count; ++i) {
        vm_region_t* region = &proc->regions[i];
        if (!vm_region_contains(region, addr)) {
            continue;
        }
        if (region->flags & VM_GUARD) {
            return 0;
        }
        if (region->flags & VM_SHARED) {
            uint32_t page_index = (addr - region->start) / VM_PAGE_SIZE;
            if (region->shared_id >= SHARED_MAX) {
                return 0;
            }
            if (page_index >= shared_counts[region->shared_id]) {
                return 0;
            }
            map_page_dir(dir, align_down(addr, VM_PAGE_SIZE), shared_pages[region->shared_id][page_index], vm_page_flags(region->flags));
            return 1;
        }
        if ((region->flags & VM_DEMAND) || (err_code & 0x1u) == 0) {
            uint32_t phys = pmm_alloc_block();
            if (!phys) {
                return 0;
            }
            uint32_t vaddr = align_down(addr, VM_PAGE_SIZE);
            map_page_dir(dir, vaddr, phys, vm_page_flags(region->flags));
            return 1;
        }
        if ((region->flags & VM_COW) && (err_code & 0x2u)) {
            uint32_t vaddr = align_down(addr, VM_PAGE_SIZE);
            uint32_t old_entry = get_page_entry_dir(dir, vaddr);
            if (!(old_entry & PAGE_PRESENT)) {
                return 0;
            }
            uint32_t old_phys = old_entry & 0xFFFFF000;
            uint32_t new_phys = pmm_alloc_block();
            if (!new_phys) {
                return 0;
            }
            uint32_t temp_src = 0xFFC00000u;
            uint32_t temp_dst = 0xFFC01000u;
            map_page_dir(dir, temp_src, old_phys, PAGE_RW);
            map_page_dir(dir, temp_dst, new_phys, PAGE_RW);
            load_cr3(dir);
            memcpy((void*)temp_dst, (void*)temp_src, VM_PAGE_SIZE);
            unmap_page_dir(dir, temp_src);
            unmap_page_dir(dir, temp_dst);
            load_cr3(dir);
            map_page_dir(dir, vaddr, new_phys, vm_page_flags(region->flags | VM_WRITE));
            return 1;
        }
        return 0;
    }
    return 0;
}
