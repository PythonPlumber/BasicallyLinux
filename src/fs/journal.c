#include "journal.h"
#include "types.h"

static uint32_t next_tx_id = 1;

void journal_init(void) {
    next_tx_id = 1;
}

journal_tx_t journal_begin(void) {
    journal_tx_t tx;
    tx.id = next_tx_id++;
    tx.active = 1;
    return tx;
}

void journal_commit(journal_tx_t tx) {
    (void)tx;
}
