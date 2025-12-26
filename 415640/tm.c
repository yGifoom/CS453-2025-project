/**
 * @file   tm.c
 * @author [...]
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers
#include "stdio.h"
// Internal headers
#include <tm.h>
#include <utils.h>          // nested free, add/rm_from_dict, get_lock_pointer
#include <tx_t.h>           // transaction struct
#include <string.h>         // (memset)
#include <shared_t.h>       // shared memory region
#include <version_types.h>  // global and lock versioning
#include <dict.h>           // alloc/free-set
#include <ll.h>             // segment and read/write set
#include "macros.h"
#include "params.h"


/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {

    if (!((align > 0) && !(align & (align - 1)))){
        return invalid_shared;
    }
    if (size % align != 0 || (size >> 48) > 0){
        return invalid_shared;
    }


    // make linked list for segments
    ll_t* segments = malloc(sizeof(ll_t));
    if(!ll_init(segments)){
        return invalid_shared;
    }

    // creation of segment
    void* first_segment;
    int res_mem_align = posix_memalign(&first_segment, align, size);

    if (res_mem_align != 0){
        free(segments);
        return invalid_shared;
    }
    memset(first_segment, 0, size);

    shared_rgn* shared_region = malloc(sizeof(shared_rgn));
    if (shared_region == NULL){
        free(first_segment);
        free(segments);
        return invalid_shared;
    }
    

    version_lock* locks = malloc(sizeof(version_lock)*LOCK_ARRAY_SIZE);
    if (locks == NULL){
        free(shared_region);
        free(first_segment);
        free(segments);
        return invalid_shared;
    }
    // initialize locks to 0
    memset(locks, 0, sizeof(version_lock)*LOCK_ARRAY_SIZE);
    ll_append(segments, first_segment);


    shared_region->global_version = 0;
    shared_region->start = first_segment;
    shared_region->size = size;
    shared_region->align = align;

    shared_region->segments = segments;
    shared_region->locks = locks;
    
    return shared_region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    shared_rgn* shared_region = (shared_rgn*)shared;

    // free each segment + each lock array + destroy dict itself
    ll_destroy_nested(shared_region->segments, free);
    free(shared_region->locks);
    free(shared_region);

    return;
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    return ((shared_rgn*)shared)->start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) {
    return ((shared_rgn*)shared)->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) {
    return ((shared_rgn*)shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) {
    shared_rgn* shared_region = (shared_rgn*)shared;

    transaction_t* tx = malloc(sizeof(transaction_t));
    if (tx == NULL){
        return invalid_tx;
    }

    tx->read_version = atomic_load(&shared_region->global_version);
    tx->write_version = 0;

    tx->read_only = is_ro;
    tx->read_set = dic_new(0);
    tx->write_set = dic_new(0);
    tx->alloc_set = malloc(sizeof(struct ll));
    ll_init(tx->alloc_set);

    return (tx_t)tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    shared_rgn* shared_region = (shared_rgn*)shared;
    transaction_t* transaction = (transaction_t*)tx;

    if(transaction->read_only){
        tx_destroy(transaction, true);
        return true;
    }

    // creation of support struct
    region_and_index* ri = malloc(sizeof(region_and_index));
    if (ri == NULL){
        tx_destroy(transaction, false);
        return false;
    }
    ri->region = shared_region;
    ri->transaction = transaction;
    ri->key = NULL;

    // lock the write set 
    dic_forEach(transaction->write_set, lock_write_set, ri);

    // rollback in case locks were already acquired
    if(ri->key != NULL){
        printf("TM_END: TRANSACTION FAILED, failed to acquire all locks write set\n");fflush(stdout);
        dic_forEach(transaction->write_set, unlock_write_set_until, ri);
        free(ri);
        tx_destroy(transaction, false);
        return false;
    }
    // fetch and increment global counter    
    transaction->write_version = atomic_fetch_add(&shared_region->global_version, 1) + 1;

    if(transaction->write_version != transaction->read_version + 1){
        printf("TM_END: validating writing set: rv:%d, wv:%d\n", transaction->read_version, transaction->write_version);fflush(stdout);
        
        // validating reading set
        ri->key = transaction->write_set; // supply write_set to only check version of already locked words
        dic_forEach(transaction->read_set, validate_reading_set, ri);
        ri->key = NULL;

        if(ri->transaction == NULL){
            printf("TM_END: TRANSACTION FAILED, failed to validate reading set\n");fflush(stdout);
            dic_forEach(transaction->write_set, unlock_write_set_until, ri);
            free(ri);
            tx_destroy(transaction, false);
            return false;
        }
    }

    dic_forEach(transaction->write_set, write_writing_set, ri);         // write values 
    dic_forEach(transaction->write_set, update_locks_writing_set, ri);  // and releases all held locks

    ll_concat_safe(shared_region->segments, transaction->alloc_set);
    
    free(ri);
    tx_destroy(transaction, true);
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    shared_rgn* shared_region = (shared_rgn*)shared;
    transaction_t* transaction = (transaction_t*)tx;

    size_t word_size = shared_region->align;

    for(size_t i = 0; i < size/word_size; i ++){
        void* current_source_word = (void*)source+i*word_size;
        void* current_target_word = target+i*word_size;
        version_lock* current_version_lock = lock_get_from_pointer((shared_rgn*)shared, current_source_word);

        if(!lock_check(current_version_lock, transaction->read_version)){
            tx_destroy(transaction, false);
            return false;
        }

        if(!transaction->read_only){
            dic_add(transaction->read_set, current_source_word, 8);
            //printf("TM_READ: adding to read-set: %p\n", current_source_word);fflush(stdout);
            if(dic_find(transaction->write_set, current_source_word, 8)){
                //printf("TM_READ: reading from write-set: %p\n", current_source_word);fflush(stdout);
                current_source_word = *transaction->write_set->value;
            }
        }

        if(!lock_check(current_version_lock, transaction->read_version)){
            tx_destroy(transaction, false);
            return false;
        }

        memcpy(current_target_word, current_source_word, word_size);

    }

    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    //printf("TM_WRITE: SANITY: source: %p, with value: %ld, target:%p with value:%ld\n", source, *(long*)source, target, *(long*)target);fflush(stdout);
    size_t word_size = tm_align(shared);
    transaction_t* transaction = (transaction_t*)tx;

    if(size % word_size != 0){
        tx_destroy(transaction, false);
        return false;
    }
    
    void* starting_target_word = target;

    for(size_t i = 0; i < size; i+=word_size){
        void* cpy_of_write = malloc(word_size);
        memcpy(cpy_of_write, source + i, word_size);
        
        printf("TM_WRITE: adding to write-set: %p, with %zuth value: %ld\n", starting_target_word + i, i, *(long*)cpy_of_write);fflush(stdout);
        int res = dic_add(transaction->write_set, starting_target_word + i, 8);
        *transaction->write_set->value = cpy_of_write; // entries are always going to be of size align
        printf("TM_WRITE: adding to write-set: %p with res:%d; %zuth value: %ld\n", starting_target_word + i, res, i, *(long*)cpy_of_write);fflush(stdout);
    }
    
    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t shared, tx_t tx, size_t size, void** target) {
    shared_rgn* shared_region = (shared_rgn*)shared;
    transaction_t* transaction = (transaction_t*)tx;

    if(target == NULL || transaction == NULL){
        return nomem_alloc;
    }
    size_t align = shared_region->align;

    // is size acceptable?
    if (size % align != 0 || (size >> 48) > 0){
        return nomem_alloc;
    }

    // allocate memory eagerly
    int res_memalign = posix_memalign(target, align, size);
    if(res_memalign != 0 || *target == NULL){
        return nomem_alloc;
    }
    memset(*target, 0, size);

    // bookeep in transaction
    ll_append(transaction->alloc_set, *target);

    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) {
    return true;
}
