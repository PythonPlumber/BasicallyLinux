#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

enum {
    SYS_WRITE = 0,
    SYS_PUTCHAR = 1,
    SYS_TICKS = 2,
    SYS_YIELD = 3,
    SYS_GETPID = 4,
    SYS_SLEEP = 5,
    SYS_SETPRIO = 6,
    SYS_GETPRIO = 7,
    SYS_SETSLICE = 8,
    SYS_OPEN = 9,
    SYS_CLOSE = 10,
    SYS_READ = 11,
    SYS_FDWRITE = 12,
    SYS_FORK = 13,
    SYS_EXEC = 14,
    SYS_EXIT = 15,
    SYS_WAIT = 16,
    SYS_SETCLASS = 17,
    SYS_SETAFFINITY = 18,
    SYS_SETCGROUP = 19,
    SYS_SETRT = 20,
    SYS_SETDEADLINE = 21,
    SYS_GETUID = 22,
    SYS_SETUID = 23,
    SYS_GETGID = 24,
    SYS_SETGID = 25,
    SYS_SETSECID = 26,
    SYS_SETCAPS = 27,
    SYS_POWER_STATE = 28,
    SYS_TRACE_EMIT = 29,
    SYS_TRACE_GET = 30,
    SYS_AUDIT_GET = 31,
    SYS_POLICY_ADD = 32,
    SYS_EXEC_ELF = 33,
    SYS_MAX = 34
};

#define OS_OK 0u
#define OS_ERR 0xFFFFFFFFu

void syscall_init(void);

#endif

