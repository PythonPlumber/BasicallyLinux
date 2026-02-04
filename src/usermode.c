#include "usermode.h"

#define USER_CODE_SEL 0x1B
#define USER_DATA_SEL 0x23

void switch_to_user_mode(uint32_t entry_point, uint32_t user_stack) {
    uint16_t data_sel = USER_DATA_SEL;
    uint16_t code_sel = USER_CODE_SEL;

    asm volatile(
        "cli\n"
        "mov %0, %%ds\n"
        "mov %0, %%es\n"
        "mov %0, %%fs\n"
        "mov %0, %%gs\n"
        "pushl %1\n" // ss
        "pushl %2\n" // esp
        "pushfl\n"
        "popl %%eax\n"
        "or $0x200, %%eax\n"
        "pushl %%eax\n" // eflags
        "pushl %3\n" // cs
        "pushl %4\n" // eip
        "iret\n"
        :
        : "r"(data_sel), "r"((uint32_t)data_sel), "r"(user_stack), "r"((uint32_t)code_sel), "r"(entry_point)
        : "eax", "memory"
    );
}
