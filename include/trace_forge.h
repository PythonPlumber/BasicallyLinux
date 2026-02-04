#ifndef TRACE_FORGE_H
#define TRACE_FORGE_H

#include "types.h"

typedef struct {
    uint32_t events;
    uint32_t dropped;
} trace_forge_state_t;

typedef struct {
    uint32_t tag;
    uint32_t value;
    uint64_t tick;
} trace_event_t;

void trace_forge_init(void);
void trace_forge_emit(uint32_t tag, uint32_t value);
trace_forge_state_t trace_forge_state(void);
uint32_t trace_forge_get(uint32_t index, trace_event_t* out);

#endif

