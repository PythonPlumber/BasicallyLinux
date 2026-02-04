#include "types.h"

typedef struct {
    uint32_t mode;
    uint32_t enforced;
    uint32_t rules;
} secure_policy_state_t;

void secure_policy_init(void);
void secure_policy_set(uint32_t mode, uint32_t enforced);
secure_policy_state_t secure_policy_state(void);
uint32_t secure_policy_add_rule(uint32_t subject, uint32_t object, uint32_t action, uint32_t allow);
uint32_t secure_policy_check(uint32_t subject, uint32_t object, uint32_t action);

#define SECURE_ACTION_READ 1u
#define SECURE_ACTION_WRITE 2u
#define SECURE_ACTION_EXEC 4u
#define SECURE_ACTION_NET 8u
#define SECURE_ACTION_ADMIN 16u
