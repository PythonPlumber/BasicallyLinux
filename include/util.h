#ifndef UTIL_H
#define UTIL_H

#include "types.h"

size_t strlen(const char* s);
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int value, size_t n);
int memcmp(const void* a, const void* b, size_t n);
int strcmp(const char* a, const char* b);
char* strncpy(char* dest, const char* src, size_t n);

void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);

uint32_t spin_lock_irqsave(spinlock_t* lock);
void spin_unlock_irqrestore(spinlock_t* lock, uint32_t flags);

#endif

