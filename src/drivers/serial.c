#include "serial.h"
#include "ports.h"
#include "types.h"
#include "util.h"

#define COM1 0x3F8

static spinlock_t serial_lock = 0;

static int serial_ready(void) {
    return (inb(COM1 + 5) & 0x20) != 0;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_write_char(char c) {
    while (!serial_ready()) { }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* text) {
    uint32_t flags = spin_lock_irqsave(&serial_lock);
    uint32_t i = 0;
    while (text[i]) {
        if (text[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(text[i]);
        ++i;
    }
    spin_unlock_irqrestore(&serial_lock, flags);
}

void serial_write_string(const char* text) {
    serial_write(text);
}

void serial_write_len(const char* text, uint32_t len) {
    uint32_t flags = spin_lock_irqsave(&serial_lock);
    for (uint32_t i = 0; i < len; ++i) {
        if (text[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(text[i]);
    }
    spin_unlock_irqrestore(&serial_lock, flags);
}

void serial_write_hex32(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((7 - i) * 4)) & 0xF);
        buf[2 + i] = hex[nibble];
    }
    buf[10] = 0;
    serial_write(buf);
}

void serial_write_hex64(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[2 + i] = hex[nibble];
    }
    buf[18] = 0;
    serial_write(buf);
}
