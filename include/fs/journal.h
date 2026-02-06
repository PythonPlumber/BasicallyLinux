#ifndef JOURNAL_H
#define JOURNAL_H

#include "types.h"

typedef struct {
    uint32_t id;
    uint32_t active;
} journal_tx_t;

void journal_init(void);
journal_tx_t journal_begin(void);
void journal_commit(journal_tx_t tx);

#endif
