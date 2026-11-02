/* Original execution-ownership binding. Included after the platform's parking,
 * affinity and diagnostic declarations. Those host policies remain separate. */
#include "guest_kernel_execution.h"
static int execution_lock_enabled(void *opaque) {
    (void)opaque;return g_guest_cs_ready && xbox_GuestLockOn();
}
static void execution_lock(void *opaque) {(void)opaque;EnterCriticalSection(&g_guest_cs);}
static void execution_unlock(void *opaque) {(void)opaque;LeaveCriticalSection(&g_guest_cs);}
static void execution_activate(void *opaque) {
    LONG running;(void)opaque;
#ifdef _WIN32
    static RECOMP_TLS unsigned affinity_initialized;
    if(!affinity_initialized) {
        DWORD_PTR mask=guest_cpu_mask();affinity_initialized=1;
        if(mask)SetThreadAffinityMask(GetCurrentThread(),mask);
    }
#endif
    park_enter();
    if(!xbox_GuestConcurrencyOn())return;
    running=InterlockedIncrement(&g_lifted_now);
    if(running>1)InterlockedIncrement(&g_lifted_overlaps);
    for(;;) {
        LONG peak=g_lifted_peak;
        if(peak>=running || InterlockedCompareExchange(&g_lifted_peak,running,peak)==peak)break;
    }
}
static void execution_deactivate(void *opaque) {
    (void)opaque;
    if(park_on())InterlockedDecrement(&g_park_running);
    if(xbox_GuestConcurrencyOn())InterlockedDecrement(&g_lifted_now);
}
const tsfp_guest_execution_ops tsfp_kernel_execution_ops={
    NULL,execution_lock_enabled,execution_lock,execution_unlock,execution_activate,execution_deactivate
};
static void execution_require(int ok) {
    if(!ok){fputs("[guest-execution] invalid native execution ownership\n",stderr);abort();}
}
void xbox_GuestLockEnter(void){execution_require(tsfp_guest_execution_lock_enter(&tsfp_kernel_execution_ops));}
void xbox_GuestLockLeave(void){execution_require(tsfp_guest_execution_lock_leave(&tsfp_kernel_execution_ops));}
int xbox_GuestLockDrop(void) {
    int depth=tsfp_guest_execution_lock_drop(&tsfp_kernel_execution_ops);
    execution_require(depth>=0);return depth;
}
void xbox_GuestLockRestore(int depth) {
    execution_require(depth>=0 && tsfp_guest_execution_lock_restore(&tsfp_kernel_execution_ops,(unsigned)depth));
}
void xbox_GuestLiftedEnter(void){execution_require(tsfp_guest_execution_enter(&tsfp_kernel_execution_ops));}
void xbox_GuestLiftedLeave(void){execution_require(tsfp_guest_execution_leave(&tsfp_kernel_execution_ops));}
void xbox_GuestLiftedExit(void) {
    tsfp_guest_execution_snapshot discarded;
    execution_require(tsfp_guest_execution_suspend(&tsfp_kernel_execution_ops,&discarded));
}
