/* Original host-thread execution ownership; no guest pointers or host unwinding. */
#ifndef TSFP_GUEST_EXECUTION_STATE_H
#define TSFP_GUEST_EXECUTION_STATE_H
#include <stdint.h>

typedef struct tsfp_guest_execution_ops {
    void *opaque;
    int (*lock_enabled)(void *);
    void (*lock)(void *);
    void (*unlock)(void *);
    void (*activate)(void *);
    void (*deactivate)(void *);
} tsfp_guest_execution_ops;

typedef struct tsfp_guest_execution_snapshot {
    const tsfp_guest_execution_ops *domain;
    uintptr_t owner;
    uint32_t lock_depth,active_depth;
} tsfp_guest_execution_snapshot;

/* Ops/opaque remain live for this host thread's lifetime. Transition callbacks
 * return normally, never execute guest code/reenter this provider, and cannot
 * report partial failure. They change only host lock/parking/accounting state.
 * Native scopes/snapshots are thread-affine, caller-owned, not guest-serializable
 * capabilities. No stale use after thread exit, native fiber transport or guest
 * CPU/TIB identity switching is supplied. An active thread binds one domain.
 * Capture/rejected operations preserve host effects and output. */
int tsfp_guest_execution_capture(const tsfp_guest_execution_ops *,tsfp_guest_execution_snapshot *);
int tsfp_guest_execution_lock_enter(const tsfp_guest_execution_ops *);
int tsfp_guest_execution_lock_leave(const tsfp_guest_execution_ops *);
int tsfp_guest_execution_lock_drop(const tsfp_guest_execution_ops *);
int tsfp_guest_execution_lock_restore(const tsfp_guest_execution_ops *,unsigned depth);
int tsfp_guest_execution_enter(const tsfp_guest_execution_ops *);
int tsfp_guest_execution_leave(const tsfp_guest_execution_ops *);
/* Exact restoration, not additive reacquisition. Leave active accounting before
 * changing lock depth; acquire the desired lock before reactivating. Nested
 * activation counts one running host thread, retaining logical nesting depth.
 * Empty leave is idempotent. Lock-drop returns depth, or-1 on rejection. */
int tsfp_guest_execution_restore(const tsfp_guest_execution_ops *,const tsfp_guest_execution_snapshot *);
/* Capture first, then suspend all active/lock levels. Output changes on success. */
int tsfp_guest_execution_suspend(const tsfp_guest_execution_ops *,tsfp_guest_execution_snapshot *);
#endif
