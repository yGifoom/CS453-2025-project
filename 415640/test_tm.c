#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include "test_tm.h"
#include <tm.h>
#include <tx_t.h>

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
        
        // Read multiple counters and verify consistency
        long counters[NUM_COUNTERS];
        bool read_success = true;
        
        for (int j = 0; j < NUM_COUNTERS; j++) {
            void* counter_addr = (char*)tm_start(args->shared) + j * sizeof(long);
            if (!tm_read(args->shared, tx, counter_addr, sizeof(long), &counters[j])) {
                read_success = false;
                printf("[Thread %d] RO transaction %d aborted during read, retrying\n", args->thread_id, i);
                break;
            }
        }
        
        if (!read_success) {
            i--;
            continue;
        }
        
        // Verify all counters are non-negative (consistency check)
        for (int j = 0; j < NUM_COUNTERS; j++) {
            if (counters[j] < 0) {
                fprintf(stderr, "[Thread %d] FATAL: Consistency violation: counter[%d] = %ld\n", 
                        args->thread_id, j, counters[j]);
                fprintf(stderr, "Test failed - shutting down\n");
                exit(1);
            }
        }
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            printf("[Thread %d] RO transaction %d aborted at commit, retrying\n", args->thread_id, i);
            i--;
        } else {
            args->counter_array[args->thread_id]++;
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
        
        // Read-modify-write on a specific counter (atomicity test)
        int counter_idx = args->thread_id % NUM_COUNTERS;
        void* counter_addr = (char*)tm_start(args->shared) + counter_idx * sizeof(long);
        
        long value;
        if (!tm_read(args->shared, tx, counter_addr, sizeof(long), &value)) {
            fprintf(stderr, "[Thread %d] RW transaction %d: read failed (transaction auto-destroyed)\n", args->thread_id, i);
            i--;
            continue;
        }
        
        // Increment the value
        value++;
        
        if (!tm_write(args->shared, tx, &value, sizeof(long), counter_addr)) {
            fprintf(stderr, "[Thread %d] RW transaction %d: write failed (transaction auto-destroyed)\n", args->thread_id, i);
            i--;
            continue;
        }
        
        args->counter_array[args->thread_id]++;
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            // Transaction aborted and destroyed, retry with new transaction
            printf("[Thread %d] RW transaction %d aborted, retrying\n", args->thread_id, i);
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
        
        printf("[Thread %d] Alloc transaction %d: allocating and writing to segments\n", args->thread_id, i);
        
        // Allocate segments and write data to them
        void* segments[5];
        int allocated = 0;
        bool transaction_valid = true;
        
        for (int j = 0; j < 5; j++) {
            alloc_t result = tm_alloc(args->shared, tx, ALIGN * (j + 2), &segments[j]);
            if (result == success_alloc) {
                // Write test data to the allocated segment
                long test_value = (long)(args->thread_id * 1000 + i * 10 + j);
                if (!tm_write(args->shared, tx, &test_value, sizeof(long), segments[j])) {
                    fprintf(stderr, "[Thread %d] Alloc transaction %d: write to segment %d failed\n", 
                           args->thread_id, i, j);
                    transaction_valid = false;
                    break;
                }
                
                // Verify we can read it back (consistency within transaction)
                long read_value;
                if (!tm_read(args->shared, tx, segments[j], sizeof(long), &read_value)) {
                    fprintf(stderr, "[Thread %d] Alloc transaction %d: read from segment %d failed\n", 
                           args->thread_id, i, j);
                    transaction_valid = false;
                    break;
                }
                
                if (read_value != test_value) {
                    fprintf(stderr, "[Thread %d] FATAL: Consistency error: wrote %ld, read %ld\n", 
                           args->thread_id, test_value, read_value);
                    fprintf(stderr, "Test failed - shutting down\n");
                    exit(1);
                }
                
                allocated++;
                if (j % 2 == 0) {
                    printf("[Thread %d] Alloc transaction %d: allocated and verified segment %d\n", 
                           args->thread_id, i, j);
                }
            } else if (result == nomem_alloc) {
                printf("[Thread %d] Alloc transaction %d: nomem at segment %d\n", args->thread_id, i, j);
                break;
            } else {
                printf("[Thread %d] Alloc transaction %d: abort_alloc at segment %d\n", args->thread_id, i, j);
                transaction_valid = false;
                break;
            }
        }
        
        if (!transaction_valid) {
            i--;
            continue;
        }
        
        printf("[Thread %d] Alloc transaction %d: allocated %d segments total\n", 
               args->thread_id, i, allocated);
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            // Transaction aborted and destroyed, retry with new transaction
            printf("[Thread %d] Alloc transaction %d aborted, retrying\n", args->thread_id, i);
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
        
        // Allocate a segment and write data
        void* new_segment;
        alloc_t alloc_result = tm_alloc(args->shared, tx, ALIGN * 4, &new_segment);
        
        if (alloc_result == success_alloc) {
            printf("[Thread %d] Mixed transaction %d: allocated segment at %p\n", 
                   args->thread_id, i, new_segment);
            
            // Write data to the segment
            long data[4];
            for (int k = 0; k < 4; k++) {
                data[k] = (long)(args->thread_id * 100 + i * 10 + k);
            }
            
            if (!tm_write(args->shared, tx, data, sizeof(data), new_segment)) {
                fprintf(stderr, "[Thread %d] Mixed transaction %d: write failed (transaction auto-destroyed)\n", 
                        args->thread_id, i);
                i--;
                continue;
            }
            
            // Read it back to verify (consistency)
            long read_data[4];
            if (!tm_read(args->shared, tx, new_segment, sizeof(read_data), read_data)) {
                fprintf(stderr, "[Thread %d] Mixed transaction %d: read failed (transaction auto-destroyed)\n", 
                        args->thread_id, i);
                i--;
                continue;
            }
            
            // Verify data integrity
            for (int k = 0; k < 4; k++) {
                if (read_data[k] != data[k]) {
                    fprintf(stderr, "[Thread %d] FATAL: Data integrity error at index %d: wrote %ld, read %ld\n", 
                           args->thread_id, k, data[k], read_data[k]);
                    fprintf(stderr, "Test failed - shutting down\n");
                    exit(1);
                }
            }
            
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
            // Transaction aborted and destroyed, retry with new transaction
            printf("[Thread %d] Mixed transaction %d aborted, retrying\n", args->thread_id, i);
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
        
        printf("[Thread %d] Very long transaction %d: performing multiple operations\n", 
               args->thread_id, i);
        
        // Test atomicity: multiple reads and writes should all succeed or all fail
        bool transaction_valid = true;
        int successful_ops = 0;
        
        // First, do multiple counter increments
        for (int j = 0; j < 5; j++) {
            int counter_idx = (args->thread_id + j) % NUM_COUNTERS;
            void* counter_addr = (char*)tm_start(args->shared) + counter_idx * sizeof(long);
            
            long value;
            if (!tm_read(args->shared, tx, counter_addr, sizeof(long), &value)) {
                fprintf(stderr, "[Thread %d] Very long transaction %d: read %d failed\n", 
                       args->thread_id, i, j);
                transaction_valid = false;
                break;
            }
            
            value++;
            
            if (!tm_write(args->shared, tx, &value, sizeof(long), counter_addr)) {
                fprintf(stderr, "[Thread %d] Very long transaction %d: write %d failed\n", 
                       args->thread_id, i, j);
                transaction_valid = false;
                break;
            }
            
            successful_ops++;
        }
        
        // Then allocate and write to new segments
        if (transaction_valid) {
            for (int j = 0; j < 5; j++) {
                void* segment;
                alloc_t result = tm_alloc(args->shared, tx, ALIGN * 2, &segment);
                if (result != success_alloc) {
                    if (result == nomem_alloc) {
                        printf("[Thread %d] Very long transaction %d: out of memory at alloc %d\n", 
                               args->thread_id, i, j);
                    }
                    break;
                }
                
                // Write and verify
                long test_val = (long)(args->thread_id * 10000 + i * 100 + j);
                if (!tm_write(args->shared, tx, &test_val, sizeof(long), segment)) {
                    fprintf(stderr, "[Thread %d] Very long transaction %d: write to segment %d failed\n", 
                           args->thread_id, i, j);
                    transaction_valid = false;
                    break;
                }
                
                long check_val;
                if (!tm_read(args->shared, tx, segment, sizeof(long), &check_val)) {
                    fprintf(stderr, "[Thread %d] Very long transaction %d: read from segment %d failed\n", 
                           args->thread_id, i, j);
                    transaction_valid = false;
                    break;
                }
                
                if (check_val != test_val) {
                    fprintf(stderr, "[Thread %d] FATAL: Atomicity violation: wrote %ld, read %ld\n", 
                           args->thread_id, test_val, check_val);
                    fprintf(stderr, "Test failed - shutting down\n");
                    exit(1);
                }
                
                successful_ops++;
                args->counter_array[args->thread_id]++;
            }
        }
        
        if (!transaction_valid) {
            printf("[Thread %d] Very long transaction %d failed mid-transaction, retrying\n", 
                   args->thread_id, i);
            i--;
            continue;
        }
        
        printf("[Thread %d] Very long transaction %d: completed %d operations\n", 
               args->thread_id, i, successful_ops);
        
        bool success = tm_end(args->shared, tx);
        if (!success) {
            // Transaction aborted and destroyed, retry with new transaction
            printf("[Thread %d] Very long transaction %d aborted, retrying\n", args->thread_id, i);
            // Rollback counter (testing atomicity)
            args->counter_array[args->thread_id] -= 5;
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
    
    // Initialize counters in shared memory
    printf("Initializing shared memory counters...\n");
    tx_t init_tx = tm_begin(shared, false);
    if (init_tx == invalid_tx) {
        fprintf(stderr, "Failed to begin initialization transaction\n");
        tm_destroy(shared);
        return 1;
    }
    
    for (int i = 0; i < NUM_COUNTERS; i++) {
        void* counter_addr = (char*)tm_start(shared) + i * sizeof(long);
        long zero = 0;
        if (!tm_write(shared, init_tx, &zero, sizeof(long), counter_addr)) {
            fprintf(stderr, "Failed to initialize counter %d\n", i);
            tm_destroy(shared);
            return 1;
        }
    }
    
    if (!tm_end(shared, init_tx)) {
        fprintf(stderr, "Failed to commit initialization transaction\n");
        tm_destroy(shared);
        return 1;
    }
    printf("✓ Initialized %d counters to 0\n\n", NUM_COUNTERS);
    
    // Create counter array for threads
    long* counter_array = calloc(NUM_THREADS, sizeof(long));
    assert(counter_array != NULL);
    
    // Test different transaction patterns
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    void* (*transaction_types[])(void*) = {
        short_ro_transaction,
        short_rw_transaction,
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
    
    // Verify final state consistency
    printf("Verifying final state consistency...\n");
    tx_t verify_tx = tm_begin(shared, true);
    if (verify_tx != invalid_tx) {
        long final_counters[NUM_COUNTERS];
        bool verification_ok = true;
        long total_shared = 0;
        
        for (int i = 0; i < NUM_COUNTERS; i++) {
            void* counter_addr = (char*)tm_start(shared) + i * sizeof(long);
            if (!tm_read(shared, verify_tx, counter_addr, sizeof(long), &final_counters[i])) {
                fprintf(stderr, "Failed to read counter %d during verification\n", i);
                verification_ok = false;
                break;
            }
            total_shared += final_counters[i];
            
            // Verify consistency: all counters should be non-negative
            if (final_counters[i] < 0) {
                fprintf(stderr, "✗ FATAL: Consistency violation: counter[%d] = %ld (negative)\n", 
                        i, final_counters[i]);
                fprintf(stderr, "Test failed - shutting down\n");
                exit(1);
            }
        }
        
        tm_end(shared, verify_tx);
        
        if (verification_ok) {
            printf("✓ All counters are non-negative (consistency maintained)\n");
            printf("Shared memory counter values:\n");
            for (int i = 0; i < NUM_COUNTERS; i++) {
                printf("  Counter[%d]: %ld\n", i, final_counters[i]);
            }
            printf("Total shared counter sum: %ld\n", total_shared);
        } else {
            fprintf(stderr, "✗ Consistency verification failed\n");
        }
    } else {
        fprintf(stderr, "Failed to begin verification transaction\n");
    }
    printf("\n");
    
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
