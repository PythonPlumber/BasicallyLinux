#include "types.h"

void vga_display_splash(void);
void vga_putc(char ch);
void vga_puts(const char* text);
void vga_clear(uint8_t color);
void vga_set_color(uint8_t color);
void vga_scroll(int lines);
void vga_set_cursor(uint8_t x, uint8_t y);
void vga_hide_cursor(void);
void vga_print_art(void);
void vga_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void terminal_set_font(int use_vector);
void backspace_handler(void);
uint8_t vga_get_x(void);
uint8_t vga_get_y(void);
void update_vga_buffer(void);
