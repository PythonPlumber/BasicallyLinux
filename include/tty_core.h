#include "types.h"

typedef struct {
    uint32_t columns;
    uint32_t rows;
} tty_state_t;

void tty_core_init(void);
tty_state_t* tty_core_state(void);
