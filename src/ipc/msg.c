#include "ipc/ipc_msg.h"
#include "types.h"
#include "util.h"

void ipc_msg_init(ipc_msg_queue_t* queue) {
    if (!queue) {
        return;
    }
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

int ipc_msg_send(ipc_msg_queue_t* queue, const uint8_t* data, uint32_t size) {
    if (!queue || !data || size == 0) {
        return 0;
    }
    if (size > IPC_MSG_DATA || queue->count >= IPC_MSG_MAX) {
        return 0;
    }
    ipc_message_t* msg = &queue->queue[queue->tail];
    msg->size = size;
    memcpy(msg->data, data, size);
    queue->tail = (queue->tail + 1) % IPC_MSG_MAX;
    queue->count++;
    return 1;
}

int ipc_msg_recv(ipc_msg_queue_t* queue, uint8_t* data, uint32_t* size) {
    if (!queue || !data || !size) {
        return 0;
    }
    if (queue->count == 0) {
        return 0;
    }
    ipc_message_t* msg = &queue->queue[queue->head];
    memcpy(data, msg->data, msg->size);
    *size = msg->size;
    queue->head = (queue->head + 1) % IPC_MSG_MAX;
    queue->count--;
    return 1;
}
