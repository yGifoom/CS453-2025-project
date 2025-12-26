#include <macros.h>
#include <shared_t.h>
#include <params.h>
#include <stdbool.h>
#include <utils.h>
#include <stdio.h>
#include <dict.h>
#include <tx_t.h>

int nested_free_value_dict(void unused(*key), int unused(count), void* *value, void unused(*user)){
    if(*value != NULL){
        free(*value);
    }
    return 1;
}


int add_from_dict(void *key, int unused(count), void* *value, void *user){
    struct dictionary* dict = (struct dictionary*)user;

    int res_add = dic_add(dict, key, 8);
    if(res_add != 1){
        *dict->value = *value;
    }
    return 1;
}

int rm_from_dict(void *key, int unused(count), void* unused(*value), void *user){
    struct dictionary* dict = (struct dictionary*)user;
    
    if(dic_find(dict, key, 8) == 1){
        dic_add(dict, key, 8);
        free(*dict->value);
        free(key);
        *dict->value = NULL;
    }
    return 1;
}

int unique_lock_create(void *key, int unused(count), void* *unused(value), void *user){
    region_and_index* ri = (region_and_index*)user;
    struct dictionary* unique_lock_set = ri->key;
    version_lock* lock = lock_get_from_pointer(ri->region, key);

    dic_add(unique_lock_set, lock, 8);

    return 1;
}


// lock up all words in the write set, can fail!
int lock_unique_lock_set(void *key, int unused(count), void* *unused(value), void *user){
    region_and_index* ri = (region_and_index*)user;
    version_lock* lock = key;
    int res_lock = lock_try_acquire(lock);
    
    if(!res_lock){
        ri->key = lock;
        //printf("LOCK_WRITE_SET: failed lock on %p\n", lock);fflush(stdout);
        return res_lock;
    }
    //printf("LOCK_WRITE_SET: locked %p\n", lock);fflush(stdout);
    return res_lock;
}

// unlocks write set words until a match with the key in ri->key is found
int unlock_unique_lock_set_until(void *key, int unused(count), void* *unused(value), void *user){
    region_and_index* ri = (region_and_index*)user;
    version_lock* lock = key;
    
    if (ri->key == lock) return 0; // if there is a stopping key release until a match is found
        
    lock_release(lock);

    return 1;
}

// in the ri the writing set is passed. all the keys that appear in the writing and reading set 
// have to be transated to locks, checked for VERSION ONLY and subsiquently removed from reading set.
// the keys appearing only in the reading set have to check both version and lock
int validate_reading_set(void *key, int unused(count), void* *unused(value), void *user){
    region_and_index* ri = (region_and_index*)user;
    struct dictionary* ws = (struct dictionary*)ri->key;

    bool res = false;

    version_lock* lock = lock_get_from_pointer(ri->region, key);
    if(dic_find(ws, key, 8)){
        // key of rs is in ws, has already been locked
        res = lock_check_version(lock, ri->transaction->read_version);

    } else {
        res = lock_check(lock, ri->transaction->read_version);
    }

    if (res == false){
        //printf("VALIDATE READING SET: failed on key:%p, res:%d, with lock at:%p with value:%d and rv:%d\n", key, res, lock, *lock, ri->transaction->read_version);fflush(stdout);
        ri->transaction = NULL;
        return 0;
    }
    //printf("VALIDATE READING SET: ok on key:%p, res:%d, with lock:%d and rv:%d\n", key, res, *lock, ri->transaction->read_version);fflush(stdout);

    return 1;
}

// write data in shared mem
int write_writing_set(void *key, int unused(count), void* *value, void *user){
    region_and_index* ri = (region_and_index*)user;
    
    // value is a void** pointing to the dictionary's value slot
    // *value is the void* pointer to the allocated buffer with actual data
    memcpy(key, *value, ri->region->align);

    return 1;
}

// updates all locks from writing set and unlocks them
int update_unique_lock_set(void *key, int unused(count), void* *unused(value), void *user){
    region_and_index* ri = (region_and_index*)user;
    
    version_lock* lock = key;
    
    lock_update_and_release(lock, ri->transaction->write_version);

    return 1;
}


void dic_nested_destroy(struct dictionary* dic){
    dic_forEach(dic, nested_free_value_dict, NULL);
    dic_delete(dic);
}

void tx_destroy(transaction_t* tx, bool committed){
    dic_nested_destroy(tx->write_set);
    dic_delete(tx->read_set);

    if (!committed){
        ll_destroy_nested(tx->alloc_set, free);
    }else{
        ll_destroy(tx->alloc_set);
    }
    
    free(tx);
    return;
}

static inline uint32_t hash_pointer(void *ptr) {
	////printf("key is %p\n", ptr);fflush(stdout);
	// Shift out alignment bits to get meaningful variation
	uintptr_t val = (uintptr_t)ptr >> 3;
	// Mix the bits to distribute values better in smaller ranges
	val ^= val >> 16;
	val *= 0x85ebca6b;
	val ^= val >> 13;
	val *= 0xc2b2ae35;
	val ^= val >> 16;
	////printf("key %p hashed to value %u\n", ptr, (uint32_t)val);fflush(stdout);
	return (uint32_t)val;
}

version_lock* lock_get_from_pointer(shared_rgn* shared, void* ptr){
    uint32_t lock_array_idx = hash_pointer(ptr);

    return &shared->locks[lock_array_idx % LOCK_ARRAY_SIZE];
}