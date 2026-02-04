#include "ipc_shm.h"
#include "paging.h"
#include "process.h"
#include "types.h"

int ipc_shm_create(uint32_t pages) {
    return vm_create_shared(pages);
}

int ipc_shm_map(process_t* proc, uint32_t shm_id, uint32_t start) {
    return vm_map_shared(proc, shm_id, start);
}
