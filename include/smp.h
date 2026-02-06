#ifndef SMP_H
#define SMP_H

#include "types.h"

// Initialize SMP
void smp_init(void);

// Get number of online CPUs
uint32_t smp_get_cpu_count(void);

// Get mask of online CPUs
uint32_t smp_get_online_mask(void);

// Send an IPI (Inter-Processor Interrupt)
void smp_send_ipi(uint32_t target_cpu, uint32_t vector);

// Send an IPI to all other CPUs
void smp_broadcast_ipi(uint32_t vector);

#endif
