/* No display or GPU: reproduce a guest busy-polling while a renderer requests
 * synchronous owner-thread work. The actual dispatcher must keep serving it. */
#include <SDL3/SDL.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
static int owned_create(pthread_t *,const pthread_attr_t *,void *(*)(void *),void *);
static int owned_join(pthread_t,void **);
#define pthread_create owned_create
#define pthread_join owned_join
#include "guest_dispatch_sdl.h"
#undef pthread_create
#undef pthread_join
static pthread_t owner;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static SDL_MainThreadCallback task;static void *argument;static int task_done;
static int video,fail_init,fail_thread,init_calls,thread_calls,joins,entries;
static _Thread_local int guest_tls;
bool SDL_IsMainThread(void){return pthread_equal(pthread_self(),owner)!=0;}
SDL_InitFlags SDL_WasInit(SDL_InitFlags flags){return video?flags:0;}
bool SDL_InitSubSystem(SDL_InitFlags flags){assert(SDL_IsMainThread() && flags==SDL_INIT_VIDEO);init_calls++;if(fail_init)return false;video=1;return true;}
const char *SDL_GetError(void){return "injected failure";}
int SDL_GetAtomicInt(SDL_AtomicInt *value){return __atomic_load_n(&value->value,__ATOMIC_ACQUIRE);}
int SDL_SetAtomicInt(SDL_AtomicInt *value,int next){return __atomic_exchange_n(&value->value,next,__ATOMIC_ACQ_REL);}
static int owned_create(pthread_t *thread,const pthread_attr_t *attributes,void *(*fn)(void *),void *data){
    size_t stack=0;assert(SDL_IsMainThread());assert(!pthread_attr_getstacksize(attributes,&stack) && stack>=8u*1024u*1024u);thread_calls++;
    if(fail_thread)return EAGAIN;return pthread_create(thread,attributes,fn,data);
}
static int owned_join(pthread_t thread,void **status){assert(SDL_IsMainThread());joins++;return pthread_join(thread,status);}
bool SDL_RunOnMainThread(SDL_MainThreadCallback callback,void *data,bool wait){
    assert(!SDL_IsMainThread() && wait);pthread_mutex_lock(&lock);assert(!task);
    task=callback;argument=data;task_done=0;pthread_cond_broadcast(&changed);
    while(!task_done)pthread_cond_wait(&changed,&lock);
    pthread_mutex_unlock(&lock);return true;
}
bool SDL_WaitEventTimeout(SDL_Event *event,Sint32 milliseconds){
    assert(SDL_IsMainThread() && !event && milliseconds==1);pthread_mutex_lock(&lock);
    if(!task){struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_nsec+=1000000;if(until.tv_nsec>=1000000000){until.tv_sec++;until.tv_nsec-=1000000000;}pthread_cond_timedwait(&changed,&lock,&until);}
    SDL_MainThreadCallback fn=task;void *data=argument;task=NULL;pthread_mutex_unlock(&lock);
    if(fn){fn(data);pthread_mutex_lock(&lock);task_done=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&lock);}return fn!=NULL;
}
static SDL_AtomicInt finished;
static void main_work(void *data){assert(SDL_IsMainThread() && guest_tls==99);*(int *)data+=5;}
static void *render_work(void *data){assert(SDL_RunOnMainThread(main_work,data,true));SDL_SetAtomicInt(&finished,1);return NULL;}
static int guest(void *data){
    assert(!SDL_IsMainThread() && !guest_tls);guest_tls=7;entries++;SDL_SetAtomicInt(&finished,0);
    pthread_t render;assert(!pthread_create(&render,NULL,render_work,data));
    while(!SDL_GetAtomicInt(&finished))sched_yield(); /* No host/kernel wait. */
    assert(!pthread_join(render,NULL));assert(guest_tls==7);return 37;
}
static int exits_thread(void *data){(void)data;pthread_exit((void *)(intptr_t)19);return 0;}
static void *wrong_owner(void *data){assert(tsfp_native_dispatch_guest(guest,data)==2);return NULL;}
int main(void){
    owner=pthread_self();guest_tls=99;int value=10;
    assert(tsfp_native_dispatch_guest(NULL,&value)==2);
    fail_init=1;assert(tsfp_native_dispatch_guest(guest,&value)==2 && !entries && !thread_calls);fail_init=0;
    video=1;fail_thread=1;assert(tsfp_native_dispatch_guest(guest,&value)==2 && !entries && !joins);fail_thread=0;
    video=0;assert(tsfp_native_dispatch_guest(guest,&value)==37);assert(value==15 && entries==1 && joins==1 && guest_tls==99);
    int before=init_calls;assert(tsfp_native_dispatch_guest(guest,&value)==37);assert(value==20 && entries==2 && joins==2 && init_calls==before);
    pthread_t thread;assert(!pthread_create(&thread,NULL,wrong_owner,&value));assert(!pthread_join(thread,NULL));assert(entries==2);
    assert(tsfp_native_dispatch_guest(exits_thread,NULL)==19 && joins==3);
    puts("PASS: continuous owner dispatch during guest polling, TLS ownership, result/join, initialization and worker failures");return 0;
}
