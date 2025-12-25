#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

typedef _Atomic int version_lock;  // 4B
typedef _Atomic int global_counter; // 4B

bool lock_try_acquire(version_lock*);
void lock_acquire(version_lock*);
void lock_release(version_lock*);
bool lock_check(version_lock*, int);

