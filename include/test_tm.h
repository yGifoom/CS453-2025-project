#ifndef TEST_TM_H
#define TEST_TM_H

#include <stdbool.h>
#include <tm.h>

// Test configuration
#define NUM_THREADS 8
#define NUM_TRANSACTIONS_PER_THREAD 100
#define SHARED_SIZE 4096
#define ALIGN 8
#define NUM_COUNTERS 16

// Thread arguments structure
typedef struct {
    shared_t shared;
    int thread_id;
    long *counter_array;
} thread_args_t;

// Transaction test functions
void* short_ro_transaction(void* arg);
void* short_rw_transaction(void* arg);
void* long_alloc_transaction(void* arg);
void* mixed_transaction(void* arg);
void* very_long_transaction(void* arg);

#endif // TEST_TM_H
