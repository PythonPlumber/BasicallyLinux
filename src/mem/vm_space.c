#include "vm_space.h"
#include "paging.h"
#include "process.h"
#include "types.h"

static int vm_space_add_region(process_t* proc, const vm_region_t* region) {
    if (!proc || !region) {
        return 0;
    }
    if (proc->region_count >= PROCESS_MAX_REGIONS) {
        return 0;
    }
    proc->regions[proc->region_count++] = *region;
    return 1;
}

uint32_t* vm_space_create(void) {
    return paging_create_directory();
}

int vm_space_clone_cow(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return 0;
    }
    if (!child->page_directory) {
        child->page_directory = paging_create_directory();
        if (!child->page_directory) {
            return 0;
        }
    }
    child->region_count = 0;
    for (uint32_t i = 0; i < parent->region_count; ++i) {
        vm_region_t region = parent->regions[i];
        uint32_t size = region.end - region.start;
        (void)size;
        if (region.flags & VM_SHARED) {
            if (!vm_map_shared(child, region.shared_id, region.start)) {
                return 0;
            }
            continue;
        }
        if (region.flags & VM_WRITE) {
            region.flags = (region.flags | VM_COW) & ~VM_WRITE;
            parent->regions[i].flags = region.flags;
        }
        if (!vm_space_add_region(child, &region)) {
            return 0;
        }
        if (!(region.flags & VM_GUARD) && !(region.flags & VM_DEMAND)) {
            uint32_t* parent_dir = parent->page_directory ? parent->page_directory : paging_get_directory();
            if (!paging_clone_cow_range(parent_dir, child->page_directory, region.start, region.end)) {
                return 0;
            }
        }
        if (region.flags & VM_DEMAND) {
            continue;
        }
    }
    return 1;
}

int vm_map_user_guarded(process_t* proc, uint32_t start, uint32_t size, uint32_t flags, uint32_t guard_size) {
    if (!proc || size == 0 || guard_size == 0) {
        return 0;
    }
    if (start < guard_size) {
        return 0;
    }
    uint32_t guard_before = start - guard_size;
    uint32_t guard_after = start + size;
    if (!vm_map_region(proc, guard_before, guard_size, VM_GUARD | VM_USER)) {
        return 0;
    }
    if (!vm_map_region(proc, start, size, flags | VM_USER)) {
        return 0;
    }
    if (!vm_map_region(proc, guard_after, guard_size, VM_GUARD | VM_USER)) {
        return 0;
    }
    return 1;
}
