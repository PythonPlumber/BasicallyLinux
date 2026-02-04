#include "secure_policy.h"
#include "types.h"

typedef struct {
    uint32_t subject;
    uint32_t object;
    uint32_t action;
    uint32_t allow;
} secure_rule_t;

#define SECURE_RULE_MAX 64

static secure_policy_state_t state;
static secure_rule_t rules[SECURE_RULE_MAX];

void secure_policy_init(void) {
    state.mode = 0;
    state.enforced = 0;
    state.rules = 0;
    for (uint32_t i = 0; i < SECURE_RULE_MAX; ++i) {
        rules[i].subject = 0;
        rules[i].object = 0;
        rules[i].action = 0;
        rules[i].allow = 0;
    }
}

void secure_policy_set(uint32_t mode, uint32_t enforced) {
    state.mode = mode;
    state.enforced = enforced;
}

secure_policy_state_t secure_policy_state(void) {
    return state;
}

uint32_t secure_policy_add_rule(uint32_t subject, uint32_t object, uint32_t action, uint32_t allow) {
    if (state.rules >= SECURE_RULE_MAX) {
        return 0;
    }
    rules[state.rules].subject = subject;
    rules[state.rules].object = object;
    rules[state.rules].action = action;
    rules[state.rules].allow = allow ? 1u : 0u;
    state.rules++;
    return 1;
}

uint32_t secure_policy_check(uint32_t subject, uint32_t object, uint32_t action) {
    if (!state.enforced) {
        return 1;
    }
    uint32_t allow = 0;
    for (uint32_t i = 0; i < state.rules; ++i) {
        if (rules[i].subject == subject && rules[i].object == object && (rules[i].action & action)) {
            allow = rules[i].allow;
        }
    }
    return allow;
}
