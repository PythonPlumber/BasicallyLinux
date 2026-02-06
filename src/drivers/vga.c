#include "video/framebuffer.h"
#include "types.h"
#include "drivers/vga.h"

static uint16_t* const vga_buffer = (uint16_t*)0xB8000;
static const uint8_t vga_width = 80;
static const uint8_t vga_height = 25;

static uint8_t vga_cursor_x = 0;
static uint8_t vga_cursor_y = 0;
static uint8_t vga_current_color = 0x07;

void vga_set_color(uint8_t color) {
    vga_current_color = color;
}

uint8_t vga_get_x(void) {
    return vga_cursor_x;
}

uint8_t vga_get_y(void) {
    return vga_cursor_y;
}

void vga_set_cursor(uint8_t x, uint8_t y) {
    if (x < vga_width) vga_cursor_x = x;
    if (y < vga_height) vga_cursor_y = y;
    
    uint16_t pos = y * vga_width + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_hide_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_clear(uint8_t color) {
    vga_hide_cursor();
    uint16_t blank = (uint16_t)' ' | ((uint16_t)color << 8);
    for (uint32_t i = 0; i < (uint32_t)vga_width * vga_height; ++i) {
        vga_buffer[i] = blank;
    }
    vga_cursor_x = 0;
    vga_cursor_y = 0;
}

static void vga_put_at(char ch, uint8_t color, uint8_t x, uint8_t y) {
    if (x >= vga_width || y >= vga_height) {
        return;
    }
    uint32_t idx = (uint32_t)y * vga_width + x;
    vga_buffer[idx] = (uint16_t)ch | ((uint16_t)color << 8);
}

void vga_putc(char ch) {
    if (ch == '\n') {
        vga_cursor_x = 0;
        vga_cursor_y++;
    } else if (ch == '\r') {
        vga_cursor_x = 0;
    } else {
        vga_put_at(ch, vga_current_color, vga_cursor_x, vga_cursor_y);
        vga_cursor_x++;
    }

    if (vga_cursor_x >= vga_width) {
        vga_cursor_x = 0;
        vga_cursor_y++;
    }

    if (vga_cursor_y >= vga_height) {
        // Simple scroll: just reset to top or stay at bottom?
        // For now, let's just stay at bottom and clear or similar.
        // Real scrolling would involve memmove.
        vga_cursor_y = vga_height - 1;
    }
}

void vga_puts(const char* text) {
    for (uint32_t i = 0; text[i] != 0; ++i) {
        vga_putc(text[i]);
    }
}

static void vga_write_at(const char* text, uint8_t color, uint8_t x, uint8_t y) {
    uint8_t col = x;
    uint8_t row = y;
    for (uint32_t i = 0; text[i] != 0; ++i) {
        if (text[i] == '\n') {
            col = x;
            row++;
            continue;
        }
        vga_put_at(text[i], color, col, row);
        col++;
        if (col >= vga_width) {
            col = x;
            row++;
        }
        if (row >= vga_height) {
            break;
        }
    }
}

static void wait_ticks(uint64_t ticks) {
    if (ticks == 0) return;
    uint32_t eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    if (!(eflags & 0x200)) {
        // Interrupts are disabled, use a smaller busy loop to avoid hangs
        // 1 tick @ 100Hz is 10ms. 1000000 NOPs is roughly a few ms on modern CPUs.
        for (uint64_t i = 0; i < ticks * 1000000; ++i) {
            asm volatile("pause"); // Use pause for better power/sync
        }
        return;
    }
    uint64_t start = timer_get_ticks();
    uint64_t timeout = 0;
    while ((timer_get_ticks() - start) < ticks) { 
        asm volatile("pause");
        if (++timeout > 10000000) { // Safety break if timer never increments
             serial_write_string("WARNING: wait_ticks safety break triggered!\n");
             break;
        }
    }
}

static void typewriter_vga(const char* text, uint8_t color, uint8_t x, uint8_t y) {
    uint8_t col = x;
    uint8_t row = y;
    for (uint32_t i = 0; text[i] != 0; ++i) {
        if (text[i] == '\n') {
            col = x;
            row++;
            continue;
        }
        vga_put_at(text[i], color, col, row);
        col++;
        wait_ticks(1);
        if (col >= vga_width) {
            col = x;
            row++;
        }
        if (row >= vga_height) {
            break;
        }
    }
}

void vga_display_splash(void) {
    static const char* logo[] = {
        " ____              _          _      _ _          _            ",
        "| __ )  __ _ ___  (_) ___  __| | ___| (_)_ __ ___| |_ ___ _ __ ",
        "|  _ \\ / _` / __| | |/ _ \\/ _` |/ _ \\ | | '__/ __| __/ _ \\ '__|",
        "| |_) | (_| \\__ \\ | |  __/ (_| |  __/ | | |  \\__ \\ ||  __/ |   ",
        "|____/ \\__,_|___/ |_|\\___|\\__,_|\\___|_|_|_|  |___/\\__\\___|_|   ",
        "                     BasicallyLinux                             "
    };
    static const char* disclaimer = "(It’s definitely not)";
    static const char* messages[] = {
        "Initializing GDT...",
        "Loading IDT...",
        "Enabling Paging...",
        "Initializing Heap...",
        "Starting Scheduler...",
        "Bringing up Devices...",
        "Launching Shell..."
    };

    framebuffer_t* fb = fb_get();
    if (fb && fb->type == 1) {
        fb_clear(0x000000);
        fb_console_set_color(0x00FF00);
        for (uint32_t i = 0; i < sizeof(logo) / sizeof(logo[0]); ++i) {
            fb_console_write(logo[i]);
            fb_console_putc('\n');
        }
        fb_console_set_color(0xFF0000);
        fb_console_write(disclaimer);
        fb_console_putc('\n');
        fb_console_set_color(0xFFFFFF);
        fb_console_putc('\n');
        for (uint32_t i = 0; i < sizeof(messages) / sizeof(messages[0]); ++i) {
            const char* msg = messages[i];
            for (uint32_t c = 0; msg[c] != 0; ++c) {
                fb_console_putc(msg[c]);
                wait_ticks(1);
            }
            fb_console_putc('\n');
        }
        fb_console_putc('\n');
        fb_console_set_color(0x00FFFF);
        fb_console_write("Build Info: v0.0.1 | i686-elf");
        wait_ticks(300);
        fb_clear(0);
        fb_console_set_color(0xFFFFFF);
        return;
    }

    vga_clear(0x00);
    for (uint32_t i = 0; i < sizeof(logo) / sizeof(logo[0]); ++i) {
        vga_write_at(logo[i], 0x0A, 2, (uint8_t)(i + 1));
    }
    vga_write_at(disclaimer, 0x0C, 26, 8);
    uint8_t row = 10;
    for (uint32_t i = 0; i < sizeof(messages) / sizeof(messages[0]); ++i) {
        typewriter_vga(messages[i], 0x07, 4, row);
        row++;
    }
    for (uint8_t col = 0; col < vga_width; ++col) {
        vga_put_at(' ', 0x1F, col, vga_height - 1);
    }
    vga_write_at("Build Info: v0.0.1 | i686-elf", 0x1F, 2, vga_height - 1);
    wait_ticks(300);
    vga_clear(0x00);
}
