/* Original OS adapter; no guest layout or ownership policy is implemented here. */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "host_critical.h"
#include <stdlib.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <errno.h>
#endif
typedef struct critical_native {
    tsfp_critical_record record;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
} critical_native;
struct tsfp_host_critical {
    tsfp_critical_registry shared;
    tsfp_critical_record *buckets[256];
#ifdef _WIN32
    SRWLOCK guard;
#else
    pthread_mutex_t guard;
#endif
};
static void host_guard(void *context) {
    tsfp_host_critical *h=context;
#ifdef _WIN32
    AcquireSRWLockExclusive(&h->guard);
#else
    if(pthread_mutex_lock(&h->guard))exit(EXIT_FAILURE);
#endif
}
static void host_unguard(void *context) {
    tsfp_host_critical *h=context;
#ifdef _WIN32
    ReleaseSRWLockExclusive(&h->guard);
#else
    if(pthread_mutex_unlock(&h->guard))exit(EXIT_FAILURE);
#endif
}
static tsfp_critical_record *host_new(void *context) {
    critical_native *p=calloc(1,sizeof(*p));(void)context;if(!p)return NULL;
#ifdef _WIN32
    if(!InitializeCriticalSectionEx(&p->mutex,0,0)){free(p);return NULL;}
#else
    pthread_mutexattr_t attr;int ok;
    if(pthread_mutexattr_init(&attr)){free(p);return NULL;}
    ok=pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE)==0 && pthread_mutex_init(&p->mutex,&attr)==0;
    (void)pthread_mutexattr_destroy(&attr);if(!ok){free(p);return NULL;}
#endif
    p->record.native=&p->mutex;return &p->record;
}
static int host_dispose(void *context,tsfp_critical_record *record) {
    critical_native *p=(critical_native *)record;(void)context;
#ifdef _WIN32
    DeleteCriticalSection(&p->mutex);
#else
    if(pthread_mutex_destroy(&p->mutex))return 0;
#endif
    free(p);return 1;
}
static int host_acquire(void *context,tsfp_critical_record *record,int try_only) {
    critical_native *p=(critical_native *)record;(void)context;
#ifdef _WIN32
    if(try_only)return TryEnterCriticalSection(&p->mutex)!=0;
    EnterCriticalSection(&p->mutex);return 1;
#else
    int result=try_only?pthread_mutex_trylock(&p->mutex):pthread_mutex_lock(&p->mutex);
    return !result?1:(try_only && result==EBUSY?0:-1);
#endif
}
static void host_release(void *context,tsfp_critical_record *record) {
    critical_native *p=(critical_native *)record;(void)context;
#ifdef _WIN32
    LeaveCriticalSection(&p->mutex);
#else
    if(pthread_mutex_unlock(&p->mutex))exit(EXIT_FAILURE);
#endif
}
static uintptr_t host_thread(void *context) {
    (void)context;
#ifdef _WIN32
    return (uintptr_t)GetCurrentThreadId();
#else
    return (uintptr_t)pthread_self();
#endif
}
tsfp_host_critical *tsfp_host_critical_create(int single) {
    tsfp_host_critical *h=calloc(1,sizeof(*h));tsfp_critical_ops ops={0};if(!h)return NULL;
#ifdef _WIN32
    InitializeSRWLock(&h->guard);
#else
    if(pthread_mutex_init(&h->guard,NULL)){free(h);return NULL;}
#endif
    ops.context=h;ops.lock=host_guard;ops.unlock=host_unguard;ops.create=host_new;ops.dispose=host_dispose;
    ops.acquire=host_acquire;ops.release=host_release;ops.thread=host_thread;
    if(!tsfp_critical_init(&h->shared,h->buckets,256,&ops,single)){
#ifndef _WIN32
        (void)pthread_mutex_destroy(&h->guard);
#endif
        free(h);return NULL;
    }
    return h;
}
tsfp_critical_registry *tsfp_host_critical_shared(tsfp_host_critical *h){return h?&h->shared:NULL;}
int tsfp_host_critical_destroy(tsfp_host_critical *h) {
    if(!h || !tsfp_critical_stop(&h->shared))return 0;
#ifndef _WIN32
    if(pthread_mutex_destroy(&h->guard))return 0;
#endif
    free(h);return 1;
}
