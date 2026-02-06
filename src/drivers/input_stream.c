#include "drivers/input_stream.h"
#include "types.h"

#define INPUT_STREAM_MAX 64

static input_event_t queue[INPUT_STREAM_MAX];
static uint32_t head = 0;
static uint32_t tail = 0;
static uint32_t count = 0;

void input_stream_init(void) {
    head = 0;
    tail = 0;
    count = 0;
}

int input_stream_push(const input_event_t* ev) {
    if (!ev || count >= INPUT_STREAM_MAX) {
        return 0;
    }
    queue[tail] = *ev;
    tail = (tail + 1) % INPUT_STREAM_MAX;
    count++;
    return 1;
}

int input_stream_pop(input_event_t* ev) {
    if (!ev || count == 0) {
        return 0;
    }
    *ev = queue[head];
    head = (head + 1) % INPUT_STREAM_MAX;
    count--;
    return 1;
}
