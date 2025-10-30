#include"pflx.h"
#include"pflx_threads_entry.h"
#include<pthread.h>

static _pflx_threads_entry* _thr_head = NULL;
static pthread_mutex_t _thr_mu = PTHREAD_MUTEX_INITIALIZER;

// Thread entry points
extern void* _send_thread_main(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    (void)_pflx_send_routine((pflx*)arg);
    return NULL;
}

extern void* _recv_thread_main(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    (void)_pflx_recv_routine((pflx*)arg);
    return NULL;
}

extern _pflx_threads_entry* _thr_get(pflx* p, int create) {
    pthread_mutex_lock(&_thr_mu);
    _pflx_threads_entry* cur = _thr_head;
    while (cur) {
        if (cur->inst == p) { pthread_mutex_unlock(&_thr_mu); return cur; }
        cur = cur->next;
    }
    if (!create) { pthread_mutex_unlock(&_thr_mu); return NULL; }
    _pflx_threads_entry* e = calloc(1, sizeof(*e));
    if (!e) { pthread_mutex_unlock(&_thr_mu); return NULL; }
    e->inst = p;
    e->next = _thr_head;
    _thr_head = e;
    pthread_mutex_unlock(&_thr_mu);
    return e;
}

extern void _thr_remove(pflx* p) {
    pthread_mutex_lock(&_thr_mu);
    _pflx_threads_entry** link = &_thr_head;
    while (*link) {
        if ((*link)->inst == p) {
            _pflx_threads_entry* del = *link;
            *link = del->next;
            free(del);
            break;
        }
        link = &(*link)->next;
    }
    pthread_mutex_unlock(&_thr_mu);
}