/* Original bounded one-argument translated callback invocation. */
#ifndef TSFP_GUEST_CALLBACK_CALL_H
#define TSFP_GUEST_CALLBACK_CALL_H
#include "recomp_types.h"
typedef struct {
    /* Host lookup/mapping callbacks preserve guest architectural state. */
    void *context;
    /* Only allowed translated/manual guest routines, not arbitrary host/code pointers. */
    tsfp_guest_entry_t (*lookup)(void *,uint32_t);
    void *(*output)(void *,uint32_t,uint32_t);
} tsfp_guest_callback_ops;
/* Returns 1 on a normal stdcall return with balanced guest stack. Failure before
 * entry preserves register/stack bytes. A returning malformed callee still has
 * its register effects restored, but reports 0 (caller must quarantine ticket).
 * All mapped pointers remain valid through invocation. Nonlocal guest transfer
 * bypasses restoration intentionally: the continuation owns target registers.
 * Caller arms a cleanup scope before invocation. No native OS/fiber switch,
 * worker join, guest exception delivery or callback body is supplied here. */
int tsfp_guest_callback_one(const tsfp_guest_callback_ops *,uint32_t routine,uint32_t argument);
#endif
