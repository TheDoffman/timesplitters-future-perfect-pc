/* Original deterministic host-policy oracle; fake waits drive boundary races. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(2);}}while(0)
#define RECOMP_TLS
#define WINAPI
#define TRUE 1
typedef int BOOL,INIT_ONCE,*PINIT_ONCE,CRITICAL_SECTION;
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef uint64_t ULONGLONG;
typedef void *PVOID,*HANDLE;
#define INIT_ONCE_STATIC_INIT 0
static unsigned initializes,locks;static DWORD thread_id=7;
static ULONGLONG ticks;static int event_ok=1,event_set=1,scenario;
static const char *lock_setting="1",*watch_setting="1",*park_setting="1";
static const char *host_environment(const char *name) {
    if(!strcmp(name,"RECOMP_GUEST_LOCK"))return lock_setting;
    if(!strcmp(name,"RECOMP_GUEST_CONCURRENCY"))return watch_setting;
    if(!strcmp(name,"RECOMP_DISPATCH_LOCK"))return park_setting;
    return NULL;
}
static void InitializeCriticalSection(CRITICAL_SECTION *p){++initializes;*p=1;}
static void EnterCriticalSection(CRITICAL_SECTION *p){CHECK(*p==1);++locks;}
static void LeaveCriticalSection(CRITICAL_SECTION *p){CHECK(*p==1 && locks);--locks;}
static BOOL InitOnceExecuteOnce(PINIT_ONCE p,BOOL (*fn)(PINIT_ONCE,PVOID,PVOID *),PVOID a,PVOID *b) {
    if(!*p){CHECK(fn(p,a,b));*p=1;}return 1;
}
static HANDLE CreateEventW(void *a,int manual,int initial,void *name) {
    CHECK(!a && manual && initial && !name);return event_ok?(HANDLE)(uintptr_t)1:NULL;
}
static int ResetEvent(HANDLE h){CHECK(h);event_set=0;return 1;}
static int SetEvent(HANDLE h){CHECK(h);event_set=1;return 1;}
static LONG InterlockedCompareExchange(volatile LONG *p,LONG v,LONG old) {
    LONG actual=*p;if(actual==old)*p=v;return actual;
}
static LONG InterlockedIncrement(volatile LONG *p){return ++*p;}
static LONG InterlockedDecrement(volatile LONG *p){return --*p;}
static DWORD GetCurrentThreadId(void){return thread_id;}
static ULONGLONG GetTickCount64(void){return ticks++;}
static void SwitchToThread(void);
static DWORD WaitForSingleObject(HANDLE,DWORD);
/* Keep this fixture portable: real Windows affinity is separately compiled. */
#ifdef _WIN32
#undef _WIN32
#endif
#define getenv host_environment
#include "guest_execution_host.inc"
#include "guest_kernel_execution.inc"
#undef getenv
static void SwitchToThread(void) {
    ++ticks;
    if(scenario==1)execution_host_park_running=0;
    if(scenario==2)execution_host_park_owner=0;
}
static DWORD WaitForSingleObject(HANDLE h,DWORD ms) {
    CHECK(h && ms==10);ticks+=ms;
    if(scenario==3)execution_host_park_owner=0;
    return 0;
}
static void reset(void) {
    execution_host_once=0;execution_host_park_owner=execution_host_park_running=0;
    execution_host_running=execution_host_peak=execution_host_overlaps=0;
    execution_host_park_depth=0;execution_host_park_held=0;
    execution_host_entry_warnings=execution_host_drain_warnings=0;
    ticks=0;scenario=0;event_ok=event_set=1;
}
int main(void) {
    unsigned n;tsfp_guest_execution_snapshot saved;
    const char *false_words[]={"0","off","OFF","No","fAlSe"};
    for(n=0;n<sizeof false_words/sizeof *false_words;++n) {
        lock_setting=false_words[n];CHECK(!xbox_EnvSwitch("RECOMP_GUEST_LOCK",1));
    }
    lock_setting="";CHECK(xbox_EnvSwitch("RECOMP_GUEST_LOCK",0));
    lock_setting="off ";CHECK(xbox_EnvSwitch("RECOMP_GUEST_LOCK",0));
    CHECK(xbox_EnvSwitch(NULL,23)==23);lock_setting="1";
    xbox_GuestLockInit();xbox_GuestLockInit();CHECK(initializes==1);
    CHECK(xbox_GuestLockOn() && xbox_GuestConcurrencyOn());
    lock_setting=watch_setting=park_setting="0";CHECK(xbox_GuestLockOn());
    xbox_GuestLockEnter();xbox_GuestLockEnter();xbox_GuestLiftedEnter();xbox_GuestLiftedEnter();
    CHECK(locks==2 && execution_host_running==1 && execution_host_park_running==1);
    CHECK(tsfp_guest_execution_suspend(&tsfp_kernel_execution_ops,&saved));
    CHECK(!locks && !execution_host_running && !execution_host_park_running);
    CHECK(tsfp_guest_execution_restore(&tsfp_kernel_execution_ops,&saved));
    CHECK(locks==2 && execution_host_running==1 && execution_host_park_running==1);
    xbox_GuestLiftedExit();CHECK(!locks && !execution_host_running && !execution_host_park_running);
    lock_setting=watch_setting=park_setting="1";reset();
    xbox_DispatchParkOthers();CHECK(execution_host_park_owner==7 && !event_set);
    ticks=0;xbox_DispatchParkOthers();xbox_DispatchReleaseOthers();CHECK(execution_host_park_owner==7 && !ticks);
    xbox_GuestLiftedEnter();CHECK(execution_host_park_running==1 && ticks<5);
    xbox_GuestLiftedLeave();xbox_DispatchReleaseOthers();CHECK(!execution_host_park_owner && event_set);
    xbox_DispatchReleaseOthers();CHECK(event_set);
    /* A foreign owner cannot be stolen, and a skipped request cannot release it. */
    execution_host_park_owner=9;xbox_DispatchParkOthers();
    CHECK(execution_host_park_owner==9 && !execution_host_park_held && ticks<80);
    xbox_DispatchReleaseOthers();CHECK(execution_host_park_owner==9);
    /* Ownership becomes available during the bounded acquire. */
    scenario=2;ticks=0;xbox_DispatchParkOthers();CHECK(execution_host_park_owner==7);
    xbox_DispatchReleaseOthers();scenario=0;
    /* Drain completion and timeout retain the requesting owner's gate. */
    execution_host_park_running=2;scenario=1;xbox_DispatchParkOthers();
    CHECK(!execution_host_park_running && execution_host_park_owner==7);
    xbox_DispatchReleaseOthers();scenario=0;execution_host_park_running=2;ticks=0;
    xbox_DispatchParkOthers();CHECK(execution_host_park_owner==7 && ticks>=50 && ticks<80);
    xbox_DispatchReleaseOthers();execution_host_park_running=0;
    /* Waiters retry admission; timeout remains explicitly best-effort. */
    execution_host_park_owner=9;scenario=3;ticks=0;xbox_GuestLiftedEnter();
    CHECK(!execution_host_park_owner && execution_host_park_running==1 && ticks<100);
    xbox_GuestLiftedLeave();scenario=0;execution_host_park_owner=9;ticks=0;
    xbox_GuestLiftedEnter();CHECK(execution_host_park_running==1 && ticks>=100 && ticks<125);
    xbox_GuestLiftedLeave();CHECK(!execution_host_park_running);execution_host_park_owner=0;
    /* Counter diagnostics observe simultaneous active threads, not nesting. */
    execution_host_activate();execution_host_activate();execution_host_deactivate();execution_host_deactivate();
    CHECK(execution_host_peak==2 && execution_host_overlaps==1 && !execution_host_running);
    xbox_GuestConcurrencyReport();
    reset();event_ok=0;xbox_GuestLockInit();CHECK(!execution_host_park_on);
    xbox_DispatchParkOthers();CHECK(!execution_host_park_depth);
    xbox_GuestLiftedEnter();xbox_GuestLiftedLeave();CHECK(!execution_host_park_running);
    reset();lock_setting=watch_setting=park_setting="0";xbox_GuestLockInit();
    xbox_GuestLockEnter();xbox_GuestLiftedEnter();CHECK(!locks && !execution_host_running);
    xbox_GuestLiftedExit();puts("PASS: original execution host policy");return 0;
}
