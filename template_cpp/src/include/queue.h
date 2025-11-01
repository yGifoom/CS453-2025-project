#ifndef QUEUE_H
#define QUEUE_H

#include<pthread.h>
#include<stdlib.h>

// interface designed with LLM
typedef struct node {
    void *data;
    size_t dataSize;
    struct node *next;
    struct node *prev;
} node_t;

typedef struct {
    node_t *head;
    node_t *tail;
    node_t *end; // pointer to the next element to be popped
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    size_t size;
} queue_t;

queue_t* queue_init();

int queue_destroy(queue_t *q);

int queue_push(queue_t *q, void *data, size_t dataSize);

int queue_pop(queue_t *q, void **data, size_t *dataSize);

size_t queue_size(queue_t *q);

// Add a timed pop wrapper around queue_pop.
// timeout_ms semantics:
//   < 0: block indefinitely (same as queue_pop)
//   = 0: non-blocking, return ETIMEDOUT if empty
//   > 0: wait up to timeout_ms milliseconds
int queue_pop_timed(queue_t *q, void **data, size_t *dataSize, long timeout_ms);

#endif