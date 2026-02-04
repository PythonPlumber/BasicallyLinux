#include "types.h"

#define SECURE_CAP_MAX 32
#define SECURE_CAP_SYS_ADMIN 0u
#define SECURE_CAP_SYS_TIME 1u
#define SECURE_CAP_NET_ADMIN 2u
#define SECURE_CAP_FS_ADMIN 3u
#define SECURE_CAP_DEBUG 4u
#define SECURE_CAP_TRACE 5u
#define SECURE_CAP_POWER 6u
#define SECURE_CAP_NAMESPACE 7u
#define SECURE_CAP_VIRT 8u

typedef struct {
    uint32_t set;
} secure_caps_t;

void secure_caps_init(void);
secure_caps_t secure_caps_from_mask(uint32_t mask);
uint32_t secure_caps_has(secure_caps_t caps, uint32_t cap);
secure_caps_t secure_caps_add(secure_caps_t caps, uint32_t cap);
secure_caps_t secure_caps_remove(secure_caps_t caps, uint32_t cap);
