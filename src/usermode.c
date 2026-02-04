#include "usermode.h"

#define USER_CODE_SEL 0x1B
#define USER_DATA_SEL 0x23

void switch_to_user_mode(uint32_t entry_point, uint32_t user_stack) {
    asm volatile(
        "cli\n"
        "mov %0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushl %0\n"
        "pushl %1\n"
        "pushfl\n"
        "popl %%eax\n"
        "or $0x200, %%eax\n"
        "pushl %%eax\n"
        "pushl %2\n"
        "pushl %3\n"
        "iret\n"
        :
        : "r"(USER_DATA_SEL), "r"(user_stack), "r"(USER_CODE_SEL), "r"(entry_point)
        : "eax"
    );
}
