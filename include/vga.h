#ifndef VGA_H
#define VGA_H

#include "types.h"

void vga_display_splash(void);
void vga_putc(char ch);
void vga_puts(const char* text);
void vga_clear(uint8_t color);
void vga_set_color(uint8_t color);
void vga_set_cursor(uint8_t x, uint8_t y);
uint8_t vga_get_x(void);
uint8_t vga_get_y(void);

#endif

