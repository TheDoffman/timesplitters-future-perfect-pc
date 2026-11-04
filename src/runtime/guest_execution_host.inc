/* Original host scheduling policy. Requires the platform's standard Win32 API
 * adapter (native Windows or the shared compatibility layer). Process lifetime
 * storage is intentional: guest threads must be quiescent before process exit. */
#include <limits.h>
#include <errno.h>
static INIT_ONCE execution_host_once=INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION execution_host_lock;
static int execution_host_lock_on,execution_host_watch,execution_host_park_on;
static volatile LONG execution_host_running,execution_host_peak,execution_host_overlaps;
static volatile LONG execution_host_park_running,execution_host_park_owner;
static volatile LONG execution_host_drain_warnings,execution_host_entry_warnings;
static HANDLE execution_host_free;
static RECOMP_TLS unsigned execution_host_park_depth;
static RECOMP_TLS int execution_host_park_held;
#ifdef _WIN32
#include "guest_cpu_choice.h"
static DWORD_PTR execution_host_cpu;
#include "guest_execution_cpu.inc"
#endif

static int execution_false_word(const char *value,const char *word) {
    unsigned char a,b;
    do {
        a=(unsigned char)*value++;b=(unsigned char)*word++;
        if(a>='A' && a<='Z')a=(unsigned char)(a+('a'-'A'));
        if(a!=b)return 0;
    } while(a);
    return 1;
}
int xbox_EnvSwitch(const char *name,int fallback) {
    const char *value=name?getenv(name):NULL;
    return value?!(execution_false_word(value,"0") || execution_false_word(value,"off") ||
                  execution_false_word(value,"no") || execution_false_word(value,"false")):fallback;
}
static BOOL WINAPI execution_host_initialize(PINIT_ONCE once,PVOID parameter,PVOID *context) {
    (void)once;(void)parameter;(void)context;
    InitializeCriticalSection(&execution_host_lock);
    execution_host_lock_on=xbox_EnvSwitch("RECOMP_GUEST_LOCK",0);
    execution_host_watch=xbox_EnvSwitch("RECOMP_GUEST_CONCURRENCY",0);
    execution_host_park_on=xbox_EnvSwitch("RECOMP_DISPATCH_LOCK",0);
    if(execution_host_park_on) {
        execution_host_free=CreateEventW(NULL,TRUE,TRUE,NULL);
        if(!execution_host_free) {
            execution_host_park_on=0;
            fputs("[guest-execution] dispatch parking unavailable: event creation failed\n",stderr);
        }
    }
#ifdef _WIN32
    execution_host_cpu=execution_cpu_configure();
#endif
    if(execution_host_lock_on)fputs("  Guest lock: on -- one guest thread runs lifted code at a time\n",stderr);
    return TRUE;
}
static void execution_host_ready(void) {
    if(!InitOnceExecuteOnce(&execution_host_once,execution_host_initialize,NULL,NULL))abort();
}
int xbox_GuestLockOn(void){execution_host_ready();return execution_host_lock_on;}
int xbox_GuestConcurrencyOn(void){execution_host_ready();return execution_host_watch;}
void xbox_GuestLockInit(void){execution_host_ready();}
static LONG execution_host_read(volatile LONG *cell){return InterlockedCompareExchange(cell,0,0);}
static void execution_host_warn(volatile LONG *count,const char *message) {
    if(InterlockedIncrement(count)<=3)fputs(message,stderr);
}
void xbox_DispatchParkOthers(void) {
    LONG me;ULONGLONG start;
    execution_host_ready();if(!execution_host_park_on)return;
    if(execution_host_park_depth==UINT_MAX)abort();
    if(execution_host_park_depth++)return;
    me=(LONG)GetCurrentThreadId();start=GetTickCount64();
    for(;;) {
        if(InterlockedCompareExchange(&execution_host_park_owner,me,0)==0)break;
        if(GetTickCount64()-start>=50) {
            execution_host_warn(&execution_host_drain_warnings,
                "[guest-execution] dispatch owner busy after 50 ms; continuing without parking\n");
            return;
        }
        SwitchToThread();
    }
    execution_host_park_held=1;
    /* The gate event is only a wake hint. Atomic owner admission is authoritative. */
    ResetEvent(execution_host_free);start=GetTickCount64();
    while(execution_host_read(&execution_host_park_running)>0) {
        if(GetTickCount64()-start>=50) {
            execution_host_warn(&execution_host_drain_warnings,
                "[guest-execution] guest threads did not drain after 50 ms; continuing\n");
            return;
        }
        SwitchToThread();
    }
}
void xbox_DispatchReleaseOthers(void) {
    execution_host_ready();if(!execution_host_park_on || !execution_host_park_depth)return;
    if(--execution_host_park_depth || !execution_host_park_held)return;
    execution_host_park_held=0;
    if(InterlockedCompareExchange(&execution_host_park_owner,0,(LONG)GetCurrentThreadId())!=
       (LONG)GetCurrentThreadId())abort();
    SetEvent(execution_host_free);
}
static void execution_host_activate(void) {
    LONG running,peak;ULONGLONG start;LONG me;
    execution_host_ready();
#ifdef _WIN32
    {
        static RECOMP_TLS int pinned;
        if(!pinned){pinned=1;if(execution_host_cpu)SetThreadAffinityMask(GetCurrentThread(),execution_host_cpu);}
    }
#endif
    if(execution_host_park_on) {
        start=GetTickCount64();me=(LONG)GetCurrentThreadId();
        for(;;) {
            LONG owner;
            InterlockedIncrement(&execution_host_park_running);
            owner=execution_host_read(&execution_host_park_owner);
            if(!owner || owner==me)break;
            InterlockedDecrement(&execution_host_park_running);
            if(GetTickCount64()-start>=100) {
                execution_host_warn(&execution_host_entry_warnings,
                    "[guest-execution] dispatch owner held 100 ms; allowing guest progress\n");
                InterlockedIncrement(&execution_host_park_running);break;
            }
            WaitForSingleObject(execution_host_free,10);
        }
    }
    if(!execution_host_watch)return;
    running=InterlockedIncrement(&execution_host_running);
    if(running>1)InterlockedIncrement(&execution_host_overlaps);
    peak=execution_host_read(&execution_host_peak);
    while(peak<running) {
        LONG actual=InterlockedCompareExchange(&execution_host_peak,running,peak);
        if(actual==peak)break;peak=actual;
    }
}
static void execution_host_deactivate(void) {
    if(execution_host_park_on && InterlockedDecrement(&execution_host_park_running)<0)abort();
    if(execution_host_watch && InterlockedDecrement(&execution_host_running)<0)abort();
}
void xbox_GuestConcurrencyReport(void) {
    if(xbox_GuestConcurrencyOn())fprintf(stderr,
        "  [CONCURRENCY] peak guest threads in lifted code: %ld; entries that found another already there: %ld\n",
        (long)execution_host_read(&execution_host_peak),(long)execution_host_read(&execution_host_overlaps));
}
