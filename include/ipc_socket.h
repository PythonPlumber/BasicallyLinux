#include "types.h"

#define IPC_SOCKET_MAX 16

typedef struct {
    uint32_t id;
    uint32_t connected;
} ipc_socket_t;

ipc_socket_t* ipc_socket_create(void);
int ipc_socket_connect(ipc_socket_t* socket, uint32_t peer_id);
int ipc_socket_send(ipc_socket_t* socket, const uint8_t* data, uint32_t size);
int ipc_socket_recv(ipc_socket_t* socket, uint8_t* data, uint32_t* size);
