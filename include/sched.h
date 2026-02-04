#ifndef SCHED_H
#define SCHED_H

#include "idt.h"
#include "process.h"

void scheduler_init(void);
process_t* process_create(void (*entry)(void), uint32_t* page_directory);
process_t* switch_task(registers_t* saved_stack, process_t** previous);
process_t* scheduler_current(void);
void context_switch(process_t* current, process_t* next, registers_t* saved_stack);
process_t* scheduler_process_list(void);
uint32_t scheduler_process_count(void);
int scheduler_kill(uint32_t pid);
void scheduler_set_priority(uint32_t pid, uint32_t priority);
void scheduler_set_timeslice(uint32_t pid, uint32_t ticks);
void scheduler_set_class(uint32_t pid, uint32_t sched_class);
void scheduler_set_affinity(uint32_t pid, uint32_t cpu_mask);
void scheduler_set_cgroup(uint32_t pid, uint32_t cgroup_id, uint32_t share);
void scheduler_set_rt(uint32_t pid, uint32_t priority, uint64_t budget, uint64_t period);
void scheduler_set_deadline(uint32_t pid, uint64_t budget, uint64_t period, uint64_t deadline);
void scheduler_sleep(uint64_t ticks);
int scheduler_wake(uint32_t pid);
void scheduler_yield(void);
void scheduler_balance_load(void);
process_t* process_fork(process_t* parent, registers_t* regs);
registers_t* process_exec(process_t* proc, void (*entry)(void));
registers_t* process_exec_elf(process_t* proc, const uint8_t* elf_data, uint32_t size);
void process_exit(process_t* proc, uint32_t code);
int process_wait(uint32_t pid, uint32_t* exit_code);

void wait_queue_init(wait_queue_t* queue);
void wait_queue_wait(wait_queue_t* queue);
process_t* wait_queue_wake_one(wait_queue_t* queue);
uint32_t wait_queue_wake_all(wait_queue_t* queue);

#endif
