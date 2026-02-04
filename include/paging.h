#include "process.h"
#include "types.h"

void paging_init(void);
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void map_page_4mb(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
uint32_t* paging_get_directory(void);
void vmm_init(void);
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void vmm_unmap_page(uint32_t virtual_addr);
void map_page_dir(uint32_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void unmap_page_dir(uint32_t* dir, uint32_t virtual_addr);
uint32_t get_page_entry_dir(uint32_t* dir, uint32_t virtual_addr);
void set_page_entry_dir(uint32_t* dir, uint32_t virtual_addr, uint32_t entry);
uint32_t* paging_create_directory(void);
void paging_destroy_directory(uint32_t* dir);
int paging_clone_cow_range(uint32_t* parent, uint32_t* child, uint32_t start, uint32_t end);
void load_cr3(uint32_t* dir);
void paging_enable(void);
uint32_t get_page_fault_addr(void);
int is_addr_valid(uint32_t addr);

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

void vm_init_process(process_t* proc);
int vm_map_region(process_t* proc, uint32_t start, uint32_t size, uint32_t flags);
int vm_unmap_region(process_t* proc, uint32_t start, uint32_t size);
int vm_handle_page_fault(process_t* proc, uint32_t addr, uint32_t err_code);
int vm_create_shared(uint32_t pages);
int vm_map_shared(process_t* proc, uint32_t shared_id, uint32_t start);
