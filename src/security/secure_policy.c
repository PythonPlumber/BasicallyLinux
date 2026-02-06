#include "security/secure_policy.h"
#include "types.h"

typedef struct {
    uint32_t subject;
    uint32_t object;
    uint32_t action;
    uint32_t allow;
} secure_rule_t;

typedef struct {
    uint32_t subject;
    char pattern[256];
    uint32_t action;
    uint32_t allow;
} secure_path_rule_t;

#define SECURE_RULE_MAX 64
#define SECURE_PATH_RULE_MAX 32
#define GLOB_MAX_RECURSION 16

static secure_policy_state_t state;
static secure_rule_t rules[SECURE_RULE_MAX];
static secure_path_rule_t path_rules[SECURE_PATH_RULE_MAX];
static uint32_t path_rule_count = 0;

static int glob_match_recursive(const char* pattern, const char* string, int depth) {
    if (depth > GLOB_MAX_RECURSION) return 0;
    while (*pattern) {
        if (*pattern == '*') {
            while (*pattern == '*') pattern++;
            if (!*pattern) return 1;
            while (*string) {
                if (glob_match_recursive(pattern, string, depth + 1)) return 1;
                string++;
            }
            return 0;
        } else if (*pattern == '?') {
            if (!*string) return 0;
            pattern++;
            string++;
        } else {
            if (*pattern != *string) return 0;
            pattern++;
            string++;
        }
    }
    return !*string;
}

static int glob_match(const char* pattern, const char* string) {
    return glob_match_recursive(pattern, string, 0);
}

static void strncpy(char* dest, const char* src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        dest[i] = src[i];
        if (!src[i]) break;
    }
    dest[n - 1] = 0;
}

void secure_policy_init(void) {
    state.mode = 0;
    state.enforced = 0;
    state.rules = 0;
    path_rule_count = 0;
    for (uint32_t i = 0; i < SECURE_RULE_MAX; ++i) {
        rules[i].subject = 0;
        rules[i].object = 0;
        rules[i].action = 0;
        rules[i].allow = 0;
    }
    for (uint32_t i = 0; i < SECURE_PATH_RULE_MAX; ++i) {
        path_rules[i].subject = 0;
        path_rules[i].pattern[0] = 0;
        path_rules[i].action = 0;
        path_rules[i].allow = 0;
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

uint32_t secure_policy_add_rule_path(uint32_t subject, const char* pattern, uint32_t action, uint32_t allow) {
    if (path_rule_count >= SECURE_PATH_RULE_MAX) {
        return 0;
    }
    path_rules[path_rule_count].subject = subject;
    strncpy(path_rules[path_rule_count].pattern, pattern, 256);
    path_rules[path_rule_count].action = action;
    path_rules[path_rule_count].allow = allow ? 1u : 0u;
    path_rule_count++;
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

uint32_t secure_policy_check_path(uint32_t subject, const char* path, uint32_t action) {
    if (!state.enforced) {
        return 1;
    }
    uint32_t allow = 0;
    for (uint32_t i = 0; i < path_rule_count; ++i) {
        if (path_rules[i].subject == subject && (path_rules[i].action & action)) {
            if (glob_match(path_rules[i].pattern, path)) {
                allow = path_rules[i].allow;
            }
        }
    }
    return allow;
}
