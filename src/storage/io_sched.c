#include "storage/io_sched.h"
#include "types.h"

#define IO_SCHED_MAX 16

static io_request_t* queue[IO_SCHED_MAX];
static uint32_t queue_head = 0;
static uint32_t queue_tail = 0;
static uint32_t queue_count = 0;

void io_sched_init(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}

int io_sched_queue(io_request_t* req) {
    if (!req || queue_count >= IO_SCHED_MAX) {
        return 0;
    }
    queue[queue_tail] = req;
    queue_tail = (queue_tail + 1) % IO_SCHED_MAX;
    queue_count++;
    return 1;
}

void io_sched_run(void) {
    while (queue_count > 0) {
        io_request_t* req = queue[queue_head];
        queue_head = (queue_head + 1) % IO_SCHED_MAX;
        queue_count--;
        if (!req || !req->dev) {
            continue;
        }
        if (req->write) {
            block_write(req->dev, req->lba, req->count, req->buffer);
        } else {
            block_read(req->dev, req->lba, req->count, req->buffer);
        }
    }
}
