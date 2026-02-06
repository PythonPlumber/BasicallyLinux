#ifndef VM_SPACE_H
#define VM_SPACE_H

#include "paging.h"
#include "process.h"
#include "types.h"

uint32_t* vm_space_create(void);
int vm_space_clone_cow(process_t* parent, process_t* child);
int vm_map_user_guarded(process_t* proc, uint32_t start, uint32_t size, uint32_t flags, uint32_t guard_size);

#endif

