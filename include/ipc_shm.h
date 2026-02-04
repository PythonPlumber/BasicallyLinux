#ifndef IPC_SHM_H
#define IPC_SHM_H

#include "process.h"
#include "types.h"

int ipc_shm_create(uint32_t pages);
int ipc_shm_map(process_t* proc, uint32_t shm_id, uint32_t start);

#endif
