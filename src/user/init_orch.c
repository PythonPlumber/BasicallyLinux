#include "user/init_orch.h"
#include "types.h"

static void (*init_entry)(void) = 0;

void init_orch_init(void) {
    init_entry = 0;
}

int init_orch_start(void (*entry)(void)) {
    if (!entry) {
        return 0;
    }
    init_entry = entry;
    return 1;
}
