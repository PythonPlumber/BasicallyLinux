#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

typedef struct {
    uint8_t* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint8_t type;
} framebuffer_t;

void fb_init(void* multiboot_info);
framebuffer_t* fb_get(void);
void fb_clear(uint32_t color);
void fb_console_init(uint32_t color, int use_vector);
void fb_console_write(const char* text);
void fb_console_write_len(const char* text, uint32_t len);
void fb_console_putc(char ch);
void fb_console_backspace(void);
void fb_console_set_mode(int use_vector);
void fb_console_set_color(uint32_t color);
void fb_console_scroll(int lines);

#endif
