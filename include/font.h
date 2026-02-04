#include "types.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    const uint8_t* data;
} bitmap_font_t;

typedef struct {
    const uint8_t* segments;
    uint32_t count;
    uint32_t width;
    uint32_t height;
} vector_glyph_t;

const bitmap_font_t* font_bitmap_get(void);
vector_glyph_t font_vector_get(uint32_t codepoint);
uint32_t utf8_decode(const char* text, uint32_t* index);
