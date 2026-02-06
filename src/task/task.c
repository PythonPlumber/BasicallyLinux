#include "task/task.h"
#include "types.h"

#define MAX_TASKS 8
#define STACK_SIZE 4096

static task_t tasks[MAX_TASKS];
static uint8_t stacks[MAX_TASKS][STACK_SIZE] __attribute__((aligned(16)));
static uint32_t task_count = 0;
static uint32_t current_task = 0;
task_t* current_task_ptr = 0;

static void init_task_context(task_t* task, void (*entry)(void), uint32_t stack_top) {
    registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
    frame->ds = 0x10;
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = stack_top;
    frame->esp = stack_top;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = (uint32_t)entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->useresp = 0;
    frame->ss = 0;
    task->context.eax = 0;
    task->context.ebx = 0;
    task->context.ecx = 0;
    task->context.edx = 0;
    task->context.esi = 0;
    task->context.edi = 0;
    task->context.esp = (uint32_t)frame;
    task->context.ebp = stack_top;
    task->context.eip = (uint32_t)entry;
    task->context.eflags = 0x202;
    task->active = 1;
}

void tasking_init(void) {
    task_count = 1;
    current_task = 0;
    tasks[0].id = 0;
    tasks[0].active = 1;
    current_task_ptr = &tasks[0];
}

int task_create(void (*entry)(void)) {
    if (task_count >= MAX_TASKS) {
        return -1;
    }
    uint32_t id = task_count;
    tasks[id].id = id;
    uint32_t stack_top = (uint32_t)&stacks[id][STACK_SIZE];
    init_task_context(&tasks[id], entry, stack_top);
    task_count++;
    return (int)id;
}

registers_t* scheduler(registers_t* regs) {
    if (task_count <= 1) {
        return regs;
    }
    task_t* current = &tasks[current_task];
    current->context.eax = regs->eax;
    current->context.ebx = regs->ebx;
    current->context.ecx = regs->ecx;
    current->context.edx = regs->edx;
    current->context.esi = regs->esi;
    current->context.edi = regs->edi;
    current->context.esp = (uint32_t)regs;
    current->context.ebp = regs->ebp;
    current->context.eip = regs->eip;
    current->context.eflags = regs->eflags;

    uint32_t next = (current_task + 1) % task_count;
    current_task = next;
    task_t* next_task = &tasks[current_task];
    current_task_ptr = next_task;

    if (next_task->context.esp == 0) {
        return regs;
    }
    return (registers_t*)next_task->context.esp;
}
