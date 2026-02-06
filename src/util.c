#include "util.h"
#include "types.h"

unsigned int strlen(const char* s) {
    unsigned int n = 0;
    while (s[n] != 0) {
        ++n;
    }
    return n;
}

void* memcpy(void* dest, const void* src, unsigned int n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (unsigned int i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* dest, int value, unsigned int n) {
    uint8_t* d = (uint8_t*)dest;
    uint8_t v = (uint8_t)value;
    for (unsigned int i = 0; i < n; ++i) {
        d[i] = v;
    }
    return dest;
}

int memcmp(const void* a, const void* b, unsigned int n) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (unsigned int i = 0; i < n; ++i) {
        if (x[i] != y[i]) {
            return (int)x[i] - (int)y[i];
        }
    }
    return 0;
}

int strcmp(const char* a, const char* b) {
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return (int)(uint8_t)a[i] - (int)(uint8_t)b[i];
        }
        i++;
    }
    return (int)(uint8_t)a[i] - (int)(uint8_t)b[i];
}

char* strncpy(char* dest, const char* src, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

void spin_lock(spinlock_t* lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile("pause");
    }
}

void spin_unlock(spinlock_t* lock) {
    __sync_lock_release(lock);
}

uint32_t spin_lock_irqsave(spinlock_t* lock) {
    uint32_t flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    spin_lock(lock);
    return flags;
}

void spin_unlock_irqrestore(spinlock_t* lock, uint32_t flags) {
    spin_unlock(lock);
    asm volatile("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}
