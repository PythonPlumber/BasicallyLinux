#ifndef PTY_MUX_H
#define PTY_MUX_H

#include "types.h"

typedef struct {
    uint32_t count;
} pty_mux_state_t;

void pty_mux_init(void);
pty_mux_state_t pty_mux_state(void);

#endif
