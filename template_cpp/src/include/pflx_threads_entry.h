#ifndef PFLX_THREADS_ENTRY_H
#define PFLX_THREADS_ENTRY_H

#include"pflx.h"
#include<pthread.h>


// struct implemented with LLMs
// Internal thread registry (no header changes)
typedef struct _pflx_threads_entry {
    pflx* inst;
    pthread_t send_tid;
    pthread_t recv_tid;
    int send_started;
    int recv_started;
    struct _pflx_threads_entry* next;
} _pflx_threads_entry;

// starts thread running send routine
extern void* _send_thread_main(void* arg);

// starts thread running recv routine
extern void* _recv_thread_main(void* arg);

// private routine adding thread entry
extern struct _pflx_threads_entry* _thr_get(pflx* p, int create);

// private routine removing threads entry
extern void _thr_remove(pflx* p);

#endif