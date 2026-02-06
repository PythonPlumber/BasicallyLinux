#include "debug.h"
#include "video/framebuffer.h"
#include "drivers/serial.h"
#include <stdarg.h>

void debug_write(const char* msg) {
    fb_console_write(msg);
    serial_write(msg);
}

void debug_write_hex32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((7 - i) * 4)) & 0xF);
        buf[2 + i] = hex[nibble];
    }
    buf[10] = 0;
    fb_console_write(buf);
    serial_write(buf);
}

void debug_write_hex64(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[2 + i] = hex[nibble];
    }
    buf[18] = 0;
    fb_console_write(buf);
    serial_write(buf);
}

void panic(const char* msg) {
    fb_console_write("PANIC: ");
    fb_console_write(msg);
    fb_console_write("\n");
    serial_write("PANIC: ");
    serial_write(msg);
    serial_write("\n");
    debug_backtrace(16);
    for (;;) {
        asm volatile("hlt");
    }
}

void debug_backtrace(uint32_t max_frames) {
    uint32_t ebp;
    asm volatile("mov %%ebp, %0" : "=r"(ebp));
    debug_backtrace_from(ebp, max_frames);
}

void debug_backtrace_from(uint32_t ebp, uint32_t max_frames) {
    uint32_t frame = 0;
    fb_console_write("Backtrace:\n");
    serial_write("Backtrace:\n");
    while (ebp && frame < max_frames) {
        uint32_t* stack = (uint32_t*)ebp;
        uint32_t ret = stack[1];
        debug_write_hex32(ret);
        debug_write("\n");
        uint32_t next = stack[0];
        if (next <= ebp) {
            break;
        }
        ebp = next;
        frame++;
    }
}

static void kprintf_write(const char* text) {
    fb_console_write(text);
    serial_write(text);
}

static void kprintf_write_hex32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((7 - i) * 4)) & 0xF);
        buf[2 + i] = hex[nibble];
    }
    buf[10] = 0;
    kprintf_write(buf);
}

static void kprintf_write_uint(uint32_t value) {
    char buf[16];
    int pos = 0;
    if (value == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[16];
        int tmp_pos = 0;
        while (value > 0 && tmp_pos < 16) {
            tmp[tmp_pos++] = (char)('0' + (value % 10));
            value /= 10;
        }
        for (int i = tmp_pos - 1; i >= 0; --i) {
            buf[pos++] = tmp[i];
        }
    }
    buf[pos] = 0;
    kprintf_write(buf);
}

static void kprintf_write_int(int32_t value) {
    if (value < 0) {
        kprintf_write("-");
        kprintf_write_uint((uint32_t)(-value));
        return;
    }
    kprintf_write_uint((uint32_t)value);
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    for (uint32_t i = 0; fmt && fmt[i]; ++i) {
        if (fmt[i] != '%') {
            char ch[2];
            ch[0] = fmt[i];
            ch[1] = 0;
            kprintf_write(ch);
            continue;
        }
        ++i;
        char spec = fmt[i];
        if (!spec) {
            break;
        }
        if (spec == '%') {
            kprintf_write("%");
        } else if (spec == 's') {
            const char* text = va_arg(args, const char*);
            kprintf_write(text ? text : "(null)");
        } else if (spec == 'c') {
            char ch[2];
            ch[0] = (char)va_arg(args, int);
            ch[1] = 0;
            kprintf_write(ch);
        } else if (spec == 'd') {
            int32_t value = va_arg(args, int32_t);
            kprintf_write_int(value);
        } else if (spec == 'u') {
            uint32_t value = va_arg(args, uint32_t);
            kprintf_write_uint(value);
        } else if (spec == 'x' || spec == 'p') {
            uint32_t value = va_arg(args, uint32_t);
            kprintf_write_hex32(value);
        }
    }
    va_end(args);
}
