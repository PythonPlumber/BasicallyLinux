#include "trace_forge.h"
#include "timer.h"
#include "types.h"

static trace_forge_state_t state;
static trace_event_t events[256];
static uint32_t head = 0;
static uint32_t count = 0;

void trace_forge_init(void) {
    state.events = 0;
    state.dropped = 0;
    head = 0;
    count = 0;
    for (uint32_t i = 0; i < 256; ++i) {
        events[i].tag = 0;
        events[i].value = 0;
        events[i].tick = 0;
    }
}

void trace_forge_emit(uint32_t tag, uint32_t value) {
    state.events++;
    events[head].tag = tag;
    events[head].value = value;
    events[head].tick = timer_get_ticks();
    head = (head + 1) % 256;
    if (count < 256) {
        count++;
    } else {
        state.dropped++;
    }
}

trace_forge_state_t trace_forge_state(void) {
    return state;
}

uint32_t trace_forge_get(uint32_t index, trace_event_t* out) {
    if (!out || index >= count) {
        return 0;
    }
    uint32_t start = (head + 256 - count) % 256;
    uint32_t slot = (start + index) % 256;
    *out = events[slot];
    return 1;
}
