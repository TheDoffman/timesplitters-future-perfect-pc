/* Original continuation contracts: register images, nesting, TLS and fibers. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_ebx,g_edi,g_esi,g_esp,g_ebp,g_seh_ebp;
static unsigned char ram[4096];
static unsigned escaped;
static uint32_t wanted;
#if defined(_WIN32)
static HANDLE start_event;
static volatile LONG ready;
static void *main_fiber,*target_fiber;
static jmp_buf host_return;
static jmp_buf *pending_target;
static int pending_value,foreign_called,fiber_resumed;
static int bad_adapter;
#else
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static unsigned ready;
static int started;
#endif
static jmp_buf *rearm_slot;
static jmp_buf *nested_slot;
static unsigned rearmed;
static void put(uint32_t address,uint32_t value) { memcpy(ram+address,&value,4); }
static void populate(uint32_t address) {
    put(address,0xabc123u);put(address+4,0xbbb123u);put(address+8,0xddd123u);
    put(address+12,0xeee123u);put(address+16,0xffffffffu);
}
static int restored(void) {
    return g_ebx==0xbbb123u && g_edi==0xddd123u && g_esi==0xeee123u &&
           g_esp==3 && g_ebp==0xabc123u && g_seh_ebp==g_ebp;
}
static void deepest(void) { (void)recomp_guest_longjmp(0x81,wanted);++escaped; }
static void middle(void) { deepest();++escaped; }
static int basic(uint32_t result) {
    wanted=result;
    switch(setjmp(*recomp_setjmp_slot(0x81))) {
    case 0: middle();return 2;
    case 1: if(wanted!=0)return 3;break;
    case 7: if(wanted!=7)return 4;break;
    case (-2147483647-1):if(wanted!=0x80000000u)return 6;break;
    case -1:if(wanted!=0xffffffffu)return 7;break;
    default:return 5;
    }
    return escaped || !restored();
}
static int rearm_inner(void) {
    if(recomp_setjmp_slot(0x81)!=rearm_slot)return 1;
    switch(setjmp(*rearm_slot)) {
    case 0:(void)recomp_guest_longjmp(0x81,7);return 2;
    case 7:rearmed=1;return !restored();
    default:return 3;
    }
}
static int rearm(void) {
    rearm_slot=recomp_setjmp_slot(0x81);
    switch(setjmp(*rearm_slot)) {
    case 0:return rearm_inner() || rearmed!=1;
    default:return 4;
    }
}
static void inner(void) {
    nested_slot=recomp_setjmp_slot(0x101);
    if(setjmp(*nested_slot)!=0)abort();
    (void)recomp_guest_longjmp(0x81,7);abort();
}
static int nesting(void) {
    switch(setjmp(*recomp_setjmp_slot(0x81))) {
    case 0:inner();return 2;
    case 7:break;
    default:return 3;
    }
    if(!restored())return 4;
    /* Observe retirement without jumping into a dead native frame, even when
     * a negative control deliberately breaks retirement. */
    if(recomp_setjmp_slot(0x201)!=nested_slot)return 6;
    g_ebx=123;
    if(recomp_guest_longjmp(0x101,2)!=0 || g_ebx!=123)return 5;
    return 0;
}
static int capacity(void) {
    jmp_buf *saved[32];
    for(unsigned i=0;i<32;++i) {
        saved[i]=recomp_setjmp_slot(i);
        for(unsigned j=0;j<i;++j)if(saved[i]==saved[j])return 1;
    }
    if(recomp_setjmp_slot(0)!=saved[0])return 2;
    if(recomp_setjmp_slot(32)!=saved[31])return 3;
    if(recomp_guest_longjmp(31,1)!=0)return 4;
    if(recomp_setjmp_slot(33)!=saved[31])return 5;
    if(recomp_guest_longjmp(32,1)!=0)return 6;
    return 0;
}
static int await_start(void) {
#if defined(_WIN32)
    InterlockedIncrement(&ready);
    return WaitForSingleObject(start_event,10000)==WAIT_OBJECT_0;
#else
    pthread_mutex_lock(&gate);
    ++ready;pthread_cond_broadcast(&changed);
    while(!started)pthread_cond_wait(&changed,&gate);
    pthread_mutex_unlock(&gate);return 1;
#endif
}
static unsigned run_worker(uint32_t value) {
    switch(setjmp(*recomp_setjmp_slot(0x81))) {
    case 0:
        if(!await_start())return 2;
        (void)recomp_guest_longjmp(0x81,value);return 3;
    case 1:if(value!=1)return 4;break;
    case 2:if(value!=2)return 4;break;
    case 3:if(value!=3)return 4;break;
    case 4:if(value!=4)return 4;break;
    default:return 4;
    }
    return !restored();
}
#if defined(_WIN32)
static DWORD WINAPI worker(void *argument) { return run_worker((uint32_t)(uintptr_t)argument); }
#else
static void *worker(void *argument) { return (void*)(uintptr_t)run_worker((uint32_t)(uintptr_t)argument); }
#endif
static int threads(void) {
#if defined(_WIN32)
    HANDLE handles[4];
    start_event=CreateEventA(NULL,TRUE,FALSE,NULL);if(!start_event)return 1;
    for(unsigned i=0;i<4;++i) {
        handles[i]=CreateThread(NULL,0,worker,(void*)(uintptr_t)(i+1),0,NULL);
        if(!handles[i])return 2;
    }
    for(unsigned i=0;i<10000 && InterlockedCompareExchange(&ready,0,0)!=4;++i)Sleep(1);
    if(InterlockedCompareExchange(&ready,0,0)!=4)return 3;
    SetEvent(start_event);
    if(WaitForMultipleObjects(4,handles,TRUE,10000)!=WAIT_OBJECT_0)return 4;
    for(unsigned i=0;i<4;++i) {
        DWORD code;if(!GetExitCodeThread(handles[i],&code)||code)return 5;
        CloseHandle(handles[i]);
    }
    CloseHandle(start_event);
#else
    pthread_t handles[4];
    for(unsigned i=0;i<4;++i)
        if(pthread_create(&handles[i],NULL,worker,(void*)(uintptr_t)(i+1)))return 2;
    pthread_mutex_lock(&gate);
    while(ready!=4)pthread_cond_wait(&changed,&gate);
    started=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
    for(unsigned i=0;i<4;++i) {
        void *result;
        if(pthread_join(handles[i],&result) || (uintptr_t)result)return 5;
    }
#endif
    return 0;
}
#if defined(_WIN32)
static VOID CALLBACK fiber_entry(void *unused) {
    (void)unused;
    switch(setjmp(*recomp_setjmp_slot(0x81))) {
    case 0:
        SwitchToFiber(main_fiber);
        if(!pending_target)abort();
        longjmp(*pending_target,pending_value);
    case 1:
        fiber_resumed=restored();SwitchToFiber(main_fiber);abort();
    default:abort();
    }
}
static void foreign_route(jmp_buf *target,int value,uintptr_t stack) {
    ULONG_PTR low,high;
    GetCurrentThreadStackLimits(&low,&high);
    if(value!=1 || (stack>=low && stack<high))abort();
    if(bad_adapter==1)return;
    ++foreign_called;pending_target=target;pending_value=value;
    SwitchToFiber(target_fiber);
    longjmp(host_return,1);
}
static int fibers(void) {
    main_fiber=ConvertThreadToFiber(NULL);if(!main_fiber)return 1;
    target_fiber=CreateFiber(0,fiber_entry,NULL);if(!target_fiber)return 2;
    SwitchToFiber(target_fiber);
    if(bad_adapter!=2)recomp_set_foreign_longjmp(foreign_route);
    switch(setjmp(host_return)) {
    case 0:(void)recomp_guest_longjmp(0x81,0);return 3;
    case 1:break;
    default:return 4;
    }
    recomp_set_foreign_longjmp(NULL);
    DeleteFiber(target_fiber);
    if(!ConvertFiberToThread())return 5;
    return foreign_called!=1 || !fiber_resumed;
}
#endif
int main(int argc,char **argv) {
    if(argc!=2)return 99;
#if defined(_WIN32)
    /* Failure-path subprocesses must not open a crash dialog on the host. */
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    SetErrorMode(SEM_NOGPFAULTERRORBOX);
#endif
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)ram;populate(0x81);populate(0x101);
    if(!strcmp(argv[1],"basic"))return basic(7);
    if(!strcmp(argv[1],"zero"))return basic(0);
    if(!strcmp(argv[1],"high-bit"))return basic(0x80000000u);
    if(!strcmp(argv[1],"all-bits"))return basic(0xffffffffu);
    if(!strcmp(argv[1],"rearm"))return rearm();
    if(!strcmp(argv[1],"nested"))return nesting();
    if(!strcmp(argv[1],"capacity"))return capacity();
    if(!strcmp(argv[1],"threads"))return threads();
#if defined(_WIN32)
    if(!strcmp(argv[1],"fibers"))return fibers();
    if(!strcmp(argv[1],"returning-adapter")) { bad_adapter=1;return fibers(); }
    if(!strcmp(argv[1],"missing-adapter")) { bad_adapter=2;return fibers(); }
#endif
    if(!strcmp(argv[1],"missing")) {
        g_xbox_mem_offset=1;g_ebx=123;
        return recomp_guest_longjmp(0xffffff00u,7)!=0 || g_ebx!=123;
    }
    return 98;
}
