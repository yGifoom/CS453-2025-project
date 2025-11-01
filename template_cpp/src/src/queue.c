#define _POSIX_C_SOURCE 199309L
#include<queue.h>
#include<stdio.h>
// add time and errno for timed wait
#include <time.h>
#include <errno.h>

int queue_push(queue_t *q, void *data, size_t dataSize){
    int err = pthread_mutex_lock(&(q->mutex));
    if (err != 0) {
        return err;
    }
    
    // case empty queue
    if (q->head == NULL){
        q->head = malloc(sizeof(node_t));
        q->head->data = data;
        q->head->dataSize = dataSize;
        q->head->next = NULL;
        q->head->prev = NULL;
        q->end = q->head;
    }
    
    else{
        node_t *oldHead = q->head;
        node_t *newhead = malloc(sizeof(node_t));
        newhead->data = data;
        newhead->dataSize = dataSize;
        newhead->next = oldHead;

        oldHead->prev = newhead;
        newhead->prev = NULL;
        q->head = newhead;
    }

    q->size++;

    // Signal waiting threads that data is available
    pthread_cond_signal(&(q->cond));

    err = pthread_mutex_unlock(&(q->mutex));
    if (err != 0){
        return err;
    }
    return err;
}

int queue_pop(queue_t *q, void **data, size_t *dataSize){
    
    int err = pthread_mutex_lock(&(q->mutex));
    if (err != 0){
        return err;
    }

    // Wait until queue has elements
    while (q->size == 0) {
        err = pthread_cond_wait(&(q->cond), &(q->mutex));
        if (err != 0) {
            pthread_mutex_unlock(&(q->mutex));
            return err;
        }
    }

    // get return data
    *data = q->end->data;
    *dataSize = q->end->dataSize;

    // cleanup queue
    node_t* oldEnd = q->end;
    q->end = q->end->prev;
    if (q->end != NULL) {
        q->end->next = NULL;
    } else {
        // Queue is now empty, update head
        q->head = NULL;
    }
    q->size--;
    free(oldEnd);

    err = pthread_mutex_unlock(&(q->mutex));
    if (err != 0){
        return err;
    }

    return err;
}

int queue_pop_timed(queue_t *q, void **data, size_t *dataSize, long timeout_ms){
    if (timeout_ms < 0) {
        // Indefinite wait: reuse blocking behavior
        return queue_pop(q, data, dataSize);
    }

    int err = pthread_mutex_lock(&(q->mutex));
    if (err != 0){
        return err;
    }

    if (q->size == 0) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&(q->mutex));
            return ETIMEDOUT;
        }

        // Build absolute timeout for pthread_cond_timedwait
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            int saved = errno;
            pthread_mutex_unlock(&(q->mutex));
            return saved;
        }
        ts.tv_sec += timeout_ms / 1000;
        long add_ns = (timeout_ms % 1000) * 1000000L;
        ts.tv_nsec += add_ns;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec %= 1000000000L;
        }

        while (q->size == 0) {
            err = pthread_cond_timedwait(&(q->cond), &(q->mutex), &ts);
            if (err == ETIMEDOUT) {
                pthread_mutex_unlock(&(q->mutex));
                return ETIMEDOUT;
            }
            if (err != 0) {
                pthread_mutex_unlock(&(q->mutex));
                return err;
            }
        }
    }

    // Pop the item (same logic as queue_pop)
    *data = q->end->data;
    *dataSize = q->end->dataSize;

    node_t* oldEnd = q->end;
    q->end = q->end->prev;
    if (q->end != NULL) {
        q->end->next = NULL;
    } else {
        q->head = NULL;
    }
    q->size--;
    free(oldEnd);

    err = pthread_mutex_unlock(&(q->mutex));
    if (err != 0){
        return err;
    }
    return 0;
}

size_t queue_size(queue_t *q){
    
    int err = pthread_mutex_lock(&(q->mutex));
    if (err != 0){
        return 0;
    }

    size_t qSize = q->size;

    err = pthread_mutex_unlock(&(q->mutex));
    if (err != 0){
        return 0;
    }

    return qSize;
}

queue_t* queue_init(){
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    
    queue_t* q = malloc(sizeof(queue_t));
    if (q == NULL) {
        return NULL;
    }
    q->mutex = mutex;
    q->cond = cond;
    q->size = 0;
    q->head = NULL;
    q->tail = NULL;
    q->end = NULL;
    return q;
}

int queue_destroy(queue_t *q){
    int err = pthread_mutex_lock(&(q->mutex));
    if(err != 0){
        return err;
    }
    node_t* current = q->head;
    while(current != NULL){
        node_t* next = current->next;
        free(current);
        current = next;
    }
    
    // Wake up any waiting threads before destroying
    pthread_cond_broadcast(&(q->cond));
    
    pthread_mutex_unlock(&(q->mutex));

    // this will fail if somebody else is holding the mutex
    err = pthread_mutex_destroy(&(q->mutex));
    if (err != 0){
        return err;
    }

    err = pthread_cond_destroy(&(q->cond));
    if (err != 0){
        return err;
    }

    free(q);
    return err;
}

