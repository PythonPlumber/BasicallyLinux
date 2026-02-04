#include "types.h"
#include "idt.h"

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi;
    uint32_t esp, ebp, eip, eflags;
} task_context_t;

typedef struct task {
    task_context_t context;
    uint32_t id;
    uint32_t active;
} task_t;

void tasking_init(void);
registers_t* scheduler(registers_t* regs);
int task_create(void (*entry)(void));
void switch_to(task_t* next);
extern task_t* current_task_ptr;
