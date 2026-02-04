#include "acpi_power.h"
#include "framebuffer.h"
#include "idt.h"
#include "power_plane.h"
#include "sched.h"
#include "secure_audit.h"
#include "secure_caps.h"
#include "secure_policy.h"
#include "syscall.h"
#include "timer.h"
#include "trace_forge.h"
#include "types.h"
#include "vfs.h"

#define SYS_COPY_LIMIT 4096u

static int fd_alloc(process_t* proc, fs_node_t* node) {
    for (uint32_t i = 0; i < PROCESS_MAX_FDS; ++i) {
        if (proc->fds[i].flags == 0) {
            proc->fds[i].node = node;
            proc->fds[i].offset = 0;
            proc->fds[i].flags = 1;
            vfs_open_node(node);
            return (int)i;
        }
    }
    return -1;
}

static file_desc_t* fd_get(process_t* proc, uint32_t fd) {
    if (fd >= PROCESS_MAX_FDS) {
        return 0;
    }
    if (proc->fds[fd].flags == 0) {
        return 0;
    }
    return &proc->fds[fd];
}

static uint32_t fd_close(process_t* proc, uint32_t fd) {
    file_desc_t* desc = fd_get(proc, fd);
    if (!desc) {
        return 0;
    }
    if (desc->node) {
        vfs_close_node(desc->node);
    }
    desc->node = 0;
    desc->offset = 0;
    desc->flags = 0;
    return 1;
}

static registers_t* sys_write(registers_t* regs) {
    const char* buf = (const char*)regs->ebx;
    uint32_t len = regs->ecx;
    if (!buf || len == 0) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (len > SYS_COPY_LIMIT) {
        len = SYS_COPY_LIMIT;
    }
    fb_console_write_len(buf, len);
    regs->eax = len;
    return regs;
}

static registers_t* sys_putchar(registers_t* regs) {
    char ch = (char)regs->ebx;
    fb_console_putc(ch);
    regs->eax = 1;
    return regs;
}

static registers_t* sys_ticks(registers_t* regs) {
    uint64_t ticks = timer_get_ticks();
    regs->eax = (uint32_t)ticks;
    regs->edx = (uint32_t)(ticks >> 32);
    return regs;
}

static registers_t* sys_yield(registers_t* regs) {
    scheduler_yield();
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_getpid(registers_t* regs) {
    process_t* current = scheduler_current();
    regs->eax = current ? current->pid : OS_ERR;
    return regs;
}

static registers_t* sys_sleep(registers_t* regs) {
    scheduler_sleep((uint64_t)regs->ebx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setprio(registers_t* regs) {
    scheduler_set_priority(regs->ebx, regs->ecx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_getprio(registers_t* regs) {
    process_t* current = scheduler_current();
    regs->eax = current ? current->priority : OS_ERR;
    return regs;
}

static registers_t* sys_setslice(registers_t* regs) {
    scheduler_set_timeslice(regs->ebx, regs->ecx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_open(registers_t* regs) {
    process_t* current = scheduler_current();
    const char* path = (const char*)regs->ebx;
    if (!current || !path) {
        regs->eax = OS_ERR;
        return regs;
    }
    fs_node_t* node = vfs_find(path);
    if (!node) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (!secure_policy_check(current->security_id, node->inode_id, SECURE_ACTION_READ)) {
        secure_audit_log(SECURE_ACTION_READ);
        regs->eax = OS_ERR;
        return regs;
    }
    int fd = fd_alloc(current, node);
    regs->eax = (fd < 0) ? OS_ERR : (uint32_t)fd;
    return regs;
}

static registers_t* sys_close(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    regs->eax = fd_close(current, regs->ebx) ? OS_OK : OS_ERR;
    return regs;
}

static registers_t* sys_read(registers_t* regs) {
    process_t* current = scheduler_current();
    uint32_t fd = regs->ebx;
    uint8_t* buffer = (uint8_t*)regs->ecx;
    uint32_t size = regs->edx;
    if (!current || !buffer || size == 0) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (size > SYS_COPY_LIMIT) {
        size = SYS_COPY_LIMIT;
    }
    file_desc_t* desc = fd_get(current, fd);
    if (!desc || !desc->node) {
        regs->eax = OS_ERR;
        return regs;
    }
    uint32_t read = vfs_read(desc->node, desc->offset, size, buffer);
    desc->offset += read;
    regs->eax = read;
    return regs;
}

static registers_t* sys_fdwrite(registers_t* regs) {
    process_t* current = scheduler_current();
    uint32_t fd = regs->ebx;
    const uint8_t* buffer = (const uint8_t*)regs->ecx;
    uint32_t size = regs->edx;
    if (!current || !buffer || size == 0) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (size > SYS_COPY_LIMIT) {
        size = SYS_COPY_LIMIT;
    }
    file_desc_t* desc = fd_get(current, fd);
    if (!desc || !desc->node) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (!secure_policy_check(current->security_id, desc->node->inode_id, SECURE_ACTION_WRITE)) {
        secure_audit_log(SECURE_ACTION_WRITE);
        regs->eax = OS_ERR;
        return regs;
    }
    uint32_t wrote = vfs_write(desc->node, desc->offset, size, buffer);
    desc->offset += wrote;
    regs->eax = wrote;
    return regs;
}

static registers_t* sys_fork(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    process_t* child = process_fork(current, regs);
    regs->eax = child ? child->pid : OS_ERR;
    return regs;
}

static registers_t* sys_exec(registers_t* regs) {
    process_t* current = scheduler_current();
    void (*entry)(void) = (void (*)(void))regs->ebx;
    registers_t* new_stack = process_exec(current, entry);
    if (new_stack) {
        return new_stack;
    }
    regs->eax = OS_ERR;
    return regs;
}

static registers_t* sys_exec_elf(registers_t* regs) {
    process_t* current = scheduler_current();
    const uint8_t* user_elf = (const uint8_t*)regs->ebx;
    uint32_t size = regs->ecx;
    
    if (!current || !user_elf || size == 0) {
        regs->eax = OS_ERR;
        return regs;
    }
    
    if (size > 16 * 1024 * 1024) {
        regs->eax = OS_ERR;
        return regs;
    }
    
    uint8_t* kernel_elf = (uint8_t*)kmalloc(size);
    if (!kernel_elf) {
        regs->eax = OS_ERR;
        return regs;
    }
    
    memcpy(kernel_elf, user_elf, size);
    
    registers_t* new_stack = process_exec_elf(current, kernel_elf, size);
    
    kfree(kernel_elf);
    
    if (new_stack) {
        return new_stack;
    }
    
    regs->eax = OS_ERR;
    return regs;
}

static registers_t* sys_exit(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    process_exit(current, regs->ebx);
    scheduler_yield();
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_wait(registers_t* regs) {
    uint32_t pid = regs->ebx;
    uint32_t* out = (uint32_t*)regs->ecx;
    regs->eax = process_wait(pid, out) ? OS_OK : OS_ERR;
    return regs;
}

static registers_t* sys_setclass(registers_t* regs) {
    scheduler_set_class(regs->ebx, regs->ecx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setaffinity(registers_t* regs) {
    scheduler_set_affinity(regs->ebx, regs->ecx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setcgroup(registers_t* regs) {
    scheduler_set_cgroup(regs->ebx, regs->ecx, regs->edx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setrt(registers_t* regs) {
    scheduler_set_rt(regs->ebx, regs->ecx, regs->edx, regs->esi);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setdeadline(registers_t* regs) {
    scheduler_set_deadline(regs->ebx, regs->ecx, regs->edx, regs->esi);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_getuid(registers_t* regs) {
    process_t* current = scheduler_current();
    regs->eax = current ? current->uid : OS_ERR;
    return regs;
}

static registers_t* sys_setuid(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (!secure_caps_has(current->caps, SECURE_CAP_SYS_ADMIN)) {
        secure_audit_log(SECURE_ACTION_ADMIN);
        regs->eax = OS_ERR;
        return regs;
    }
    current->uid = regs->ebx;
    current->euid = regs->ebx;
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_getgid(registers_t* regs) {
    process_t* current = scheduler_current();
    regs->eax = current ? current->gid : OS_ERR;
    return regs;
}

static registers_t* sys_setgid(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (!secure_caps_has(current->caps, SECURE_CAP_SYS_ADMIN)) {
        secure_audit_log(SECURE_ACTION_ADMIN);
        regs->eax = OS_ERR;
        return regs;
    }
    current->gid = regs->ebx;
    current->egid = regs->ebx;
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setsecid(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (!secure_caps_has(current->caps, SECURE_CAP_SYS_ADMIN)) {
        secure_audit_log(SECURE_ACTION_ADMIN);
        regs->eax = OS_ERR;
        return regs;
    }
    current->security_id = regs->ebx;
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_setcaps(registers_t* regs) {
    process_t* current = scheduler_current();
    if (!current) {
        regs->eax = OS_ERR;
        return regs;
    }
    if (!secure_caps_has(current->caps, SECURE_CAP_SYS_ADMIN)) {
        secure_audit_log(SECURE_ACTION_ADMIN);
        regs->eax = OS_ERR;
        return regs;
    }
    current->caps = secure_caps_from_mask(regs->ebx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_power_state(registers_t* regs) {
    if (regs->ebx & 1u) {
        acpi_power_request_sleep(regs->ecx);
    } else {
        power_plane_set_state(regs->ecx);
    }
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_trace_emit(registers_t* regs) {
    process_t* current = scheduler_current();
    if (current && !secure_caps_has(current->caps, SECURE_CAP_TRACE)) {
        regs->eax = OS_ERR;
        return regs;
    }
    trace_forge_emit(regs->ebx, regs->ecx);
    regs->eax = OS_OK;
    return regs;
}

static registers_t* sys_trace_get(registers_t* regs) {
    trace_event_t* out = (trace_event_t*)regs->ecx;
    regs->eax = trace_forge_get(regs->ebx, out) ? OS_OK : OS_ERR;
    return regs;
}

static registers_t* sys_audit_get(registers_t* regs) {
    secure_audit_entry_t* out = (secure_audit_entry_t*)regs->ecx;
    regs->eax = secure_audit_get(regs->ebx, out) ? OS_OK : OS_ERR;
    return regs;
}

static registers_t* sys_policy_add(registers_t* regs) {
    process_t* current = scheduler_current();
    if (current && !secure_caps_has(current->caps, SECURE_CAP_SYS_ADMIN)) {
        secure_audit_log(SECURE_ACTION_ADMIN);
        regs->eax = OS_ERR;
        return regs;
    }
    regs->eax = secure_policy_add_rule(regs->ebx, regs->ecx, regs->edx, regs->esi) ? OS_OK : OS_ERR;
    return regs;
}

typedef registers_t* (*syscall_fn_t)(registers_t* regs);

static syscall_fn_t syscall_table[SYS_MAX] = {
    sys_write,
    sys_putchar,
    sys_ticks,
    sys_yield,
    sys_getpid,
    sys_sleep,
    sys_setprio,
    sys_getprio,
    sys_setslice,
    sys_open,
    sys_close,
    sys_read,
    sys_fdwrite,
    sys_fork,
    sys_exec,
    sys_exit,
    sys_wait,
    sys_setclass,
    sys_setaffinity,
    sys_setcgroup,
    sys_setrt,
    sys_setdeadline,
    sys_getuid,
    sys_setuid,
    sys_getgid,
    sys_setgid,
    sys_setsecid,
    sys_setcaps,
    sys_power_state,
    sys_trace_emit,
    sys_trace_get,
    sys_audit_get,
    sys_policy_add,
    sys_exec_elf
};

static registers_t* syscall_handler(registers_t* regs) {
    if (regs->eax >= SYS_MAX) {
        regs->eax = OS_ERR;
        return regs;
    }
    syscall_fn_t fn = syscall_table[regs->eax];
    if (!fn) {
        regs->eax = OS_ERR;
        return regs;
    }
    return fn(regs);
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
}
