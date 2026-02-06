#ifndef IO_SCHED_H
#define IO_SCHED_H

#include "storage/block.h"
#include "types.h"

typedef struct {
    block_device_t* dev;
    uint32_t lba;
    uint32_t count;
    uint8_t* buffer;
    uint32_t write;
} io_request_t;

void io_sched_init(void);
int io_sched_queue(io_request_t* req);
void io_sched_run(void);

#endif
