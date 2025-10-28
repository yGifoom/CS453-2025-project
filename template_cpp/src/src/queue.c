#include<queue.h>

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

