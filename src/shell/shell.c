#include "ai_model.h"
#include "bench.h"
#include "diag.h"
#include "debug.h"
#include "framebuffer.h"
#include "heap.h"
#include "keyboard.h"
#include "kernel.h"
#include "pmm.h"
#include "paging.h"
#include "sched.h"
#include "selftest.h"
#include "shell.h"
#include "cpu.h"
#include "syscall.h"
#include "types.h"
#include "util.h"
#include "vfs.h"

#define SHELL_LINE_SIZE 128
#define SHELL_MAX_ARGS 16
#define SHELL_HISTORY 10
#define SHELL_TICKS_PER_SEC 100

static char line_buffer[SHELL_LINE_SIZE];
static uint32_t line_length = 0;
static uint32_t cursor_pos = 0;
static char history[SHELL_HISTORY][SHELL_LINE_SIZE];
static uint32_t history_count = 0;
static uint32_t history_head = 0;
static uint32_t history_index = 0;

static char cmd_help_name[] = "help";
static char cmd_echo_name[] = "echo";
static char cmd_clear_name[] = "clear";
static char cmd_fontvec_name[] = "fontvec";
static char cmd_fontbmp_name[] = "fontbmp";
static char cmd_ticks_name[] = "ticks";
static char cmd_uptime_name[] = "uptime";
static char cmd_info_name[] = "info";
static char cmd_free_name[] = "free";
static char cmd_mmap_name[] = "mmap";
static char cmd_peek_name[] = "peek";
static char cmd_ps_name[] = "ps";
static char cmd_kill_name[] = "kill";
static char cmd_prio_name[] = "prio";
static char cmd_slice_name[] = "slice";
static char cmd_sleep_name[] = "sleep";
static char cmd_fdcat_name[] = "fdcat";
static char cmd_brain_stats_name[] = "brain-stats";
static char cmd_debug_ai_name[] = "debug-ai";
static char cmd_log_name[] = "log";
static char cmd_selftest_name[] = "selftest";
static char cmd_bench_name[] = "bench";
static char cmd_ls_name[] = "ls";
static char cmd_cat_name[] = "cat";
static char cmd_panic_name[] = "panic";
static char cmd_syscall_name[] = "syscall";
static char cmd_mem_name[] = "mem";
static char cmd_procs_name[] = "procs";
static char cmd_bt_name[] = "bt";
static char cmd_ask_name[] = "ask";
static char cmd_aiinfo_name[] = "aiinfo";
static char cmd_infer_name[] = "infer";
static char cmd_reboot_name[] = "reboot";
static char cmd_lspci_name[] = "lspci";
static char cmd_rtc_name[] = "rtc";
static char cmd_diag_name[] = "diag";
static char cmd_acpi_name[] = "acpi";
static char cmd_cpuinfo_name[] = "cpuinfo";

static void shell_redraw_line(void);
static void shell_putc(char ch);
static void shell_write(const char* str);
static void shell_prompt(void);
static void shell_execute_line(void);
static int shell_is_space(char ch);
static void shell_autocomplete(void);

static void cmd_help(int argc, char** argv);
static void cmd_echo(int argc, char** argv);
static void cmd_clear(int argc, char** argv);
static void cmd_fontvec(int argc, char** argv);
static void cmd_fontbmp(int argc, char** argv);
static void cmd_ticks(int argc, char** argv);
static void cmd_uptime(int argc, char** argv);
static void cmd_info(int argc, char** argv);
static void cmd_free(int argc, char** argv);
static void cmd_mmap(int argc, char** argv);
static void cmd_peek(int argc, char** argv);
static void cmd_ps(int argc, char** argv);
static void cmd_kill(int argc, char** argv);
static void cmd_prio(int argc, char** argv);
static void cmd_slice(int argc, char** argv);
static void cmd_sleep(int argc, char** argv);
static void cmd_fdcat(int argc, char** argv);
static void cmd_brain_stats(int argc, char** argv);
static void cmd_debug_ai(int argc, char** argv);
static void cmd_log(int argc, char** argv);
static void cmd_selftest(int argc, char** argv);
static void cmd_bench(int argc, char** argv);
static void cmd_ls(int argc, char** argv);
static void cmd_cat(int argc, char** argv);
static void cmd_panic(int argc, char** argv);
static void cmd_syscall(int argc, char** argv);
static void cmd_mem(int argc, char** argv);
static void cmd_procs(int argc, char** argv);
static void cmd_bt(int argc, char** argv);
static void cmd_ask(int argc, char** argv);
static void cmd_aiinfo(int argc, char** argv);
static void cmd_infer(int argc, char** argv);
static void cmd_reboot(int argc, char** argv);
static void cmd_lspci(int argc, char** argv);
static void cmd_rtc(int argc, char** argv);
static void cmd_diag(int argc, char** argv);
static void cmd_acpi(int argc, char** argv);
static void cmd_cpuinfo(int argc, char** argv);

static command_t commands[] = {
    { cmd_help_name, cmd_help },
    { cmd_echo_name, cmd_echo },
    { cmd_clear_name, cmd_clear },
    { cmd_fontvec_name, cmd_fontvec },
    { cmd_fontbmp_name, cmd_fontbmp },
    { cmd_ticks_name, cmd_ticks },
    { cmd_uptime_name, cmd_uptime },
    { cmd_info_name, cmd_info },
    { cmd_free_name, cmd_free },
    { cmd_mmap_name, cmd_mmap },
    { cmd_peek_name, cmd_peek },
    { cmd_ps_name, cmd_ps },
    { cmd_kill_name, cmd_kill },
    { cmd_prio_name, cmd_prio },
    { cmd_slice_name, cmd_slice },
    { cmd_sleep_name, cmd_sleep },
    { cmd_fdcat_name, cmd_fdcat },
    { cmd_ls_name, cmd_ls },
    { cmd_cat_name, cmd_cat },
    { cmd_panic_name, cmd_panic },
    { cmd_syscall_name, cmd_syscall },
    { cmd_mem_name, cmd_mem },
    { cmd_procs_name, cmd_procs },
    { cmd_bt_name, cmd_bt },
    { cmd_ask_name, cmd_ask },
    { cmd_aiinfo_name, cmd_aiinfo },
    { cmd_infer_name, cmd_infer },
    { cmd_brain_stats_name, cmd_brain_stats },
    { cmd_debug_ai_name, cmd_debug_ai },
    { cmd_log_name, cmd_log },
    { cmd_selftest_name, cmd_selftest },
    { cmd_bench_name, cmd_bench },
    { cmd_reboot_name, cmd_reboot },
    { cmd_lspci_name, cmd_lspci },
    { cmd_rtc_name, cmd_rtc },
    { cmd_diag_name, cmd_diag },
    { cmd_acpi_name, cmd_acpi },
    { cmd_cpuinfo_name, cmd_cpuinfo }
};

static void shell_putc(char ch) {
    fb_console_putc(ch);
}

static void shell_write(const char* str) {
    for (uint32_t i = 0; str[i] != 0; ++i) {
        shell_putc(str[i]);
    }
}

static void shell_prompt(void) {
    shell_write("> ");
}

static void shell_write_uint64(uint64_t value) {
    char buf[24];
    int pos = 0;
    if (value == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[20];
        int tmp_pos = 0;
        while (value > 0 && tmp_pos < 20) {
            tmp[tmp_pos++] = (char)('0' + (value % 10));
            value /= 10;
        }
        for (int i = tmp_pos - 1; i >= 0; --i) {
            buf[pos++] = tmp[i];
        }
    }
    buf[pos] = 0;
    shell_write(buf);
}

static void shell_write_hex32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    shell_write("0x");
    for (int i = 7; i >= 0; --i) {
        uint8_t nibble = (uint8_t)((value >> (i * 4)) & 0xF);
        shell_putc(hex[nibble]);
    }
}

static void shell_write_hex8(uint8_t value) {
    static const char hex[] = "0123456789ABCDEF";
    shell_putc(hex[(value >> 4) & 0xF]);
    shell_putc(hex[value & 0xF]);
}

static void shell_write_two(uint32_t value) {
    char buf[3];
    buf[0] = (char)('0' + ((value / 10) % 10));
    buf[1] = (char)('0' + (value % 10));
    buf[2] = 0;
    shell_write(buf);
}

static uint8_t shell_bcd_to_bin(uint8_t value) {
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static void shell_rtc_wait_ready(void) {
    while (rtc_read(0x0A) & 0x80) { }
}

static void shell_rtc_read_raw(uint8_t* sec, uint8_t* min, uint8_t* hour, uint8_t* day, uint8_t* month, uint8_t* year) {
    uint8_t last_sec;
    uint8_t last_min;
    uint8_t last_hour;
    uint8_t last_day;
    uint8_t last_month;
    uint8_t last_year;
    do {
        shell_rtc_wait_ready();
        last_sec = rtc_read(0x00);
        last_min = rtc_read(0x02);
        last_hour = rtc_read(0x04);
        last_day = rtc_read(0x07);
        last_month = rtc_read(0x08);
        last_year = rtc_read(0x09);
        shell_rtc_wait_ready();
        *sec = rtc_read(0x00);
        *min = rtc_read(0x02);
        *hour = rtc_read(0x04);
        *day = rtc_read(0x07);
        *month = rtc_read(0x08);
        *year = rtc_read(0x09);
    } while (last_sec != *sec || last_min != *min || last_hour != *hour || last_day != *day || last_month != *month || last_year != *year);
}

static const char* shell_pci_class_name(uint8_t class_id, uint8_t subclass) {
    switch (class_id) {
        case 0x01:
            switch (subclass) {
                case 0x00: return "scsi";
                case 0x01: return "ide";
                case 0x02: return "floppy";
                case 0x03: return "ipi";
                case 0x04: return "raid";
                case 0x05: return "ata";
                case 0x06: return "sata";
                case 0x07: return "sas";
                case 0x08: return "nvme";
                default: return "storage";
            }
        case 0x02:
            return "network";
        case 0x03:
            switch (subclass) {
                case 0x00: return "vga";
                case 0x02: return "3d";
                default: return "display";
            }
        case 0x04:
            return "multimedia";
        case 0x05:
            return "memory";
        case 0x06:
            switch (subclass) {
                case 0x00: return "host";
                case 0x01: return "isa";
                case 0x04: return "pci-pci";
                case 0x09: return "pcie";
                default: return "bridge";
            }
        case 0x07:
            return "simple-comm";
        case 0x08:
            return "base-sys";
        case 0x09:
            return "input";
        case 0x0A:
            return "dock";
        case 0x0B:
            return "cpu";
        case 0x0C:
            switch (subclass) {
                case 0x03: return "usb";
                case 0x05: return "smbus";
                default: return "serial";
            }
        case 0x0D:
            return "wireless";
        case 0x0E:
            return "intelligent-io";
        case 0x0F:
            return "satellite";
        case 0x10:
            return "crypto";
        case 0x11:
            return "signal";
        default:
            return "unknown";
    }
}

static void shell_write_flag(const char* name, int set) {
    if (set) {
        shell_write(name);
        shell_write(" ");
    }
}

static void shell_write_sig(const uint8_t* sig) {
    for (uint32_t i = 0; i < 4; ++i) {
        shell_putc((char)sig[i]);
    }
}

static int shell_parse_u32(const char* text, uint32_t* out) {
    if (!text || !out) {
        return 0;
    }
    uint32_t base = 10;
    uint32_t i = 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        i = 2;
    }
    if (!text[i]) {
        return 0;
    }
    uint32_t value = 0;
    for (; text[i]; ++i) {
        char c = text[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = (uint32_t)(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = (uint32_t)(c - 'a' + 10);
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            return 0;
        }
        value = value * base + digit;
    }
    *out = value;
    return 1;
}

static void shell_cpuid(uint32_t leaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf));
    if (a) {
        *a = eax;
    }
    if (b) {
        *b = ebx;
    }
    if (c) {
        *c = ecx;
    }
    if (d) {
        *d = edx;
    }
}

static const char* history_at(uint32_t index) {
    if (index >= history_count) {
        return "";
    }
    uint32_t oldest = (history_head + SHELL_HISTORY - history_count) % SHELL_HISTORY;
    uint32_t slot = (oldest + index) % SHELL_HISTORY;
    return history[slot];
}

static int shell_starts_with(const char* text, const char* prefix) {
    uint32_t i = 0;
    while (prefix[i]) {
        if (text[i] != prefix[i]) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static void shell_autocomplete(void) {
    uint32_t space_pos = 0;
    for (; space_pos < cursor_pos; ++space_pos) {
        if (shell_is_space(line_buffer[space_pos])) {
            return;
        }
    }
    char prefix[SHELL_LINE_SIZE];
    uint32_t len = 0;
    while (len < cursor_pos && len + 1 < SHELL_LINE_SIZE) {
        prefix[len] = line_buffer[len];
        len++;
    }
    prefix[len] = 0;
    uint32_t matches = 0;
    const char* match = 0;
    uint32_t count = sizeof(commands) / sizeof(commands[0]);
    for (uint32_t i = 0; i < count; ++i) {
        if (shell_starts_with(commands[i].name, prefix)) {
            matches++;
            match = commands[i].name;
        }
    }
    if (matches == 0) {
        return;
    }
    if (matches == 1 && match) {
        uint32_t name_len = strlen(match);
        if (name_len + 1 >= SHELL_LINE_SIZE) {
            return;
        }
        for (uint32_t i = 0; i < name_len; ++i) {
            line_buffer[i] = match[i];
        }
        line_buffer[name_len] = ' ';
        line_buffer[name_len + 1] = 0;
        line_length = name_len + 1;
        cursor_pos = line_length;
        shell_redraw_line();
        return;
    }
    shell_putc('\n');
    for (uint32_t i = 0; i < count; ++i) {
        if (shell_starts_with(commands[i].name, prefix)) {
            shell_write(commands[i].name);
            shell_putc('\n');
        }
    }
    shell_prompt();
    shell_redraw_line();
}

static void shell_redraw_line(void) {
    uint32_t back = cursor_pos;
    for (uint32_t i = 0; i < back; ++i) {
        shell_putc('\b');
    }
    for (uint32_t i = 0; i < line_length; ++i) {
        shell_putc(' ');
    }
    for (uint32_t i = 0; i < line_length; ++i) {
        shell_putc('\b');
    }
    for (uint32_t i = 0; i < line_length; ++i) {
        shell_putc(line_buffer[i]);
    }
    uint32_t right = line_length - cursor_pos;
    for (uint32_t i = 0; i < right; ++i) {
        shell_putc('\b');
    }
}

static void shell_set_line(const char* src) {
    uint32_t len = 0;
    while (src[len] && len + 1 < SHELL_LINE_SIZE) {
        line_buffer[len] = src[len];
        len++;
    }
    line_length = len;
    cursor_pos = len;
    shell_redraw_line();
}

static int shell_is_space(char ch) {
    return ch == ' ' || ch == '\t';
}

static int shell_strcmp(const char* a, const char* b) {
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return (int)((uint8_t)a[i]) - (int)((uint8_t)b[i]);
        }
        ++i;
    }
    return (int)((uint8_t)a[i]) - (int)((uint8_t)b[i]);
}

static void shell_execute_line(void) {
    char* argv[SHELL_MAX_ARGS];
    int argc = 0;
    uint32_t i = 0;

    while (line_buffer[i]) {
        while (line_buffer[i] && shell_is_space(line_buffer[i])) {
            line_buffer[i] = 0;
            ++i;
        }
        if (!line_buffer[i]) {
            break;
        }
        if (argc < SHELL_MAX_ARGS) {
            argv[argc++] = &line_buffer[i];
        }
        while (line_buffer[i] && !shell_is_space(line_buffer[i])) {
            ++i;
        }
    }

    if (argc == 0) {
        return;
    }

    uint32_t count = sizeof(commands) / sizeof(commands[0]);
    for (uint32_t c = 0; c < count; ++c) {
        if (shell_strcmp(argv[0], commands[c].name) == 0) {
            commands[c].func(argc, argv);
            return;
        }
    }
    shell_write("Unknown command\n");
}

static void cmd_help(int argc, char** argv) {
    (void)argc;
    (void)argv;
    shell_write("Commands:\n");
    uint32_t count = sizeof(commands) / sizeof(commands[0]);
    for (uint32_t i = 0; i < count; ++i) {
        shell_write("  ");
        shell_write(commands[i].name);
        shell_write("\n");
    }
}

static void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        shell_write(argv[i]);
        if (i + 1 < argc) {
            shell_write(" ");
        }
    }
    shell_write("\n");
}

static void cmd_clear(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fb_clear(0);
    fb_console_init(0xFFFFFF, 0);
}

static void cmd_fontvec(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fb_console_set_mode(1);
}

static void cmd_fontbmp(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fb_console_set_mode(0);
}

static void cmd_ticks(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint64_t ticks = timer_get_ticks();
    shell_write_uint64(ticks);
    shell_putc('\n');
}

static void cmd_uptime(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint64_t ticks = timer_get_ticks();
    uint64_t seconds = ticks / SHELL_TICKS_PER_SEC;
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;
    shell_write_uint64(hours);
    shell_write(":");
    shell_write_two((uint32_t)minutes);
    shell_write(":");
    shell_write_two((uint32_t)secs);
    shell_putc('\n');
}

static void cmd_info(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    shell_cpuid(0, &a, &b, &c, &d);
    char vendor[13];
    ((uint32_t*)vendor)[0] = b;
    ((uint32_t*)vendor)[1] = d;
    ((uint32_t*)vendor)[2] = c;
    vendor[12] = 0;
    shell_write("CPU ");
    shell_write(vendor);
    shell_write("\n");
    shell_write("Kernel v0.0.1\n");
    shell_write("Not-Linux yes\n");
}

static void cmd_free(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint64_t total_blocks = pmm_total_blocks();
    uint64_t used_blocks = pmm_used_blocks();
    uint64_t block_size = pmm_block_size();
    uint64_t total = total_blocks * block_size;
    uint64_t used = used_blocks * block_size;
    uint64_t free = total > used ? total - used : 0;
    shell_write("total ");
    shell_write_uint64(total);
    shell_write("\n");
    shell_write("used ");
    shell_write_uint64(used);
    shell_write("\n");
    shell_write("free ");
    shell_write_uint64(free);
    shell_write("\n");
}

static void cmd_mmap(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t* dir = paging_get_directory();
    for (uint32_t i = 0; i < 1024; ++i) {
        uint32_t entry = dir[i];
        if (entry & 0x1) {
            shell_write("pd[");
            shell_write_uint64(i);
            shell_write("]=");
            shell_write_hex32(entry);
            shell_write("\n");
        }
    }
}

static void cmd_peek(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: peek <addr>\n");
        return;
    }
    uint32_t addr;
    if (!shell_parse_u32(argv[1], &addr)) {
        shell_write("Invalid address\n");
        return;
    }
    volatile uint8_t* ptr = (volatile uint8_t*)addr;
    shell_write_hex32(addr);
    shell_write(": ");
    for (uint32_t i = 0; i < 16; ++i) {
        shell_write_hex8(ptr[i]);
        shell_putc(' ');
    }
    shell_putc('\n');
}

static void cmd_ls(int argc, char** argv) {
    (void)argc;
    (void)argv;
    fs_node_t* root = vfs_root();
    if (!root || !root->impl) {
        shell_write("No filesystem\n");
        return;
    }
    fs_node_t* nodes = (fs_node_t*)root->impl;
    uint32_t count = root->length;
    for (uint32_t i = 0; i < count; ++i) {
        shell_write((char*)nodes[i].name);
        shell_write("\n");
    }
}

static void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: cat <file>\n");
        return;
    }
    fs_node_t* node = vfs_find(argv[1]);
    if (!node) {
        shell_write("Not found\n");
        return;
    }
    uint8_t buffer[64];
    uint32_t offset = 0;
    while (offset < node->length) {
        uint32_t to_read = node->length - offset;
        if (to_read > sizeof(buffer)) {
            to_read = sizeof(buffer);
        }
        uint32_t read = vfs_read(node, offset, to_read, buffer);
        if (read == 0) {
            break;
        }
        for (uint32_t i = 0; i < read; ++i) {
            shell_putc((char)buffer[i]);
        }
        offset += read;
    }
    shell_putc('\n');
}

static void cmd_panic(int argc, char** argv) {
    (void)argc;
    (void)argv;
    panic("Manual panic");
}

static uint32_t syscall_write(const char* text, uint32_t len) {
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(text), "c"(len)
        : "edx", "esi", "edi", "memory"
    );
    return ret;
}

static uint32_t syscall_getpid(void) {
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
        : "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
    return ret;
}

static uint32_t syscall_getprio(void) {
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPRIO)
        : "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
    return ret;
}

static void syscall_sleep(uint32_t ticks) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYS_SLEEP), "b"(ticks)
        : "ecx", "edx", "esi", "edi", "memory"
    );
}

static void syscall_setprio(uint32_t pid, uint32_t prio) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYS_SETPRIO), "b"(pid), "c"(prio)
        : "edx", "esi", "edi", "memory"
    );
}

static void syscall_setslice(uint32_t pid, uint32_t ticks) {
    asm volatile(
        "int $0x80"
        :
        : "a"(SYS_SETSLICE), "b"(pid), "c"(ticks)
        : "edx", "esi", "edi", "memory"
    );
}

static int syscall_open(const char* path) {
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(path)
        : "ecx", "edx", "esi", "edi", "memory"
    );
    return (int)ret;
}

static uint32_t syscall_read_fd(uint32_t fd, uint8_t* buffer, uint32_t size) {
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
        : "esi", "edi", "memory"
    );
    return ret;
}

static int syscall_close(uint32_t fd) {
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "ecx", "edx", "esi", "edi", "memory"
    );
    return (int)ret;
}

static void cmd_syscall(int argc, char** argv) {
    (void)argc;
    (void)argv;
    const char* msg = "syscall write ok\n";
    syscall_write(msg, strlen(msg));
}

static void cmd_mem(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint64_t total_blocks = pmm_total_blocks();
    uint64_t used_blocks = pmm_used_blocks();
    uint64_t block_size = pmm_block_size();
    shell_write("pmm total: ");
    shell_write_uint64(total_blocks * block_size);
    shell_write("\n");
    shell_write("pmm used: ");
    shell_write_uint64(used_blocks * block_size);
    shell_write("\n");
    shell_write("heap total: ");
    shell_write_uint64(heap_total_bytes());
    shell_write("\n");
    shell_write("heap free: ");
    shell_write_uint64(heap_free_bytes());
    shell_write("\n");
}

static void cmd_ps(int argc, char** argv) {
    cmd_procs(argc, argv);
}

static void cmd_kill(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: kill <pid>\n");
        return;
    }
    uint32_t pid;
    if (!shell_parse_u32(argv[1], &pid)) {
        shell_write("Invalid pid\n");
        return;
    }
    if (scheduler_kill(pid)) {
        shell_write("killed\n");
    } else {
        shell_write("kill failed\n");
    }
}

static void cmd_prio(int argc, char** argv) {
    if (argc == 1) {
        uint32_t pid = syscall_getpid();
        uint32_t prio = syscall_getprio();
        shell_write("pid ");
        shell_write_uint64(pid);
        shell_write(" prio ");
        shell_write_uint64(prio);
        shell_putc('\n');
        return;
    }
    uint32_t pid = 0;
    uint32_t prio = 0;
    if (argc == 2) {
        pid = syscall_getpid();
        if (!shell_parse_u32(argv[1], &prio)) {
            shell_write("Invalid prio\n");
            return;
        }
    } else if (argc == 3) {
        if (!shell_parse_u32(argv[1], &pid) || !shell_parse_u32(argv[2], &prio)) {
            shell_write("Invalid pid or prio\n");
            return;
        }
    } else {
        shell_write("Usage: prio [pid] <prio>\n");
        return;
    }
    syscall_setprio(pid, prio);
    shell_write("ok\n");
}

static void cmd_slice(int argc, char** argv) {
    if (argc == 1) {
        process_t* current = scheduler_current();
        if (!current) {
            shell_write("no current process\n");
            return;
        }
        shell_write("pid ");
        shell_write_uint64(current->pid);
        shell_write(" slice ");
        shell_write_uint64(current->time_slice);
        shell_write(" rem ");
        shell_write_uint64(current->time_remaining);
        shell_putc('\n');
        return;
    }
    uint32_t pid = 0;
    uint32_t ticks = 0;
    if (argc == 2) {
        pid = syscall_getpid();
        if (!shell_parse_u32(argv[1], &ticks)) {
            shell_write("Invalid ticks\n");
            return;
        }
    } else if (argc == 3) {
        if (!shell_parse_u32(argv[1], &pid) || !shell_parse_u32(argv[2], &ticks)) {
            shell_write("Invalid pid or ticks\n");
            return;
        }
    } else {
        shell_write("Usage: slice [pid] <ticks>\n");
        return;
    }
    if (ticks == 0) {
        shell_write("ticks must be > 0\n");
        return;
    }
    syscall_setslice(pid, ticks);
    shell_write("ok\n");
}

static void cmd_sleep(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: sleep <ticks>\n");
        return;
    }
    uint32_t ticks = 0;
    if (!shell_parse_u32(argv[1], &ticks) || ticks == 0) {
        shell_write("Invalid ticks\n");
        return;
    }
    syscall_sleep(ticks);
    shell_write("ok\n");
}

static void cmd_fdcat(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: fdcat <path>\n");
        return;
    }
    int fd = syscall_open(argv[1]);
    if (fd < 0) {
        shell_write("open failed\n");
        return;
    }
    uint8_t buffer[64];
    for (;;) {
        uint32_t read = syscall_read_fd((uint32_t)fd, buffer, sizeof(buffer));
        if (read == OS_ERR) {
            shell_write("read failed\n");
            break;
        }
        if (read == 0) {
            break;
        }
        for (uint32_t i = 0; i < read; ++i) {
            shell_putc((char)buffer[i]);
        }
    }
    syscall_close((uint32_t)fd);
    shell_putc('\n');
}

static void cmd_procs(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t count = scheduler_process_count();
    shell_write("processes: ");
    shell_write_uint64(count);
    shell_write("\n");
    process_t* head = scheduler_process_list();
    process_t* current = head;
    for (uint32_t i = 0; i < count && current; ++i) {
        shell_write("pid ");
        shell_write_uint64(current->pid);
        shell_write(" state ");
        shell_write_uint64(current->state);
        shell_write(" prio ");
        shell_write_uint64(current->priority);
        shell_write(" boost ");
        shell_write_uint64(current->boost);
        shell_write(" slice ");
        shell_write_uint64(current->time_slice);
        shell_write(" rem ");
        shell_write_uint64(current->time_remaining);
        shell_write(" run ");
        shell_write_uint64(current->runtime_ticks);
        shell_write(" ready ");
        shell_write_uint64(current->ready_ticks);
        shell_write(" sw ");
        shell_write_uint64(current->switches);
        shell_write("\n");
        current = current->next;
    }
}

static void cmd_bt(int argc, char** argv) {
    (void)argc;
    (void)argv;
    debug_backtrace(16);
}

static void cmd_ask(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: ask <question>\n");
        return;
    }
    char query[SHELL_LINE_SIZE];
    uint32_t pos = 0;
    for (int i = 1; i < argc && pos + 1 < SHELL_LINE_SIZE; ++i) {
        const char* part = argv[i];
        uint32_t j = 0;
        while (part[j] && pos + 1 < SHELL_LINE_SIZE) {
            query[pos++] = part[j++];
        }
        if (i + 1 < argc && pos + 1 < SHELL_LINE_SIZE) {
            query[pos++] = ' ';
        }
    }
    query[pos] = 0;

    ai_context_t context;
    context.mem_total = (uint64_t)pmm_total_blocks() * (uint64_t)pmm_block_size();
    context.mem_used = (uint64_t)pmm_used_blocks() * (uint64_t)pmm_block_size();
    context.heap_total = heap_total_bytes();
    context.heap_free = heap_free_bytes();
    context.uptime_ticks = timer_get_ticks();

    ai_ask(query, &context);
}

static void cmd_aiinfo(int argc, char** argv) {
    (void)argc;
    (void)argv;
    gguf_header_t header;
    if (!ai_model_read_header(&header)) {
        shell_write("No AI model\n");
        return;
    }
    shell_write("gguf version ");
    shell_write_uint64(header.version);
    shell_write("\n");
    shell_write("tensors ");
    shell_write_uint64(header.tensor_count);
    shell_write("\n");
    shell_write("kv ");
    shell_write_uint64(header.kv_count);
    shell_write("\n");
    shell_write("size ");
    shell_write_uint64(ai_model_size());
    shell_write("\n");
    uint64_t data_start = 0;
    if (ai_tensor_data_start(&data_start)) {
        shell_write("data ");
        shell_write_hex32((uint32_t)data_start);
        shell_write("\n");
    }

    uint64_t show = header.tensor_count < 3 ? header.tensor_count : 3;
    for (uint64_t i = 0; i < show; ++i) {
        ai_tensor_desc_t desc;
        if (!ai_tensor_get((uint32_t)i, &desc)) {
            shell_write("gguf tensor error\n");
            return;
        }
        shell_write("tensor ");
        shell_write(desc.name);
        shell_write(" type ");
        shell_write(ai_tensor_type_name(desc.type));
        shell_write(" dims ");
        for (uint32_t d = 0; d < desc.n_dims; ++d) {
            if (d > 0) {
                shell_write("x");
            }
            shell_write_uint64(desc.dims[d]);
        }
        shell_write(" offset ");
        shell_write_hex32((uint32_t)desc.data_offset);
        shell_write(" bytes ");
        shell_write_uint64(desc.data_bytes);
        shell_write("\n");
    }
}

static void cmd_brain_stats(int argc, char** argv) {
    (void)argc;
    (void)argv;
    gguf_header_t header;
    if (!ai_model_read_header(&header)) {
        shell_write("No AI model\n");
        return;
    }
    shell_write("base ");
    shell_write_hex32((uint32_t)ai_model_data());
    shell_write("\n");
    shell_write("size ");
    shell_write_uint64(ai_model_size());
    shell_write("\n");
    shell_write("gguf ");
    shell_write_uint64(header.version);
    shell_write("\n");
    shell_write("tensors ");
    shell_write_uint64(header.tensor_count);
    shell_write("\n");
    shell_write("kv ");
    shell_write_uint64(header.kv_count);
    shell_write("\n");
    uint64_t data_start = 0;
    if (ai_tensor_data_start(&data_start)) {
        shell_write("data ");
        shell_write_hex32((uint32_t)data_start);
        shell_write("\n");
    }
}

static void cmd_debug_ai(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char buf[16];
    uint32_t count = ai_debug_last_tokens(buf, sizeof(buf));
    if (count == 0) {
        shell_write("No tokens\n");
        return;
    }
    shell_write("tokens ");
    shell_write_uint64(count);
    shell_write(": ");
    shell_write(buf);
    shell_write("\n");
}

static void cmd_log(int argc, char** argv) {
    if (argc > 1 && shell_strcmp(argv[1], "clear") == 0) {
        diag_clear();
        shell_write("log cleared\n");
        return;
    }
    uint32_t count = diag_count();
    if (count == 0) {
        shell_write("No log entries\n");
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        shell_write(diag_get(i));
        shell_putc('\n');
    }
}

static void cmd_selftest(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t failures = selftest_run();
    shell_write("selftest failures ");
    shell_write_uint64(failures);
    shell_write("\n");
}

static void cmd_bench(int argc, char** argv) {
    uint32_t size = 32;
    if (argc > 1) {
        if (!shell_parse_u32(argv[1], &size)) {
            shell_write("Usage: bench <size>\n");
            return;
        }
    }
    uint64_t ticks = bench_matmul(size);
    if (ticks == 0) {
        shell_write("bench failed\n");
        return;
    }
    shell_write("bench ticks ");
    shell_write_uint64(ticks);
    shell_write("\n");
}

static void cmd_infer(int argc, char** argv) {
    if (argc < 2) {
        shell_write("Usage: infer <prompt>\n");
        return;
    }
    char query[SHELL_LINE_SIZE];
    uint32_t pos = 0;
    for (int i = 1; i < argc && pos + 1 < SHELL_LINE_SIZE; ++i) {
        const char* part = argv[i];
        uint32_t j = 0;
        while (part[j] && pos + 1 < SHELL_LINE_SIZE) {
            query[pos++] = part[j++];
        }
        if (i + 1 < argc && pos + 1 < SHELL_LINE_SIZE) {
            query[pos++] = ' ';
        }
    }
    query[pos] = 0;

    ai_context_t context;
    context.mem_total = (uint64_t)pmm_total_blocks() * (uint64_t)pmm_block_size();
    context.mem_used = (uint64_t)pmm_used_blocks() * (uint64_t)pmm_block_size();
    context.heap_total = heap_total_bytes();
    context.heap_free = heap_free_bytes();
    context.uptime_ticks = timer_get_ticks();

    ai_infer(query, &context);
}

static void cmd_reboot(int argc, char** argv) {
    (void)argc;
    (void)argv;
    shell_write("Rebooting...\n");
    cpu_reboot();
}

static void cmd_lspci(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t count = pci_scan_bus();
    if (count == 0) {
        shell_write("No PCI devices\n");
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t bus = 0;
        uint8_t slot = 0;
        uint8_t func = 0;
        uint16_t vendor = 0;
        uint16_t device = 0;
        uint8_t class_id = 0;
        uint8_t subclass = 0;
        if (!pci_device_info(i, &bus, &slot, &func, &vendor, &device, &class_id, &subclass)) {
            continue;
        }
        shell_write("bus ");
        shell_write_hex8(bus);
        shell_write(" slot ");
        shell_write_hex8(slot);
        shell_write(" func ");
        shell_write_hex8(func);
        shell_write(" ven ");
        shell_write_hex32(vendor);
        shell_write(" dev ");
        shell_write_hex32(device);
        shell_write(" class ");
        shell_write_hex8(class_id);
        shell_write(" sub ");
        shell_write_hex8(subclass);
        shell_write(" ");
        shell_write(shell_pci_class_name(class_id, subclass));
        shell_write("\n");
    }
}

static void cmd_rtc(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    shell_rtc_read_raw(&sec, &min, &hour, &day, &month, &year);
    uint8_t status_b = rtc_read(0x0B);
    int bcd = (status_b & 0x04) == 0;
    int hour_24 = (status_b & 0x02) != 0;
    int pm = hour & 0x80;
    if (bcd) {
        sec = shell_bcd_to_bin(sec);
        min = shell_bcd_to_bin(min);
        hour = shell_bcd_to_bin((uint8_t)(hour & 0x7F));
        day = shell_bcd_to_bin(day);
        month = shell_bcd_to_bin(month);
        year = shell_bcd_to_bin(year);
    } else {
        hour &= 0x7F;
    }
    if (!hour_24) {
        if (pm && hour < 12) {
            hour = (uint8_t)(hour + 12);
        } else if (!pm && hour == 12) {
            hour = 0;
        }
    }
    shell_write("20");
    shell_write_two(year);
    shell_write("-");
    shell_write_two(month);
    shell_write("-");
    shell_write_two(day);
    shell_write(" ");
    shell_write_two(hour);
    shell_write(":");
    shell_write_two(min);
    shell_write(":");
    shell_write_two(sec);
    shell_write("\n");
}

static void cmd_diag(int argc, char** argv) {
    if (argc == 1) {
        shell_write("diag ");
        shell_write(diag_is_enabled() ? "on" : "off");
        shell_write(" entries ");
        shell_write_uint64(diag_count());
        shell_write("\n");
        return;
    }
    if (shell_strcmp(argv[1], "on") == 0) {
        diag_set_enabled(1);
        shell_write("diag on\n");
        return;
    }
    if (shell_strcmp(argv[1], "off") == 0) {
        diag_set_enabled(0);
        shell_write("diag off\n");
        return;
    }
    shell_write("Usage: diag [on|off]\n");
}

static void cmd_acpi(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t rsdp = acpi_rsdp_addr();
    if (!rsdp) {
        shell_write("ACPI not found\n");
        return;
    }
    uint32_t rsdt = acpi_rsdt_addr();
    shell_write("rsdp ");
    shell_write_hex32(rsdp);
    shell_write("\n");
    shell_write("rsdt ");
    shell_write_hex32(rsdt);
    shell_write("\n");
    uint32_t count = acpi_rsdt_entries();
    shell_write("entries ");
    shell_write_uint64(count);
    shell_write("\n");
    typedef struct {
        uint8_t signature[4];
        uint32_t length;
    } acpi_sdt_header_t;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t addr = acpi_rsdt_entry(i);
        if (!addr) {
            continue;
        }
        const acpi_sdt_header_t* hdr = (const acpi_sdt_header_t*)addr;
        shell_write_hex32(addr);
        shell_write(" ");
        shell_write_sig(hdr->signature);
        shell_write(" len ");
        shell_write_uint64(hdr->length);
        shell_write("\n");
    }
}

static void cmd_cpuinfo(int argc, char** argv) {
    (void)argc;
    (void)argv;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    shell_cpuid(0, &a, &b, &c, &d);
    char vendor[13];
    ((uint32_t*)vendor)[0] = b;
    ((uint32_t*)vendor)[1] = d;
    ((uint32_t*)vendor)[2] = c;
    vendor[12] = 0;
    shell_write("vendor ");
    shell_write(vendor);
    shell_write("\n");
    shell_cpuid(1, &a, &b, &c, &d);
    uint32_t stepping = a & 0xF;
    uint32_t model = (a >> 4) & 0xF;
    uint32_t family = (a >> 8) & 0xF;
    uint32_t ext_model = (a >> 16) & 0xF;
    uint32_t ext_family = (a >> 20) & 0xFF;
    if (family == 0xF) {
        family += ext_family;
    }
    if (family == 0x6 || family == 0xF) {
        model += ext_model << 4;
    }
    shell_write("family ");
    shell_write_uint64(family);
    shell_write(" model ");
    shell_write_uint64(model);
    shell_write(" stepping ");
    shell_write_uint64(stepping);
    shell_write("\n");
    shell_write("logical ");
    shell_write_uint64((b >> 16) & 0xFF);
    shell_write("\n");
    shell_write("features ");
    shell_write_flag("mmx", (d >> 23) & 1u);
    shell_write_flag("sse", (d >> 25) & 1u);
    shell_write_flag("sse2", (d >> 26) & 1u);
    shell_write_flag("sse3", (c >> 0) & 1u);
    shell_write_flag("ssse3", (c >> 9) & 1u);
    shell_write_flag("sse4.1", (c >> 19) & 1u);
    shell_write_flag("sse4.2", (c >> 20) & 1u);
    shell_write_flag("avx", (c >> 28) & 1u);
    shell_cpuid(7, &a, &b, &c, &d);
    shell_write_flag("bmi1", (b >> 3) & 1u);
    shell_write_flag("avx2", (b >> 5) & 1u);
    shell_write_flag("bmi2", (b >> 8) & 1u);
    shell_write("\n");
}

void shell_init(void) {
    line_length = 0;
    cursor_pos = 0;
    history_count = 0;
    history_head = 0;
    history_index = 0;
    shell_write("Shell ready\n");
    shell_prompt();
}

void shell_on_input(int key) {
    int ch = key;
    if (ch == 12) {
        fb_clear(0);
        fb_console_init(0xFFFFFF, 0);
        line_length = 0;
        cursor_pos = 0;
        shell_prompt();
        return;
    }
    if (ch == 21) {
        line_length = 0;
        cursor_pos = 0;
        shell_redraw_line();
        return;
    }
    if (ch == 1) {
        while (cursor_pos > 0) {
            cursor_pos--;
            shell_putc('\b');
        }
        return;
    }
    if (ch == 5) {
        while (cursor_pos < line_length) {
            shell_putc(line_buffer[cursor_pos]);
            cursor_pos++;
        }
        return;
    }
    if (ch == 2) {
        if (cursor_pos > 0) {
            cursor_pos--;
            shell_putc('\b');
        }
        return;
    }
    if (ch == 6) {
        if (cursor_pos < line_length) {
            shell_putc(line_buffer[cursor_pos]);
            cursor_pos++;
        }
        return;
    }
    if (ch == 4) {
        if (cursor_pos < line_length) {
            for (uint32_t i = cursor_pos; i + 1 < line_length; ++i) {
                line_buffer[i] = line_buffer[i + 1];
            }
            line_length--;
            shell_redraw_line();
        }
        return;
    }
    if (ch == KEY_ARROW_UP || ch == 16) {
        if (history_count > 0 && history_index > 0) {
            history_index--;
            shell_set_line(history_at(history_index));
        }
        return;
    }
    if (ch == KEY_ARROW_DOWN || ch == 14) {
        if (history_index + 1 < history_count) {
            history_index++;
            shell_set_line(history_at(history_index));
        } else if (history_index < history_count) {
            history_index = history_count;
            line_length = 0;
            cursor_pos = 0;
            shell_redraw_line();
        }
        return;
    }
    if (ch == KEY_ARROW_LEFT) {
        if (cursor_pos > 0) {
            cursor_pos--;
            shell_putc('\b');
        }
        return;
    }
    if (ch == KEY_ARROW_RIGHT) {
        if (cursor_pos < line_length) {
            shell_putc(line_buffer[cursor_pos]);
            cursor_pos++;
        }
        return;
    }
    if (ch == KEY_DELETE) {
        if (cursor_pos < line_length) {
            for (uint32_t i = cursor_pos; i + 1 < line_length; ++i) {
                line_buffer[i] = line_buffer[i + 1];
            }
            line_length--;
            shell_redraw_line();
        }
        return;
    }
    if (ch == KEY_PAGE_UP) {
        fb_console_scroll(5);
        return;
    }
    if (ch == KEY_PAGE_DOWN) {
        fb_console_scroll(-5);
        return;
    }
    if (ch == '\n') {
        shell_putc('\n');
        if (line_length > 0) {
            uint32_t slot = history_head;
            for (uint32_t i = 0; i < line_length && i + 1 < SHELL_LINE_SIZE; ++i) {
                history[slot][i] = line_buffer[i];
            }
            history[slot][line_length] = 0;
            history_head = (history_head + 1) % SHELL_HISTORY;
            if (history_count < SHELL_HISTORY) {
                history_count++;
            }
            history_index = history_count;
        }
        line_buffer[line_length] = 0;
        shell_execute_line();
        line_length = 0;
        cursor_pos = 0;
        shell_prompt();
        return;
    }
    if (ch == '\t') {
        shell_autocomplete();
        return;
    }
    if (ch == '\b') {
        if (cursor_pos > 0) {
            for (uint32_t i = cursor_pos - 1; i + 1 < line_length; ++i) {
                line_buffer[i] = line_buffer[i + 1];
            }
            line_length--;
            cursor_pos--;
            shell_redraw_line();
        }
        return;
    }
    if (line_length + 1 >= SHELL_LINE_SIZE) {
        return;
    }
    for (uint32_t i = line_length; i > cursor_pos; --i) {
        line_buffer[i] = line_buffer[i - 1];
    }
    line_buffer[cursor_pos] = (char)ch;
    line_length++;
    cursor_pos++;
    shell_redraw_line();
}
