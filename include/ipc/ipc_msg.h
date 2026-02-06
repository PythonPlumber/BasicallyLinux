#ifndef IPC_MSG_H
#define IPC_MSG_H

#include "types.h"

#define IPC_MSG_MAX 16
#define IPC_MSG_DATA 64

typedef struct {
    uint32_t size;
    uint8_t data[IPC_MSG_DATA];
} ipc_message_t;

typedef struct {
    ipc_message_t queue[IPC_MSG_MAX];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} ipc_msg_queue_t;

void ipc_msg_init(ipc_msg_queue_t* queue);
int ipc_msg_send(ipc_msg_queue_t* queue, const uint8_t* data, uint32_t size);
int ipc_msg_recv(ipc_msg_queue_t* queue, uint8_t* data, uint32_t* size);

#endif
