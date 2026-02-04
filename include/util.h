#ifndef UTIL_H
#define UTIL_H

#include "types.h"

unsigned int strlen(const char* s);
void* memcpy(void* dest, const void* src, unsigned int n);
void* memset(void* dest, int value, unsigned int n);
int memcmp(const void* a, const void* b, unsigned int n);

void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);

#endif

