#ifndef INPUT_STREAM_H
#define INPUT_STREAM_H

#include "types.h"

typedef struct {
    uint32_t type;
    uint32_t code;
    uint32_t value;
} input_event_t;

void input_stream_init(void);
int input_stream_push(const input_event_t* ev);
int input_stream_pop(input_event_t* ev);

#endif
