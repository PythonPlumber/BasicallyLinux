#ifndef SECURE_AUDIT_H
#define SECURE_AUDIT_H

#include "types.h"

typedef struct {
    uint32_t count;
    uint32_t dropped;
} secure_audit_state_t;

typedef struct {
    uint32_t code;
    uint64_t tick;
} secure_audit_entry_t;

void secure_audit_init(void);
void secure_audit_log(uint32_t code);
secure_audit_state_t secure_audit_state(void);
uint32_t secure_audit_get(uint32_t index, secure_audit_entry_t* out);

#endif
