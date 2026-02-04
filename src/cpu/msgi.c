#include "msgi.h"
#include "types.h"

void msgi_init(void) {
}

int msgi_send(uint32_t vector, uint32_t cpu) {
    (void)vector;
    (void)cpu;
    return 1;
}
