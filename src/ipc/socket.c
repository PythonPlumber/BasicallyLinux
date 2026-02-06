#include "ipc/ipc_socket.h"
#include "types.h"

static ipc_socket_t sockets[IPC_SOCKET_MAX];
static uint32_t socket_count = 0;

ipc_socket_t* ipc_socket_create(void) {
    if (socket_count >= IPC_SOCKET_MAX) {
        return 0;
    }
    ipc_socket_t* sock = &sockets[socket_count];
    sock->id = socket_count;
    sock->connected = 0;
    socket_count++;
    return sock;
}

int ipc_socket_connect(ipc_socket_t* socket, uint32_t peer_id) {
    if (!socket) {
        return 0;
    }
    if (peer_id >= socket_count) {
        return 0;
    }
    socket->connected = peer_id + 1;
    return 1;
}

int ipc_socket_send(ipc_socket_t* socket, const uint8_t* data, uint32_t size) {
    if (!socket || !data || size == 0) {
        return 0;
    }
    if (socket->connected == 0) {
        return 0;
    }
    return 1;
}

int ipc_socket_recv(ipc_socket_t* socket, uint8_t* data, uint32_t* size) {
    if (!socket || !data || !size) {
        return 0;
    }
    return 0;
}
