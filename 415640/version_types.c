#include "version_types.h"
#include <stdlib.h>
#include <stdio.h>

bool lock_try_acquire(version_lock* lk){
    printf("try_lock_acquire pointer:%p\n", lk);
    int vl = atomic_load(lk);

    if (vl & 0x1)
    {
        return false;
    }

    return atomic_compare_exchange_strong(lk, &vl, vl | 0x1);
}

void lock_acquire(version_lock* lk){
    int vl = 0;
    do{
        vl = 0;
    }
    while(!atomic_compare_exchange_weak(lk, &vl, vl | 0x1))
    ; // blocked until acquire lock
    return;
}

// should only be called on locks owned by caller
void lock_release(version_lock* lk){
    atomic_fetch_sub(lk, 1);
    return;
}


bool lock_check(version_lock* lk, int own_vl){
    int vl = atomic_load(lk);
    
    if (vl & 0x1 || (vl >> 1) > (own_vl >> 1)){
        return false;
    }
    return true;
}

bool lock_check_version(version_lock* lk, int own_vl){
    int vl = atomic_load(lk);
    
    if ((vl >> 1) > (own_vl >> 1)){
        return false;
    }
    return true;
}

void lock_update_and_release(version_lock* lk, int updated_version){
    atomic_store(lk, (updated_version << 1)); // inserts automatically a 0 to reset lock
}


