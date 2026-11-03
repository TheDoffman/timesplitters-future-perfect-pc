/* Original execution-state cleanup attached to the object-operation ledger. */
#ifndef TSFP_GUEST_EXECUTION_SCOPE_H
#define TSFP_GUEST_EXECUTION_SCOPE_H
#include "guest_execution_state.h"
#include "guest_object_scope.h"
typedef struct tsfp_guest_execution_scope {
    tsfp_guest_object_scope objects;
    tsfp_guest_execution_snapshot saved;
    tsfp_guest_object_scopes *stack;
} tsfp_guest_execution_scope;
/* Caller-owned scope remains live through ordinary leave or pre-transfer unwind.
 * Cleanup releases leases before exact execution-state restoration. The existing
 * object lifetime mark/abort hooks therefore also restore execution ownership.
 * Native callbacks must honor the state provider contract. This does not unwind
 * C++/Objective-C/OS frames or pin a foreign native stack. */
int tsfp_guest_execution_scope_enter(tsfp_guest_object_scopes *,tsfp_guest_execution_scope *,
                                    tsfp_guest_objects *,const tsfp_guest_execution_ops *);
int tsfp_guest_execution_scope_leave(tsfp_guest_object_scopes *,tsfp_guest_execution_scope *);
/* Use this when abandoning execution scopes. Drain all leases/cleanup while
 * suspended, then restore the oldest abandoned execution snapshot once. A stale
 * mark rejects before host effects. Cleanup failures still drain remaining
 * scopes and attempt restoration, then report failure. mark0 drains all. */
int tsfp_guest_execution_scope_unwind(tsfp_guest_object_scopes *,uint64_t mark,
                                     const tsfp_guest_execution_ops *);
/* Thread exit: drain all scopes while suspended and remain inactive/unlocked.
 * Never reacquire an abandoned caller's lock merely to terminate the thread. */
int tsfp_guest_execution_scope_abort(tsfp_guest_object_scopes *,const tsfp_guest_execution_ops *);
#endif
