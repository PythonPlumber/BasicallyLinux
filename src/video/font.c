#include "video/font.h"

static const uint8_t font8x8_basic[128][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {24, 60, 60, 24, 24, 0, 24, 0},
    {54, 54, 20, 0, 0, 0, 0, 0},
    {54, 54, 127, 54, 127, 54, 54, 0},
    {24, 62, 3, 30, 48, 31, 24, 0},
    {0, 99, 103, 14, 28, 56, 115, 99},
    {28, 54, 28, 59, 102, 102, 59, 0},
    {6, 6, 12, 0, 0, 0, 0, 0},
    {12, 6, 3, 3, 3, 6, 12, 0},
    {3, 6, 12, 12, 12, 6, 3, 0},
    {0, 102, 60, 255, 60, 102, 0, 0},
    {0, 12, 12, 63, 12, 12, 0, 0},
    {0, 0, 0, 0, 0, 12, 12, 6},
    {0, 0, 0, 63, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 12, 12, 0},
    {96, 48, 24, 12, 6, 3, 1, 0},
    {62, 99, 115, 123, 111, 103, 62, 0},
    {12, 14, 15, 12, 12, 12, 63, 0},
    {62, 99, 96, 48, 24, 12, 127, 0},
    {62, 99, 96, 56, 96, 99, 62, 0},
    {48, 56, 60, 54, 127, 48, 120, 0},
    {127, 3, 63, 96, 96, 99, 62, 0},
    {60, 6, 3, 63, 99, 99, 62, 0},
    {127, 99, 48, 24, 12, 12, 12, 0},
    {62, 99, 99, 62, 99, 99, 62, 0},
    {62, 99, 99, 126, 96, 48, 30, 0},
    {0, 12, 12, 0, 0, 12, 12, 0},
    {0, 12, 12, 0, 0, 12, 12, 6},
    {48, 24, 12, 6, 12, 24, 48, 0},
    {0, 0, 63, 0, 0, 63, 0, 0},
    {6, 12, 24, 48, 24, 12, 6, 0},
    {62, 99, 96, 48, 24, 0, 24, 0},
    {62, 99, 96, 110, 107, 107, 62, 0},
    {24, 60, 102, 102, 126, 102, 102, 0},
    {63, 102, 102, 62, 102, 102, 63, 0},
    {60, 102, 3, 3, 3, 102, 60, 0},
    {31, 54, 102, 102, 102, 54, 31, 0},
    {127, 70, 22, 30, 22, 70, 127, 0},
    {127, 70, 22, 30, 22, 6, 15, 0},
    {60, 102, 3, 3, 115, 102, 124, 0},
    {102, 102, 102, 126, 102, 102, 102, 0},
    {63, 12, 12, 12, 12, 12, 63, 0},
    {120, 48, 48, 48, 51, 51, 30, 0},
    {103, 102, 54, 30, 54, 102, 103, 0},
    {15, 6, 6, 6, 70, 102, 127, 0},
    {99, 119, 127, 107, 99, 99, 99, 0},
    {99, 103, 111, 123, 115, 99, 99, 0},
    {62, 99, 99, 99, 99, 99, 62, 0},
    {63, 102, 102, 62, 6, 6, 15, 0},
    {62, 99, 99, 99, 107, 59, 110, 0},
    {63, 102, 102, 62, 54, 102, 103, 0},
    {62, 99, 7, 62, 112, 99, 62, 0},
    {255, 219, 24, 24, 24, 24, 60, 0},
    {102, 102, 102, 102, 102, 102, 62, 0},
    {102, 102, 102, 102, 102, 60, 24, 0},
    {99, 99, 99, 107, 127, 119, 99, 0},
    {99, 99, 54, 28, 54, 99, 99, 0},
    {102, 102, 102, 60, 24, 24, 60, 0},
    {127, 115, 25, 12, 70, 99, 127, 0},
    {30, 6, 6, 6, 6, 6, 30, 0},
    {3, 6, 12, 24, 48, 96, 64, 0},
    {30, 24, 24, 24, 24, 24, 30, 0},
    {8, 28, 54, 99, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 255},
    {12, 12, 24, 0, 0, 0, 0, 0},
    {0, 0, 62, 96, 126, 99, 126, 0},
    {7, 6, 6, 62, 102, 102, 59, 0},
    {0, 0, 62, 99, 3, 99, 62, 0},
    {112, 96, 96, 126, 99, 99, 126, 0},
    {0, 0, 62, 99, 127, 3, 62, 0},
    {56, 108, 12, 63, 12, 12, 30, 0},
    {0, 0, 126, 99, 99, 126, 96, 63},
    {7, 6, 54, 110, 102, 102, 103, 0},
    {12, 0, 14, 12, 12, 12, 63, 0},
    {48, 0, 56, 48, 48, 51, 51, 30},
    {7, 6, 102, 54, 30, 54, 103, 0},
    {14, 12, 12, 12, 12, 12, 63, 0},
    {0, 0, 55, 127, 107, 99, 99, 0},
    {0, 0, 59, 103, 99, 99, 99, 0},
    {0, 0, 62, 99, 99, 99, 62, 0},
    {0, 0, 59, 102, 102, 62, 6, 15},
    {0, 0, 126, 99, 99, 126, 96, 240},
    {0, 0, 59, 103, 99, 3, 7, 0},
    {0, 0, 126, 3, 62, 96, 63, 0},
    {8, 12, 63, 12, 12, 108, 56, 0},
    {0, 0, 99, 99, 99, 99, 126, 0},
    {0, 0, 99, 99, 99, 54, 28, 0},
    {0, 0, 99, 99, 107, 127, 54, 0},
    {0, 0, 99, 54, 28, 54, 99, 0},
    {0, 0, 99, 99, 99, 126, 96, 63},
    {0, 0, 127, 89, 24, 38, 127, 0},
    {56, 12, 12, 7, 12, 12, 56, 0},
    {12, 12, 12, 0, 12, 12, 12, 0},
    {7, 12, 12, 56, 12, 12, 7, 0},
    {54, 109, 0, 0, 0, 0, 0, 0},
    {0, 24, 36, 66, 66, 126, 0, 0}
};

static const bitmap_font_t bitmap_font = {
    8,
    8,
    &font8x8_basic[0][0]
};

static const uint8_t vec_zero[] = {0, 0, 7, 0, 7, 0, 7, 7, 7, 7, 0, 7, 0, 7, 0, 0};
static const uint8_t vec_one[] = {3, 0, 3, 7};
static const uint8_t vec_two[] = {0, 0, 7, 0, 7, 0, 7, 3, 7, 3, 0, 3, 0, 3, 0, 7, 0, 7, 7, 7};
static const uint8_t vec_three[] = {0, 0, 7, 0, 7, 0, 7, 7, 0, 3, 7, 3, 0, 7, 7, 7};
static const uint8_t vec_four[] = {0, 0, 0, 3, 0, 3, 7, 3, 7, 0, 7, 7};
static const uint8_t vec_five[] = {7, 0, 0, 0, 0, 0, 0, 3, 0, 3, 7, 3, 7, 3, 7, 7, 7, 7, 0, 7};
static const uint8_t vec_six[] = {7, 0, 0, 0, 0, 0, 0, 7, 0, 7, 7, 7, 7, 7, 7, 3, 7, 3, 0, 3};
static const uint8_t vec_seven[] = {0, 0, 7, 0, 7, 0, 3, 7};
static const uint8_t vec_eight[] = {0, 0, 7, 0, 7, 0, 7, 7, 7, 7, 0, 7, 0, 7, 0, 0, 0, 3, 7, 3};
static const uint8_t vec_nine[] = {7, 7, 7, 0, 7, 0, 0, 0, 0, 0, 0, 3, 0, 3, 7, 3};
static const uint8_t vec_a[] = {0, 7, 0, 2, 0, 2, 4, 0, 4, 0, 7, 7, 1, 4, 6, 4};
static const uint8_t vec_b[] = {0, 0, 0, 7, 0, 0, 5, 0, 5, 0, 5, 3, 5, 3, 0, 3, 0, 3, 6, 3, 6, 3, 6, 7, 6, 7, 0, 7};
static const uint8_t vec_c[] = {7, 0, 0, 0, 0, 0, 0, 7, 0, 7, 7, 7};
static const uint8_t vec_d[] = {0, 0, 0, 7, 0, 0, 6, 0, 6, 0, 6, 7, 6, 7, 0, 7};
static const uint8_t vec_e[] = {7, 0, 0, 0, 0, 0, 0, 7, 0, 7, 7, 7, 0, 3, 5, 3};
static const uint8_t vec_f[] = {7, 0, 0, 0, 0, 0, 0, 7, 0, 3, 5, 3};
static const uint8_t vec_g[] = {7, 0, 0, 0, 0, 0, 0, 7, 0, 7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4};
static const uint8_t vec_h[] = {0, 0, 0, 7, 7, 0, 7, 7, 0, 3, 7, 3};
static const uint8_t vec_i[] = {3, 0, 3, 7};
static const uint8_t vec_j[] = {7, 0, 7, 7, 7, 7, 2, 7, 2, 7, 0, 5};
static const uint8_t vec_k[] = {0, 0, 0, 7, 7, 0, 0, 4, 0, 4, 7, 7};
static const uint8_t vec_l[] = {0, 0, 0, 7, 0, 7, 7, 7};
static const uint8_t vec_m[] = {0, 7, 0, 0, 0, 0, 3, 4, 3, 4, 6, 0, 6, 0, 6, 7};
static const uint8_t vec_n[] = {0, 7, 0, 0, 0, 0, 7, 7, 7, 7, 7, 0};
static const uint8_t vec_o[] = {0, 0, 7, 0, 7, 0, 7, 7, 7, 7, 0, 7, 0, 7, 0, 0};
static const uint8_t vec_p[] = {0, 7, 0, 0, 0, 0, 7, 0, 7, 0, 7, 4, 7, 4, 0, 4};
static const uint8_t vec_q[] = {0, 0, 7, 0, 7, 0, 7, 7, 7, 7, 0, 7, 0, 7, 0, 0, 4, 4, 7, 7};
static const uint8_t vec_r[] = {0, 7, 0, 0, 0, 0, 7, 0, 7, 0, 7, 4, 7, 4, 0, 4, 0, 4, 7, 7};
static const uint8_t vec_s[] = {7, 0, 0, 0, 0, 0, 0, 4, 0, 4, 7, 4, 7, 4, 7, 7, 7, 7, 0, 7};
static const uint8_t vec_t[] = {0, 0, 7, 0, 3, 0, 3, 7};
static const uint8_t vec_u[] = {0, 0, 0, 7, 0, 7, 7, 7, 7, 7, 7, 0};
static const uint8_t vec_v[] = {0, 0, 3, 7, 3, 7, 7, 0};
static const uint8_t vec_w[] = {0, 0, 0, 7, 0, 7, 3, 4, 3, 4, 6, 7, 6, 7, 6, 0};
static const uint8_t vec_x[] = {0, 0, 7, 7, 7, 0, 0, 7};
static const uint8_t vec_y[] = {0, 0, 3, 4, 3, 4, 7, 0, 3, 4, 3, 7};
static const uint8_t vec_z[] = {0, 0, 7, 0, 7, 0, 0, 7, 0, 7, 7, 7};
static const uint8_t vec_box[] = {0, 0, 7, 0, 7, 0, 7, 7, 7, 7, 0, 7, 0, 7, 0, 0};

const bitmap_font_t* font_bitmap_get(void) {
    return &bitmap_font;
}

static vector_glyph_t vector_from_segments(const uint8_t* segments, uint32_t count) {
    vector_glyph_t glyph;
    glyph.segments = segments;
    glyph.count = count;
    glyph.width = 8;
    glyph.height = 8;
    return glyph;
}

vector_glyph_t font_vector_get(uint32_t codepoint) {
    switch (codepoint) {
        case '0': return vector_from_segments(vec_zero, 4);
        case '1': return vector_from_segments(vec_one, 1);
        case '2': return vector_from_segments(vec_two, 5);
        case '3': return vector_from_segments(vec_three, 4);
        case '4': return vector_from_segments(vec_four, 3);
        case '5': return vector_from_segments(vec_five, 5);
        case '6': return vector_from_segments(vec_six, 5);
        case '7': return vector_from_segments(vec_seven, 2);
        case '8': return vector_from_segments(vec_eight, 5);
        case '9': return vector_from_segments(vec_nine, 4);
        case 'A': case 'a': return vector_from_segments(vec_a, 4);
        case 'B': case 'b': return vector_from_segments(vec_b, 7);
        case 'C': case 'c': return vector_from_segments(vec_c, 3);
        case 'D': case 'd': return vector_from_segments(vec_d, 4);
        case 'E': case 'e': return vector_from_segments(vec_e, 4);
        case 'F': case 'f': return vector_from_segments(vec_f, 3);
        case 'G': case 'g': return vector_from_segments(vec_g, 5);
        case 'H': case 'h': return vector_from_segments(vec_h, 3);
        case 'I': case 'i': return vector_from_segments(vec_i, 1);
        case 'J': case 'j': return vector_from_segments(vec_j, 3);
        case 'K': case 'k': return vector_from_segments(vec_k, 3);
        case 'L': case 'l': return vector_from_segments(vec_l, 2);
        case 'M': case 'm': return vector_from_segments(vec_m, 4);
        case 'N': case 'n': return vector_from_segments(vec_n, 3);
        case 'O': case 'o': return vector_from_segments(vec_o, 4);
        case 'P': case 'p': return vector_from_segments(vec_p, 4);
        case 'Q': case 'q': return vector_from_segments(vec_q, 5);
        case 'R': case 'r': return vector_from_segments(vec_r, 5);
        case 'S': case 's': return vector_from_segments(vec_s, 5);
        case 'T': case 't': return vector_from_segments(vec_t, 2);
        case 'U': case 'u': return vector_from_segments(vec_u, 3);
        case 'V': case 'v': return vector_from_segments(vec_v, 2);
        case 'W': case 'w': return vector_from_segments(vec_w, 4);
        case 'X': case 'x': return vector_from_segments(vec_x, 2);
        case 'Y': case 'y': return vector_from_segments(vec_y, 3);
        case 'Z': case 'z': return vector_from_segments(vec_z, 3);
        default: return vector_from_segments(vec_box, 4);
    }
}

uint32_t utf8_decode(const char* text, uint32_t* index) {
    uint8_t c = (uint8_t)text[*index];
    if (c == 0) {
        return 0;
    }
    if (c < 0x80) {
        (*index)++;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        uint8_t c1 = (uint8_t)text[*index + 1];
        if ((c1 & 0xC0) != 0x80) {
            (*index)++;
            return '?';
        }
        (*index) += 2;
        return ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
    }
    if ((c & 0xF0) == 0xE0) {
        uint8_t c1 = (uint8_t)text[*index + 1];
        uint8_t c2 = (uint8_t)text[*index + 2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) {
            (*index)++;
            return '?';
        }
        (*index) += 3;
        return ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F);
    }
    if ((c & 0xF8) == 0xF0) {
        uint8_t c1 = (uint8_t)text[*index + 1];
        uint8_t c2 = (uint8_t)text[*index + 2];
        uint8_t c3 = (uint8_t)text[*index + 3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
            (*index)++;
            return '?';
        }
        (*index) += 4;
        return ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F);
    }
    (*index)++;
    return '?';
}
