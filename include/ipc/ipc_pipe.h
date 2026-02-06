#ifndef IPC_PIPE_H
#define IPC_PIPE_H

#include "types.h"

typedef struct {
    uint8_t* buffer;
    uint32_t size;
    uint32_t read_pos;
    uint32_t write_pos;
} ipc_pipe_t;

ipc_pipe_t* ipc_pipe_create(uint32_t size);
uint32_t ipc_pipe_write(ipc_pipe_t* pipe, const uint8_t* data, uint32_t size);
uint32_t ipc_pipe_read(ipc_pipe_t* pipe, uint8_t* data, uint32_t size);

#endif
