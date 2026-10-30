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
/* Bounded original stdcall entry for driver callbacks, at most four guest words.
 * Argument array and optional result are stable host storage, outside mapped
 * guest stack bytes and architectural globals. Result is written only after a
 * balanced return; it receives guest EAX before architectural state restoration.
 * Caller owns callback/device/request scopes across nonlocal guest transfer. */
int tsfp_guest_callback_stdcall(const tsfp_guest_callback_ops *,uint32_t routine,
    const uint32_t *arguments,unsigned count,uint32_t *result);
/* Host-only, noncopyable prepared frame; all mappings/entry remain valid until
 * invocation. Prepare changes no guest bytes/state and preserves output on
 * failure. Invocation consumes the plan before admission, even if rejected.
 * admit is optional, metadata-only, normal-returning and preserves guest state;
 * successful admission publishes ownership immediately before frame writes and
 * entry. Caller arms nonlocal cleanup first. No guest/host pointer capability
 * may be constructed from untrusted plan bytes. */
typedef struct {
    tsfp_guest_entry_t entry;unsigned char *frame;
    uint32_t esp,address,bytes,words[4];unsigned count;
} tsfp_guest_callback_plan;
int tsfp_guest_callback_prepare(const tsfp_guest_callback_ops *,uint32_t,const uint32_t *,unsigned,tsfp_guest_callback_plan *);
int tsfp_guest_callback_invoke(tsfp_guest_callback_plan *,int (*admit)(void *),void *,uint32_t *result);
#endif
