#ifndef SMP_RALLY_H
#define SMP_RALLY_H

#include "types.h"

#define LAPIC_BASE      0xFEE00000
#define LAPIC_ID        0x0020
#define LAPIC_VER       0x0030
#define LAPIC_TPR       0x0080
#define LAPIC_EOI       0x00B0
#define LAPIC_SVR       0x00F0
#define LAPIC_ICR_LOW   0x0300
#define LAPIC_ICR_HIGH  0x0310
#define LAPIC_LVT_TMR   0x0320
#define LAPIC_TMRINIT   0x0380
#define LAPIC_TMRCURR   0x0390
#define LAPIC_TMRDIV    0x03E0

void smp_rally_init(void);
uint32_t smp_rally_cpu_count(void);
int smp_rally_boot(uint32_t cpu_id);
uint32_t smp_rally_online_mask(void);
int smp_rally_add_cpu(uint32_t cpu_id);
int smp_rally_remove_cpu(uint32_t cpu_id);

#endif
