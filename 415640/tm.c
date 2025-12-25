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

// Internal headers
#include <tm.h>
#include <utils.h>          // nested free, add/rm_from_dict
#include <tx_t.h>           // transaction struct
#include <string.h>         // (memset)
#include <shared_t.h>       // shared memory region
#include <version_types.h>  // global and lock versioning
#include <dict.h>           // read/write-set
#include "macros.h"
#include "stdio.h"

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

    // creation of segment
    void* first_segment;
    int res_mem_align = posix_memalign(&first_segment, align, size);

    if (res_mem_align != 0){
        return invalid_shared;
    }
    memset(first_segment, 0, size);

    shared_rgn* shared_region = malloc(sizeof(shared_rgn));
    if (shared_region == NULL){
        free(first_segment);
        return invalid_shared;
    }

    shared_region->global_version = 0;
    shared_region->start = first_segment;
    shared_region->size = size;
    shared_region->align = align;

    // add to region the first segment
    struct dictionary* segments_dict = dic_new(0);
    dic_add(segments_dict, first_segment, 8);

    // make lock per word
    version_lock* segment_locks = malloc(sizeof(version_lock) * (size/align));
    if(segment_locks == NULL){
        free(first_segment);
        free(shared_region);
        return invalid_shared;
    }

    for(size_t i = 0; i<size/align; i++){
        segment_locks[i] = 0;
    }
    *segments_dict->value = segment_locks;

    shared_region->segments = segments_dict;
    shared_region->segment_guard = 0;
    
    return shared_region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    shared_rgn* shared_region = (shared_rgn*)shared;

    // free each segment + each lock array + destroy dict itself
    dic_nested_destroy(shared_region->segments);

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
    if(!is_ro){ // if read only accessing these sets is undefined
        tx->write_set = dic_new(0);
        tx->alloc_set = dic_new(0);
        tx->free_set = dic_new(0);
    }

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

    }else{
        // fetch and increment global counter
        int unused(wv) = atomic_fetch_add(&shared_region->global_version, 1);

        // TODO: read/write check following TL2
        lock_acquire(&shared_region->segment_guard);
        printf("CRITICAL SECTION ENTER\n");fflush(stdout);
        // add allocs
        dic_forEach(transaction->alloc_set, add_from_dict, shared_region->segments);

        // remove frees
        dic_forEach(transaction->free_set, rm_from_dict, shared_region->segments);
        
        printf("CRITICAL SECTION EXIT\n");fflush(stdout);
        lock_release(&shared_region->segment_guard);
    }
    
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
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const* unused(source), size_t unused(size), void* unused(target)) {
    // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
    return false;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t unused(shared), tx_t unused(tx), void const* unused(source), size_t unused(size), void* unused(target)) {
    // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
    return false;
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
    int res_memalign = posix_memalign(target, align, size);
    if(res_memalign != 0 || *target == NULL){
        return nomem_alloc;
    }
    memset(*target, 0, size);

    // bookeep in transaction
    dic_add(transaction->alloc_set, *target, 8);

    // make lock per word
    version_lock* segment_locks = malloc(sizeof(version_lock) * (size/align));
    if(segment_locks == NULL){
        free(target);
        *transaction->alloc_set->value = NULL;
        return nomem_alloc;
    }

    for(size_t i = 0; i<size/align; i++){
        segment_locks[i] = 0; // maybe can do this with memset
    }
    *transaction->alloc_set->value = segment_locks;


    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t tx, void* target) {
    shared_rgn* shared_region = (shared_rgn*)shared;
    transaction_t* transaction = (transaction_t*)tx;

    // trying to deallocate the first segment?
    if(target == shared_region->start){
        return false;
    }
    
    if(dic_find(transaction->alloc_set, target, 8) == 1){
        // free a segment allocd earlier by the transaction
        dic_add(transaction->alloc_set, target, 8);
        free(*transaction->alloc_set->value);
        *transaction->alloc_set->value = NULL; 
    }else{
        dic_add(transaction->free_set, target, 8);
        *transaction->free_set->value = NULL;
    }

    return true;
}
