/* Original cross-translation-unit and native-thread storage contracts. */
#include "recomp_types.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
static CRITICAL_SECTION gate;
static HANDLE go;
static volatile LONG ready;
#else
#include <pthread.h>
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static unsigned ready;
static int go;
#endif
void peer_fill(uint32_t seed);
int peer_verify(uint32_t seed);
void peer_process_state(void);

#define WORDS(X) X(g_eax) X(g_ebx) X(g_ecx) X(g_edx) X(g_esi) X(g_edi) \
    X(g_esp) X(g_ebp) X(g_seh_ebp) X(g_fs_base) X(g_icall_saved_esp) X(g_icall_dispatch_form)
#define VECTORS(X) X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7)

#ifdef STATE_PEER
void peer_process_state(void) {
    g_force_return=1;g_icall_count=UINT64_C(0x100000001);g_icall_trace_idx=19;
    for(unsigned i=0;i<ICALL_TRACE_SIZE;++i)g_icall_trace[i]=i+100;
}
void peer_fill(uint32_t seed) {
    unsigned ordinal=0;
#define STORE(name) name=seed+(++ordinal);
    WORDS(STORE)
#undef STORE
#define STORE(n) g_xmm##n.q[0]=((uint64_t)seed<<32)+(n); \
    g_xmm##n.q[1]=~g_xmm##n.q[0];g_mm##n.q=g_xmm##n.q[1];
    VECTORS(STORE)
#undef STORE
    for(unsigned i=0;i<8;++i) g_fp_stack[i]=(double)seed+i+0.5;
    g_fp_top=3;g_fp_cmp=-1;g_df=1;
    g_fp_control_word=0x0b7f;g_fp_cc=0x4500;
}
int peer_verify(uint32_t seed) {
    unsigned ordinal=0;
#define CHECK(name) if(name!=seed+(++ordinal))return 0;
    WORDS(CHECK)
#undef CHECK
#define CHECK(n) if(g_xmm##n.q[0]!=((uint64_t)seed<<32)+(n) || \
    g_xmm##n.q[1]!=~g_xmm##n.q[0] || g_mm##n.q!=g_xmm##n.q[1])return 0;
    VECTORS(CHECK)
#undef CHECK
    for(unsigned i=0;i<8;++i)if(g_fp_stack[i]!=(double)seed+i+0.5)return 0;
    return g_fp_top==3 && g_fp_cmp==-1 && g_df==1 &&
        g_fp_control_word==0x0b7f && g_fp_cc==0x4500;
}
#else
static int defaults(void) {
    if(g_fs_base!=0x1000)return 0;
#define CHECK(name) if(name!=0 && &name!=&g_fs_base)return 0;
    WORDS(CHECK)
#undef CHECK
#define CHECK(n) if(g_xmm##n.q[0] || g_xmm##n.q[1] || g_mm##n.q)return 0;
    VECTORS(CHECK)
#undef CHECK
    for(unsigned i=0;i<8;++i)if(g_fp_stack[i]!=0.0)return 0;
    return !g_fp_top && !g_fp_cmp && !g_df &&
        g_fp_control_word==0x037f && g_fp_cc==0x4000;
}
static int verify_here(uint32_t seed) {
    unsigned ordinal=0;
#define CHECK(name) if(name!=seed+(++ordinal))return 0;
    WORDS(CHECK)
#undef CHECK
    /* A compiled generated-style expression sees the peer's same storage. */
    if(g_xmm7.q[1]!=~(((uint64_t)seed<<32)+7) || g_mm7.q!=g_xmm7.q[1])return 0;
    return peer_verify(seed);
}
static unsigned run_thread(unsigned id) {
    const uint32_t seed=id*0x10000u;
    int valid;
#if defined(_WIN32)
    EnterCriticalSection(&gate);
#else
    pthread_mutex_lock(&gate);
#endif
    valid=defaults() && g_force_return==1 && g_icall_count==UINT64_C(0x100000001)
        && g_icall_trace_idx==19 && g_icall_trace[15]==115;
    peer_fill(seed);
#if defined(_WIN32)
    LeaveCriticalSection(&gate);
    InterlockedIncrement(&ready);
    if(WaitForSingleObject(go,10000)!=WAIT_OBJECT_0)return 2;
#else
    ++ready;pthread_cond_broadcast(&changed);
    while(!go)pthread_cond_wait(&changed,&gate);
    pthread_mutex_unlock(&gate);
#endif
    return !valid || !verify_here(seed);
}
#if defined(_WIN32)
static DWORD WINAPI worker(void *arg) { return run_thread((unsigned)(uintptr_t)arg); }
#else
static void *worker(void *arg) { return (void*)(uintptr_t)run_thread((unsigned)(uintptr_t)arg); }
#endif
int main(void) {
    if(!defaults() || g_force_return || g_icall_count || g_icall_trace_idx)return 1;
    for(unsigned i=0;i<ICALL_TRACE_SIZE;++i)if(g_icall_trace[i])return 2;
    peer_process_state();
    if(g_force_return!=1 || g_icall_count!=UINT64_C(0x100000001) || g_icall_trace_idx!=19)return 9;
    for(unsigned i=0;i<ICALL_TRACE_SIZE;++i)if(g_icall_trace[i]!=i+100)return 9;
    peer_fill(123);
    if(!verify_here(123))return 3;
    /* Native-thread creation must start with ABI defaults, not parent's cells. */
#if defined(_WIN32)
    HANDLE threads[4];InitializeCriticalSection(&gate);
    go=CreateEventA(NULL,TRUE,FALSE,NULL);if(!go)return 4;
    for(unsigned i=0;i<4;++i) {
        threads[i]=CreateThread(NULL,0,worker,(void*)(uintptr_t)(i+1),0,NULL);
        if(!threads[i])return 4;
    }
    for(unsigned i=0;i<10000 && InterlockedCompareExchange(&ready,0,0)!=4;++i)Sleep(1);
    if(InterlockedCompareExchange(&ready,0,0)!=4)return 5;
    SetEvent(go);
    if(WaitForMultipleObjects(4,threads,TRUE,10000)!=WAIT_OBJECT_0)return 6;
    for(unsigned i=0;i<4;++i) {
        DWORD result;if(!GetExitCodeThread(threads[i],&result) || result)return 7;
        CloseHandle(threads[i]);
    }
    CloseHandle(go);DeleteCriticalSection(&gate);
#else
    pthread_t threads[4];
    for(unsigned i=0;i<4;++i)
        if(pthread_create(&threads[i],NULL,worker,(void*)(uintptr_t)(i+1)))return 4;
    pthread_mutex_lock(&gate);
    while(ready!=4)pthread_cond_wait(&changed,&gate);
    go=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
    for(unsigned i=0;i<4;++i) {
        void *result;if(pthread_join(threads[i],&result) || (uintptr_t)result)return 7;
    }
#endif
    return verify_here(123) ? 0 : 8;
}
#endif
