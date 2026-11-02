/* Original execution-ownership binding, after guest_execution_host.inc. */
#include "guest_kernel_execution.h"
static int execution_lock_enabled(void *opaque) {(void)opaque;return xbox_GuestLockOn();}
static void execution_lock(void *opaque) {(void)opaque;EnterCriticalSection(&execution_host_lock);}
static void execution_unlock(void *opaque) {(void)opaque;LeaveCriticalSection(&execution_host_lock);}
static void execution_activate(void *opaque) {(void)opaque;execution_host_activate();}
static void execution_deactivate(void *opaque) {(void)opaque;execution_host_deactivate();}
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
