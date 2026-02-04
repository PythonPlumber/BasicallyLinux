#ifndef SMP_RALLY_H
#define SMP_RALLY_H

#include "types.h"

void smp_rally_init(void);
uint32_t smp_rally_cpu_count(void);
int smp_rally_boot(uint32_t cpu_id);
uint32_t smp_rally_online_mask(void);
int smp_rally_add_cpu(uint32_t cpu_id);
int smp_rally_remove_cpu(uint32_t cpu_id);

#endif
