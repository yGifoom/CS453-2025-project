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
bool lock_check_version(version_lock* lk, int own_vl);
void lock_update_and_release(version_lock* lk, int updated_version);

