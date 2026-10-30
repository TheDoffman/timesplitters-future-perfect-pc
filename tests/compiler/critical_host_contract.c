#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "host_critical.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE thread_handle;
typedef DWORD (WINAPI *thread_function)(void *);
#define THREAD_RESULT DWORD WINAPI
#else
#include <pthread.h>
#include <sched.h>
#include <time.h>
typedef pthread_t thread_handle;
typedef void *(*thread_function)(void *);
#define THREAD_RESULT void *
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"critical host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static tsfp_critical_registry *registry;static uint64_t counter;static int single;
#ifdef _WIN32
static volatile LONG probed,acquired;
static void signal_flag(volatile LONG *flag){InterlockedExchange(flag,1);}
static int flag_value(volatile LONG *flag){return InterlockedCompareExchange(flag,0,0)!=0;}
static uint64_t milliseconds(void){return GetTickCount64();}
static void yield(void){(void)SwitchToThread();}
#else
static int probed,acquired;static pthread_mutex_t flags=PTHREAD_MUTEX_INITIALIZER;
static void signal_flag(int *flag){CHECK(!pthread_mutex_lock(&flags));*flag=1;CHECK(!pthread_mutex_unlock(&flags));}
static int flag_value(int *flag){int result;CHECK(!pthread_mutex_lock(&flags));result=*flag;CHECK(!pthread_mutex_unlock(&flags));return result;}
static uint64_t milliseconds(void){struct timespec now;CHECK(!clock_gettime(CLOCK_MONOTONIC,&now));return (uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000;}
static void yield(void){(void)sched_yield();}
#endif
static THREAD_RESULT probe(void *unused) {
    (void)unused;
    if(single) {
        CHECK(tsfp_critical_enter(registry,9,99,1)==TSFP_CRITICAL_BUSY);
        CHECK(tsfp_critical_leave(registry,9)==TSFP_CRITICAL_NOT_OWNER);signal_flag(&probed);return 0;
    }
    CHECK(tsfp_critical_enter(registry,3,99,1)==TSFP_CRITICAL_BUSY);
    CHECK(tsfp_critical_leave(registry,3)==TSFP_CRITICAL_NOT_OWNER);
    CHECK(tsfp_critical_enter(registry,9,99,1)==TSFP_CRITICAL_OK);
    CHECK(tsfp_critical_leave(registry,9)==TSFP_CRITICAL_OK);signal_flag(&probed);
    CHECK(tsfp_critical_enter(registry,3,88,0)==TSFP_CRITICAL_OK);signal_flag(&acquired);
    CHECK(tsfp_critical_leave(registry,3)==TSFP_CRITICAL_OK);return 0;
}
static THREAD_RESULT increment(void *unused) {
    unsigned i;(void)unused;
    for(i=0;i<5000;++i) {
        uint64_t value;CHECK(tsfp_critical_enter(registry,19,0,0)==TSFP_CRITICAL_OK);
        CHECK(tsfp_critical_enter(registry,19,0,1)==TSFP_CRITICAL_OK);value=counter;
        if(i%113==0)yield();counter=value+1;
        CHECK(tsfp_critical_leave(registry,19)==TSFP_CRITICAL_OK && tsfp_critical_leave(registry,19)==TSFP_CRITICAL_OK);
    }
    return 0;
}
static thread_handle start(thread_function function) {
    thread_handle thread;
#ifdef _WIN32
    thread=CreateThread(NULL,0,function,NULL,0,NULL);CHECK(thread!=NULL);
#else
    CHECK(!pthread_create(&thread,NULL,function,NULL));
#endif
    return thread;
}
static void join(thread_handle thread) {
#ifdef _WIN32
    CHECK(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread));
#else
    CHECK(!pthread_join(thread,NULL));
#endif
}
static tsfp_critical_record *no_memory(void *context){(void)context;return NULL;}
int main(int argc,char **argv) {
    tsfp_host_critical *holder;tsfp_critical_view view;thread_handle worker,threads[6];uint64_t deadline;unsigned i;size_t before;
    tsfp_critical_record *(*original_create)(void *);
    single=argc>1 && !strcmp(argv[1],"single");holder=tsfp_host_critical_create(single);CHECK(holder);registry=tsfp_host_critical_shared(holder);
    original_create=registry->ops.create;registry->ops.create=no_memory;
    CHECK(tsfp_critical_enter(registry,3,0,1)==TSFP_CRITICAL_NO_MEMORY);registry->ops.create=original_create;
    CHECK(tsfp_critical_enter(registry,3,11,0)==TSFP_CRITICAL_OK && tsfp_critical_enter(registry,3,22,1)==TSFP_CRITICAL_OK);
    worker=start(probe);deadline=milliseconds()+10000;
    while(!flag_value(&probed) && milliseconds()<deadline)yield();CHECK(flag_value(&probed));
    CHECK(!tsfp_host_critical_destroy(holder));CHECK(tsfp_critical_snapshot(registry,3,&view) && view.depth==2 && view.site==11);
    if(single) {
        join(worker);CHECK(tsfp_critical_enter(registry,9,0,1)==TSFP_CRITICAL_OK);
        CHECK(tsfp_critical_leave(registry,9)==TSFP_CRITICAL_OK && tsfp_critical_leave(registry,3)==TSFP_CRITICAL_OK && tsfp_critical_leave(registry,3)==TSFP_CRITICAL_OK);
        CHECK(tsfp_critical_snapshot(registry,0,&view) && view.live==1 && !view.held && !view.active);
    } else {
        deadline=milliseconds()+10000;
        do {CHECK(tsfp_critical_snapshot(registry,3,&view));if(view.active)break;yield();}while(milliseconds()<deadline);
        CHECK(view.active && !flag_value(&acquired));
        CHECK(tsfp_critical_leave(registry,3)==TSFP_CRITICAL_OK && !flag_value(&acquired));
        CHECK(tsfp_critical_leave(registry,3)==TSFP_CRITICAL_OK);join(worker);CHECK(flag_value(&acquired));
        for(i=0;i<6;++i)threads[i]=start(increment);for(i=0;i<6;++i)join(threads[i]);CHECK(counter==30000);
        CHECK(tsfp_critical_snapshot(registry,0,&view) && !view.held && !view.active && view.enters==view.leaves);before=view.live;
        for(i=0;i<10000;++i)CHECK(tsfp_critical_initialize(registry,0x100000u+(uintptr_t)i*64)==TSFP_CRITICAL_OK);
        CHECK(tsfp_critical_snapshot(registry,0,&view) && view.live==before+10000);
    }
    CHECK(tsfp_host_critical_destroy(holder));puts("PASS: actual OS recursive locks, try/enter/leave ownership, independent keys, blocked waiter, 30000 synchronized increments and 10000 records");return 0;
}
