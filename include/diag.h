#ifndef DIAG_H
#define DIAG_H

#include "types.h"

typedef enum {
    DIAG_INFO = 0,
    DIAG_WARN = 1,
    DIAG_ERROR = 2
} diag_level_t;

void diag_init(void);
void diag_clear(void);
void diag_log(diag_level_t level, const char* msg);
void diag_log_hex32(diag_level_t level, const char* msg, uint32_t value);
uint32_t diag_count(void);
const char* diag_get(uint32_t index);
void diag_set_enabled(int enabled);
int diag_is_enabled(void);

#endif
