#include "heap.h"
#include "ipc_pipe.h"
#include "types.h"

ipc_pipe_t* ipc_pipe_create(uint32_t size) {
    if (size == 0) {
        return 0;
    }
    ipc_pipe_t* pipe = (ipc_pipe_t*)kmalloc(sizeof(ipc_pipe_t));
    if (!pipe) {
        return 0;
    }
    pipe->buffer = (uint8_t*)kmalloc(size);
    if (!pipe->buffer) {
        kfree(pipe);
        return 0;
    }
    pipe->size = size;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    return pipe;
}

uint32_t ipc_pipe_write(ipc_pipe_t* pipe, const uint8_t* data, uint32_t size) {
    if (!pipe || !data || size == 0) {
        return 0;
    }
    uint32_t written = 0;
    while (written < size) {
        uint32_t next = (pipe->write_pos + 1) % pipe->size;
        if (next == pipe->read_pos) {
            break;
        }
        pipe->buffer[pipe->write_pos] = data[written++];
        pipe->write_pos = next;
    }
    return written;
}

uint32_t ipc_pipe_read(ipc_pipe_t* pipe, uint8_t* data, uint32_t size) {
    if (!pipe || !data || size == 0) {
        return 0;
    }
    uint32_t read = 0;
    while (read < size && pipe->read_pos != pipe->write_pos) {
        data[read++] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % pipe->size;
    }
    return read;
}
