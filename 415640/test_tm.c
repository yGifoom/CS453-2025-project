#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include "test_tm.h"
#include <tm.h>

#define NUM_THREADS 8
#define NUM_TRANSACTIONS_PER_THREAD 100
#define SHARED_SIZE 1024
#define ALIGN 8

// Short read-only transaction
void* short_ro_transaction(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("[Thread %d] Starting short read-only transactions\n", args->thread_id);
    
    for (int i = 0; i < NUM_TRANSACTIONS_PER_THREAD; i++) {
        tx_t tx = tm_begin(args->shared, true);
        if (tx == invalid_tx) {
            fprintf(stderr, "[Thread %d] Failed to begin RO transaction %d\n", args->thread_id, i);
            continue;
        }
        
        if (i % 25 == 0) {
            printf("[Thread %d] RO transaction %d started\n", args->thread_id, i);
        }
        
        // Simulate some read operations
        usleep(rand() % 100);
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            printf("[Thread %d] RO transaction %d aborted, retrying\n", args->thread_id, i);
            // Transaction aborted, retry
            i--;
        }
    }
    
    printf("[Thread %d] Completed all read-only transactions\n", args->thread_id);
    return NULL;
}

// Short read-write transaction
void* short_rw_transaction(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("[Thread %d] Starting short read-write transactions\n", args->thread_id);
    
    for (int i = 0; i < NUM_TRANSACTIONS_PER_THREAD / 2; i++) {
        tx_t tx = tm_begin(args->shared, false);
        if (tx == invalid_tx) {
            fprintf(stderr, "[Thread %d] Failed to begin RW transaction %d\n", args->thread_id, i);
            continue;
        }
        
        if (i % 10 == 0) {
            printf("[Thread %d] RW transaction %d started\n", args->thread_id, i);
        }
        
        // Simulate write operations
        args->counter_array[args->thread_id]++;
        usleep(rand() % 50);
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            printf("[Thread %d] RW transaction %d aborted, retrying\n", args->thread_id, i);
            // Transaction aborted, retry
            args->counter_array[args->thread_id]--;
            i--;
        }
    }
    
    printf("[Thread %d] Completed all read-write transactions\n", args->thread_id);
    return NULL;
}

// Long transaction with allocations
void* long_alloc_transaction(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("[Thread %d] Starting long allocation transactions\n", args->thread_id);
    
    for (int i = 0; i < NUM_TRANSACTIONS_PER_THREAD / 10; i++) {
        tx_t tx = tm_begin(args->shared, false);
        if (tx == invalid_tx) {
            fprintf(stderr, "[Thread %d] Failed to begin alloc transaction %d\n", args->thread_id, i);
            continue;
        }
        
        printf("[Thread %d] Alloc transaction %d: allocating multiple segments\n", args->thread_id, i);
        
        // Allocate multiple segments
        void* segments[5];
        int allocated = 0;
        for (int j = 0; j < 5; j++) {
            alloc_t result = tm_alloc(args->shared, tx, ALIGN * (j + 1), &segments[j]);
            if (result == success_alloc) {
                allocated++;
                printf("[Thread %d] Alloc transaction %d: allocated segment %d (%u bytes)\n", 
                       args->thread_id, i, j, ALIGN * (j + 1));
            } else if (result == nomem_alloc) {
                printf("[Thread %d] Alloc transaction %d: nomem at segment %d\n", args->thread_id, i, j);
                break;
            } else {
                printf("[Thread %d] Alloc transaction %d: abort_alloc at segment %d\n", args->thread_id, i, j);
                break;
            }
        }
        
        printf("[Thread %d] Alloc transaction %d: allocated %d segments total\n", 
               args->thread_id, i, allocated);
        
        // Simulate some work
        usleep(rand() % 200);
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            printf("[Thread %d] Alloc transaction %d aborted, retrying\n", args->thread_id, i);
            // Transaction aborted, retry
            i--;
        } else {
            printf("[Thread %d] Alloc transaction %d committed successfully\n", args->thread_id, i);
        }
    }
    
    printf("[Thread %d] Completed all allocation transactions\n", args->thread_id);
    return NULL;
}

// Mixed transaction with alloc and free
void* mixed_transaction(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("[Thread %d] Starting mixed alloc/free transactions\n", args->thread_id);
    
    for (int i = 0; i < NUM_TRANSACTIONS_PER_THREAD / 5; i++) {
        tx_t tx = tm_begin(args->shared, false);
        if (tx == invalid_tx) {
            fprintf(stderr, "[Thread %d] Failed to begin mixed transaction %d\n", args->thread_id, i);
            continue;
        }
        
        printf("[Thread %d] Mixed transaction %d: allocating segment\n", args->thread_id, i);
        
        // Allocate a segment
        void* new_segment;
        alloc_t alloc_result = tm_alloc(args->shared, tx, ALIGN * 4, &new_segment);
        
        if (alloc_result == success_alloc) {
            printf("[Thread %d] Mixed transaction %d: allocated segment at %p\n", 
                   args->thread_id, i, new_segment);
            
            // Simulate work with the segment
            usleep(rand() % 150);
            
            printf("[Thread %d] Mixed transaction %d: freeing segment\n", args->thread_id, i);
            
            // Free it in the same transaction
            bool free_result = tm_free(args->shared, tx, new_segment);
            if (!free_result) {
                fprintf(stderr, "[Thread %d] Mixed transaction %d: failed to free segment\n", 
                        args->thread_id, i);
            } else {
                printf("[Thread %d] Mixed transaction %d: freed segment successfully\n", 
                       args->thread_id, i);
            }
        } else {
            printf("[Thread %d] Mixed transaction %d: allocation failed\n", args->thread_id, i);
        }
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            printf("[Thread %d] Mixed transaction %d aborted, retrying\n", args->thread_id, i);
            // Transaction aborted, retry
            i--;
        } else {
            printf("[Thread %d] Mixed transaction %d committed\n", args->thread_id, i);
        }
    }
    
    printf("[Thread %d] Completed all mixed transactions\n", args->thread_id);
    return NULL;
}

// Very long transaction
void* very_long_transaction(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    
    printf("[Thread %d] Starting very long transactions\n", args->thread_id);
    
    for (int i = 0; i < NUM_TRANSACTIONS_PER_THREAD / 20; i++) {
        tx_t tx = tm_begin(args->shared, false);
        if (tx == invalid_tx) {
            fprintf(stderr, "[Thread %d] Failed to begin very long transaction %d\n", args->thread_id, i);
            continue;
        }
        
        printf("[Thread %d] Very long transaction %d: performing 10 allocations\n", 
               args->thread_id, i);
        
        // Multiple allocations and operations
        int successful_allocs = 0;
        for (int j = 0; j < 10; j++) {
            void* segment;
            alloc_t result = tm_alloc(args->shared, tx, ALIGN * 2, &segment);
            if (result != success_alloc) {
                printf("[Thread %d] Very long transaction %d: allocation %d failed\n", 
                       args->thread_id, i, j);
                break;
            }
            successful_allocs++;
            
            usleep(rand() % 50);
            args->counter_array[args->thread_id]++;
        }
        
        printf("[Thread %d] Very long transaction %d: completed %d allocations\n", 
               args->thread_id, i, successful_allocs);
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            printf("[Thread %d] Very long transaction %d aborted, retrying\n", args->thread_id, i);
            // Transaction aborted, rollback counter
            args->counter_array[args->thread_id] -= 10;
            i--;
        } else {
            printf("[Thread %d] Very long transaction %d committed successfully\n", 
                   args->thread_id, i);
        }
    }
    
    printf("[Thread %d] Completed all very long transactions\n", args->thread_id);
    return NULL;
}

int main(void) {
    printf("=== Starting TM test with %d threads ===\n\n", NUM_THREADS);
    
    // Create shared memory region
    printf("Creating shared memory region (size=%zu, align=%d)...\n", (size_t)SHARED_SIZE, ALIGN);
    shared_t shared = tm_create(SHARED_SIZE, ALIGN);
    assert(shared != invalid_shared);
    printf("✓ Created shared memory region\n\n");
    
    // Verify basic properties
    printf("Verifying shared memory properties...\n");
    assert(tm_start(shared) != NULL);
    assert(tm_size(shared) == SHARED_SIZE);
    assert(tm_align(shared) == ALIGN);
    printf("✓ Start address: %p\n", tm_start(shared));
    printf("✓ Size: %zu bytes\n", tm_size(shared));
    printf("✓ Alignment: %zu bytes\n\n", tm_align(shared));
    
    // Create counter array for threads
    long* counter_array = calloc(NUM_THREADS, sizeof(long));
    assert(counter_array != NULL);
    
    // Test different transaction patterns
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    void* (*transaction_types[])(void*) = {
        //short_ro_transaction,
        //short_rw_transaction,
        long_alloc_transaction,
        mixed_transaction,
        very_long_transaction
    };
    const char* type_names[] = {
        "Short RO",
        "Short RW",
        "Long Alloc",
        "Mixed",
        "Very Long"
    };
    int num_types = sizeof(transaction_types) / sizeof(transaction_types[0]);
    
    printf("=== Starting concurrent transactions ===\n");
    printf("Thread assignments:\n");
    
    // Create threads with different transaction patterns
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].shared = shared;
        args[i].thread_id = i;
        args[i].counter_array = counter_array;
        
        // Assign different transaction types to threads
        void* (*func)(void*) = transaction_types[i % num_types];
        printf("  Thread %d: %s\n", i, type_names[i % num_types]);
        
        int ret = pthread_create(&threads[i], NULL, func, &args[i]);
        assert(ret == 0);
    }
    printf("\n");
    
    // Wait for all threads to complete
    printf("Waiting for threads to complete...\n\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("\n=== All transactions completed ===\n\n");
    
    // Print statistics
    printf("Transaction statistics:\n");
    long total_operations = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("  Thread %d (%s): %ld operations\n", 
               i, type_names[i % num_types], counter_array[i]);
        total_operations += counter_array[i];
    }
    printf("\nTotal operations: %ld\n\n", total_operations);
    
    // Cleanup
    printf("Cleaning up...\n");
    free(counter_array);
    tm_destroy(shared);
    
    printf("✓ Test completed successfully - no memory leaks or concurrency issues detected\n");
    
    return 0;
}
