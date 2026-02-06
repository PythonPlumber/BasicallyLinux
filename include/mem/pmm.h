#ifndef PMM_H
#define PMM_H

#include "types.h"

void pmm_init(uint32_t start_addr, uint32_t max_size);
void pmm_add_region(uint32_t addr, uint32_t size);
void pmm_reserve_region(uint32_t addr, uint32_t size);
uint32_t pmm_alloc_block(void);
void pmm_free_block(uint32_t addr);
uint32_t pmm_total_blocks(void);
uint32_t pmm_used_blocks(void);
uint32_t pmm_block_size(void);
uint32_t get_ram_size(void);
uint32_t mem_usage_pct(void);

#endif
