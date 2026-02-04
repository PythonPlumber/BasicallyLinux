#include "diag.h"
#include "serial.h"
#include "util.h"

#define DIAG_LINES 64
#define DIAG_LINE_LEN 96

// Fixed-size ring buffer for recent diagnostics.
static char diag_lines[DIAG_LINES][DIAG_LINE_LEN];
static uint32_t diag_head = 0;
static uint32_t diag_count_value = 0;
static int diag_enabled = 1;

static const char* diag_level_name(diag_level_t level) {
    switch (level) {
        case DIAG_INFO:
            return "INFO";
        case DIAG_WARN:
            return "WARN";
        case DIAG_ERROR:
            return "ERROR";
        default:
            return "UNK";
    }
}

// Store a single line in the ring buffer.
static void diag_store_line(const char* text) {
    uint32_t pos = 0;
    while (text[pos] && pos + 1 < DIAG_LINE_LEN) {
        diag_lines[diag_head][pos] = text[pos];
        pos++;
    }
    diag_lines[diag_head][pos] = 0;
    diag_head = (diag_head + 1) % DIAG_LINES;
    if (diag_count_value < DIAG_LINES) {
        diag_count_value++;
    }
}

void diag_init(void) {
    diag_head = 0;
    diag_count_value = 0;
    diag_enabled = 1;
}

void diag_clear(void) {
    diag_head = 0;
    diag_count_value = 0;
}

void diag_log(diag_level_t level, const char* msg) {
    if (!diag_enabled) {
        return;
    }
    const char* level_name = diag_level_name(level);
    const char* text = msg ? msg : "(null)";
    char line[DIAG_LINE_LEN];
    uint32_t pos = 0;
    line[pos++] = '[';
    for (uint32_t i = 0; level_name[i] && pos + 1 < DIAG_LINE_LEN; ++i) {
        line[pos++] = level_name[i];
    }
    if (pos + 2 < DIAG_LINE_LEN) {
        line[pos++] = ']';
        line[pos++] = ' ';
    }
    for (uint32_t i = 0; text[i] && pos + 1 < DIAG_LINE_LEN; ++i) {
        line[pos++] = text[i];
    }
    line[pos] = 0;
    diag_store_line(line);
    serial_write(line);
    serial_write("\n");
}

void diag_log_hex32(diag_level_t level, const char* msg, uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char combined[DIAG_LINE_LEN];
    uint32_t pos = 0;
    if (msg) {
        for (uint32_t i = 0; msg[i] && pos + 1 < DIAG_LINE_LEN; ++i) {
            combined[pos++] = msg[i];
        }
        if (pos + 1 < DIAG_LINE_LEN) {
            combined[pos++] = ' ';
        }
    }
    if (pos + 2 < DIAG_LINE_LEN) {
        combined[pos++] = '0';
        combined[pos++] = 'x';
    }
    for (int i = 7; i >= 0 && pos + 1 < DIAG_LINE_LEN; --i) {
        uint8_t nibble = (uint8_t)((value >> (i * 4)) & 0xF);
        combined[pos++] = hex[nibble];
    }
    combined[pos] = 0;
    diag_log(level, combined);
}

uint32_t diag_count(void) {
    return diag_count_value;
}

const char* diag_get(uint32_t index) {
    static const char empty[] = "";
    if (index >= diag_count_value) {
        return empty;
    }
    uint32_t start = (diag_head + DIAG_LINES - diag_count_value) % DIAG_LINES;
    uint32_t slot = (start + index) % DIAG_LINES;
    return diag_lines[slot];
}

void diag_set_enabled(int enabled) {
    diag_enabled = enabled ? 1 : 0;
}

int diag_is_enabled(void) {
    return diag_enabled;
}
