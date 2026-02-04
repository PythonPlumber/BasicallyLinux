#include "framebuffer.h"
#include "vga.h"
#include "font.h"
#include "multiboot.h"
#include "ports.h"
#include "types.h"

#define FB_TYPE_TEXT 0
#define FB_TYPE_LINEAR 1
#define TEXT_COLS 80
#define TEXT_ROWS 25
#define SCROLLBACK_LINES 200

typedef struct {
    uint16_t attributes;
    uint8_t winA;
    uint8_t winB;
    uint16_t granularity;
    uint16_t winsize;
    uint16_t segmentA;
    uint16_t segmentB;
    uint32_t realFctPtr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t wChar;
    uint8_t yChar;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;
    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t directcolor_attributes;
    uint32_t physbase;
    uint32_t offscreen_mem_offset;
    uint16_t offscreen_mem_size;
    uint8_t reserved1[206];
} __attribute__((packed)) vbe_mode_info_t;

static framebuffer_t framebuffer;
static uint16_t* text_buffer = (uint16_t*)0xB8000;
static uint8_t text_color = 0x07;
static uint32_t text_x = 0;
static uint32_t text_y = 0;
static uint16_t scrollback[SCROLLBACK_LINES][TEXT_COLS];
static uint32_t scrollback_head = 0;
static uint32_t scrollback_count = 1;
static uint32_t scrollback_view = 0;
static uint8_t scrollback_viewing = 0;
static uint32_t console_color = 0xFFFFFF;
static uint32_t console_x = 0;
static uint32_t console_y = 0;
static uint8_t console_use_vector = 0;

static uint16_t text_entry(uint8_t ch, uint8_t color) {
    return (uint16_t)ch | ((uint16_t)color << 8);
}

static void scrollback_clear_line(uint32_t idx) {
    uint16_t blank = text_entry(' ', text_color);
    for (uint32_t i = 0; i < TEXT_COLS; ++i) {
        scrollback[idx][i] = blank;
    }
}

static uint32_t scrollback_oldest(void) {
    if (scrollback_count < SCROLLBACK_LINES) {
        return 0;
    }
    return (scrollback_head + 1) % SCROLLBACK_LINES;
}

static uint32_t scrollback_tail_start(void) {
    if (scrollback_count <= TEXT_ROWS) {
        return 0;
    }
    return (scrollback_head + SCROLLBACK_LINES + 1 - TEXT_ROWS) % SCROLLBACK_LINES;
}

static void scrollback_render(void) {
    for (uint32_t row = 0; row < TEXT_ROWS; ++row) {
        uint32_t line_idx = (scrollback_view + row) % SCROLLBACK_LINES;
        uint16_t* dst = &text_buffer[row * TEXT_COLS];
        if (scrollback_count < TEXT_ROWS && row >= scrollback_count) {
            uint16_t blank = text_entry(' ', text_color);
            for (uint32_t col = 0; col < TEXT_COLS; ++col) {
                dst[col] = blank;
            }
            continue;
        }
        for (uint32_t col = 0; col < TEXT_COLS; ++col) {
            dst[col] = scrollback[line_idx][col];
        }
    }
    if (!scrollback_viewing) {
        text_y = (scrollback_count >= TEXT_ROWS) ? (TEXT_ROWS - 1) : (scrollback_count - 1);
    }
}

static void scrollback_reset(void) {
    scrollback_head = 0;
    scrollback_count = 1;
    scrollback_view = 0;
    scrollback_viewing = 0;
    for (uint32_t i = 0; i < SCROLLBACK_LINES; ++i) {
        scrollback_clear_line(i);
    }
    scrollback_render();
}

static void scrollback_newline(void) {
    if (scrollback_count < SCROLLBACK_LINES) {
        scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
        scrollback_count++;
    } else {
        scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
        uint32_t oldest = scrollback_oldest();
        if (scrollback_view == oldest) {
            scrollback_view = (scrollback_view + 1) % SCROLLBACK_LINES;
        }
    }
    scrollback_clear_line(scrollback_head);
    if (!scrollback_viewing) {
        scrollback_view = scrollback_tail_start();
    }
    scrollback_render();
}
framebuffer_t* fb_get(void) {
    return &framebuffer;
}

void fb_init(void* multiboot_info) {
    framebuffer.address = 0;
    framebuffer.width = 80;
    framebuffer.height = 25;
    framebuffer.pitch = 160;
    framebuffer.bpp = 16;
    framebuffer.type = FB_TYPE_TEXT;

    multiboot_info_t* info = (multiboot_info_t*)multiboot_info;
    if (info && (info->flags & (1u << 11)) && info->vbe_mode_info) {
        vbe_mode_info_t* mode = (vbe_mode_info_t*)(uint32_t)info->vbe_mode_info;
        if (mode->physbase && mode->bpp >= 24) {
            framebuffer.address = (uint8_t*)(uint32_t)mode->physbase;
            framebuffer.width = mode->width;
            framebuffer.height = mode->height;
            framebuffer.pitch = mode->pitch;
            framebuffer.bpp = mode->bpp;
            framebuffer.type = FB_TYPE_LINEAR;
        }
    }
}

static void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (framebuffer.type != FB_TYPE_LINEAR) {
        return;
    }
    if (x >= framebuffer.width || y >= framebuffer.height) {
        return;
    }
    uint8_t* row = framebuffer.address + y * framebuffer.pitch;
    if (framebuffer.bpp == 32) {
        uint32_t* pixel = (uint32_t*)(row + x * 4);
        *pixel = color;
    } else if (framebuffer.bpp == 24) {
        uint8_t* pixel = row + x * 3;
        pixel[0] = (uint8_t)(color & 0xFF);
        pixel[1] = (uint8_t)((color >> 8) & 0xFF);
        pixel[2] = (uint8_t)((color >> 16) & 0xFF);
    }
}

static void fb_draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color) {
    int32_t dx = (int32_t)(x1 > x0 ? x1 - x0 : x0 - x1);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = (int32_t)(y1 > y0 ? y1 - y0 : y0 - y1);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = (dx > dy ? dx : -dy) / 2;
    for (;;) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int32_t e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 = (uint32_t)((int32_t)x0 + sx);
        }
        if (e2 < dy) {
            err += dx;
            y0 = (uint32_t)((int32_t)y0 + sy);
        }
    }
}

static void fb_draw_glyph_bitmap(uint32_t x, uint32_t y, uint32_t color, uint32_t codepoint) {
    const bitmap_font_t* font = font_bitmap_get();
    if (codepoint >= 128) {
        codepoint = '?';
    }
    const uint8_t* glyph = font->data + codepoint * font->height;
    for (uint32_t row = 0; row < font->height; ++row) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < font->width; ++col) {
            if (bits & (1u << (7 - col))) {
                fb_put_pixel(x + col, y + row, color);
            }
        }
    }
}

static void fb_draw_glyph_vector(uint32_t x, uint32_t y, uint32_t color, uint32_t codepoint) {
    vector_glyph_t glyph = font_vector_get(codepoint);
    for (uint32_t i = 0; i < glyph.count; ++i) {
        uint32_t idx = i * 4;
        uint32_t x0 = (uint32_t)glyph.segments[idx + 0];
        uint32_t y0 = (uint32_t)glyph.segments[idx + 1];
        uint32_t x1 = (uint32_t)glyph.segments[idx + 2];
        uint32_t y1 = (uint32_t)glyph.segments[idx + 3];
        fb_draw_line(x + x0, y + y0, x + x1, y + y1, color);
    }
}

void fb_clear(uint32_t color) {
    if (framebuffer.type == FB_TYPE_TEXT) {
        scrollback_reset();
        text_x = 0;
        text_y = 0;
        return;
    }
    for (uint32_t y = 0; y < framebuffer.height; ++y) {
        for (uint32_t x = 0; x < framebuffer.width; ++x) {
            fb_put_pixel(x, y, color);
        }
    }
}

static void console_newline(void) {
    const bitmap_font_t* font = font_bitmap_get();
    if (framebuffer.type == FB_TYPE_TEXT) {
        text_x = 0;
        scrollback_newline();
        return;
    }
    console_x = 0;
    console_y += font->height;
    if (console_y + font->height >= framebuffer.height) {
        console_y = 0;
    }
}

void fb_console_init(uint32_t color, int use_vector) {
    console_color = color;
    console_use_vector = use_vector ? 1 : 0;
    console_x = 0;
    console_y = 0;
    if (framebuffer.type == FB_TYPE_TEXT) {
        scrollback_reset();
        text_x = 0;
        text_y = 0;
    }
}

void fb_console_set_mode(int use_vector) {
    console_use_vector = use_vector ? 1 : 0;
}

void fb_console_set_color(uint32_t color) {
    console_color = color;
}

void fb_console_putc(char ch) {
    if (ch == '\n') {
        console_newline();
        return;
    }
    if (ch == '\b') {
        const bitmap_font_t* font = font_bitmap_get();
        if (framebuffer.type == FB_TYPE_TEXT) {
            if (text_x > 0) {
                text_x--;
                scrollback[scrollback_head][text_x] = text_entry(' ', text_color);
                scrollback_render();
            }
            return;
        }
        if (console_x >= font->width) {
            console_x -= font->width;
            for (uint32_t row = 0; row < font->height; ++row) {
                for (uint32_t col = 0; col < font->width; ++col) {
                    fb_put_pixel(console_x + col, console_y + row, 0);
                }
            }
        }
        return;
    }
    if (framebuffer.type == FB_TYPE_TEXT) {
        if (scrollback_viewing) {
            scrollback_viewing = 0;
            scrollback_view = scrollback_tail_start();
        }
        if (text_x >= TEXT_COLS) {
            console_newline();
        }
        scrollback[scrollback_head][text_x] = text_entry((uint8_t)ch, text_color);
        text_x++;
        if (text_x >= TEXT_COLS) {
            console_newline();
        } else {
            scrollback_render();
        }
        return;
    }
    uint32_t codepoint = (uint32_t)(uint8_t)ch;
    if (console_use_vector) {
        fb_draw_glyph_vector(console_x, console_y, console_color, codepoint);
    } else {
        fb_draw_glyph_bitmap(console_x, console_y, console_color, codepoint);
    }
    console_x += font_bitmap_get()->width;
    if (console_x + font_bitmap_get()->width >= framebuffer.width) {
        console_newline();
    }
}

void fb_console_backspace(void) {
    fb_console_putc('\b');
}

void fb_console_write_len(const char* text, uint32_t len) {
    uint32_t index = 0;
    if (framebuffer.type == FB_TYPE_TEXT && scrollback_viewing) {
        scrollback_viewing = 0;
        scrollback_view = scrollback_tail_start();
        scrollback_render();
    }
    while (index < len) {
        uint32_t codepoint;
        uint32_t before = index;
        codepoint = utf8_decode(text, &index);
        if (index == before) {
            index++;
            codepoint = '?';
        }
        if (codepoint == '\n') {
            console_newline();
            continue;
        }
        if (codepoint == '\b') {
            fb_console_backspace();
            continue;
        }
        if (framebuffer.type == FB_TYPE_TEXT) {
            fb_console_putc((char)codepoint);
            continue;
        }
        if (console_use_vector) {
            fb_draw_glyph_vector(console_x, console_y, console_color, codepoint);
        } else {
            fb_draw_glyph_bitmap(console_x, console_y, console_color, codepoint);
        }
        console_x += font_bitmap_get()->width;
        if (console_x + font_bitmap_get()->width >= framebuffer.width) {
            console_newline();
        }
    }
}

void fb_console_write(const char* text) {
    uint32_t index = 0;
    if (framebuffer.type == FB_TYPE_TEXT && scrollback_viewing) {
        scrollback_viewing = 0;
        scrollback_view = scrollback_tail_start();
        scrollback_render();
    }
    while (text[index]) {
        uint32_t codepoint = utf8_decode(text, &index);
        if (codepoint == '\n') {
            console_newline();
            continue;
        }
        if (codepoint == '\b') {
            fb_console_backspace();
            continue;
        }
        if (framebuffer.type == FB_TYPE_TEXT) {
            fb_console_putc((char)codepoint);
            continue;
        }
        if (console_use_vector) {
            fb_draw_glyph_vector(console_x, console_y, console_color, codepoint);
        } else {
            fb_draw_glyph_bitmap(console_x, console_y, console_color, codepoint);
        }
        console_x += font_bitmap_get()->width;
        if (console_x + font_bitmap_get()->width >= framebuffer.width) {
            console_newline();
        }
    }
}

void fb_console_scroll(int lines) {
    if (framebuffer.type != FB_TYPE_TEXT) {
        return;
    }
    if (scrollback_count <= TEXT_ROWS) {
        return;
    }
    if (lines > 0) {
        if (!scrollback_viewing) {
            scrollback_viewing = 1;
            scrollback_view = scrollback_tail_start();
        }
        uint32_t oldest = scrollback_oldest();
        for (int i = 0; i < lines; ++i) {
            if (scrollback_view == oldest) {
                break;
            }
            scrollback_view = (scrollback_view + SCROLLBACK_LINES - 1) % SCROLLBACK_LINES;
        }
    } else if (lines < 0) {
        if (scrollback_viewing) {
            uint32_t tail = scrollback_tail_start();
            for (int i = 0; i < -lines; ++i) {
                if (scrollback_view == tail) {
                    scrollback_viewing = 0;
                    break;
                }
                scrollback_view = (scrollback_view + 1) % SCROLLBACK_LINES;
            }
        }
    }
    if (!scrollback_viewing) {
        scrollback_view = scrollback_tail_start();
    }
    scrollback_render();
}

void vga_putc(char ch) {
    fb_console_putc(ch);
}

void vga_puts(const char* text) {
    fb_console_write(text);
}

void vga_clear(uint8_t color) {
    text_color = color;
    fb_clear(0);
}

void vga_set_color(uint8_t color) {
    text_color = color;
    fb_console_set_color((uint32_t)color);
}

void vga_scroll(int lines) {
    fb_console_scroll(lines);
}

void vga_set_cursor(uint8_t x, uint8_t y) {
    if (framebuffer.type != FB_TYPE_TEXT) {
        return;
    }
    if (x >= TEXT_COLS || y >= TEXT_ROWS) {
        return;
    }
    text_x = x;
    text_y = y;
    uint16_t pos = (uint16_t)(y * TEXT_COLS + x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_hide_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
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
    static const char* disclaimer = "(It's definitely not)";
    static const char* messages[] = {
        "Initializing GDT...",
        "Loading IDT...",
        "Enabling Paging...",
        "Initializing Heap...",
        "Starting Scheduler...",
        "Bringing up Devices...",
        "Launching Shell..."
    };

    fb_clear(0);
    fb_console_init(0x00FF00, 0); // Green logo
    for (uint32_t i = 0; i < sizeof(logo) / sizeof(logo[0]); ++i) {
        fb_console_write(logo[i]);
        fb_console_write("\n");
    }
    fb_console_init(0xFF0000, 0); // Red disclaimer
    fb_console_write(disclaimer);
    fb_console_write("\n\n");
    
    fb_console_init(0xFFFFFF, 0); // White messages
    for (uint32_t i = 0; i < sizeof(messages) / sizeof(messages[0]); ++i) {
        const char* msg = messages[i];
        for (uint32_t c = 0; msg[c] != 0; ++c) {
            char buf[2] = {msg[c], 0};
            fb_console_write(buf);
            // Simple wait loop since we don't have a reliable wait_ticks here
            for (volatile int j = 0; j < 1000000; j++);
        }
        fb_console_write("\n");
    }
    
    fb_console_write("\n");
    fb_console_init(0x00FFFF, 0); // Cyan build info
    fb_console_write("Build Info: v0.0.1 | i686-linux-gnu\n");
    
    for (volatile int j = 0; j < 50000000; j++);
    fb_clear(0);
}

void vga_print_art(void) {
    vga_display_splash();
}

void vga_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (framebuffer.type == FB_TYPE_TEXT) {
        uint8_t attr = (uint8_t)color;
        for (uint16_t row = 0; row < h && (y + row) < TEXT_ROWS; ++row) {
            for (uint16_t col = 0; col < w && (x + col) < TEXT_COLS; ++col) {
                uint32_t idx = (uint32_t)(y + row) * TEXT_COLS + (x + col);
                text_buffer[idx] = text_entry(' ', attr);
            }
        }
        scrollback_render();
        return;
    }
    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t col = 0; col < w; ++col) {
            fb_put_pixel(x + col, y + row, color);
        }
    }
}

void terminal_set_font(int use_vector) {
    fb_console_set_mode(use_vector);
}

void backspace_handler(void) {
    fb_console_backspace();
}

uint8_t vga_get_x(void) {
    return (uint8_t)text_x;
}

uint8_t vga_get_y(void) {
    return (uint8_t)text_y;
}

void update_vga_buffer(void) {
    if (framebuffer.type == FB_TYPE_TEXT) {
        scrollback_render();
    }
}
