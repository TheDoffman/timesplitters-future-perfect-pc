/* Original checked i386 registration-chain exception dispatch and unwind. */
#ifndef TSFP_GUEST_EXCEPTION_H
#define TSFP_GUEST_EXCEPTION_H
#include "guest_x86_context.h"

enum {TSFP_EXCEPTION_RECORD_BYTES=80,TSFP_EXCEPTION_GUARD_BYTES=16};
enum {TSFP_EXCEPTION_CONTINUE=0,TSFP_EXCEPTION_SEARCH=1,
      TSFP_EXCEPTION_NESTED=2,TSFP_EXCEPTION_COLLIDED=3};
enum {TSFP_EXCEPTION_NONCONTINUABLE=1,TSFP_EXCEPTION_UNWINDING=2,
      TSFP_EXCEPTION_EXIT_UNWIND=4,TSFP_EXCEPTION_STACK_INVALID=8,
      TSFP_EXCEPTION_NESTED_CALL=16,TSFP_EXCEPTION_COLLIDED_UNWIND=64};
enum {TSFP_EXCEPTION_BAD_ARGUMENT=0,TSFP_EXCEPTION_CONTINUED=1,
      TSFP_EXCEPTION_UNWOUND=2,TSFP_EXCEPTION_UNHANDLED=3,
      TSFP_EXCEPTION_BAD_CHAIN=-1,TSFP_EXCEPTION_BAD_HANDLER=-2,
      TSFP_EXCEPTION_BAD_DISPOSITION=-3,TSFP_EXCEPTION_CANNOT_CONTINUE=-4,
      TSFP_EXCEPTION_BAD_TARGET=-5,TSFP_EXCEPTION_CALLBACK_FAILED=-6,
      TSFP_EXCEPTION_BAD_CONTEXT=-7};

typedef struct tsfp_exception_ops {
    void *opaque;
    /* Checked stable ordinary guest memory; no MMIO or mapping creation.
     * writable=0 reads; writable=1 requires read/write access. */
    void *(*resolve)(void *,uint32_t address,uint32_t bytes,int writable);
    /* Admit a guest routine identifier, never convert guest bytes to a host PC. */
    int (*handler)(void *,uint32_t routine);
    /* Invoke an admitted cdecl handler with these four guest arguments. On
     * balanced normal return, write its disposition and return nonzero. Native
     * nonlocal transfer must abandon this call, not resume an abandoned walker.
     * Handler execution owns its native resource and execution checkpoints. */
    int (*invoke)(void *,uint32_t routine,uint32_t record,uint32_t frame,
                  uint32_t context,uint32_t dispatcher,uint32_t *disposition);
} tsfp_exception_ops;

typedef struct tsfp_exception_request {
    uint32_t head,record,context,guard,dispatcher,guard_handler;
    /* Used only when unwind's record is zero. Must be separate writable memory. */
    uint32_t fallback_record;
    uint64_t stack_low,stack_high;
    /* Full caller snapshot, copied before callbacks. Unselected CONTEXT groups
     * retain this state instead of being zeroed during continuation. */
    const tsfp_x86_context_state *baseline;
} tsfp_exception_request;

/* All workspace addresses are nonzero, disjoint, ordinary mapped guest storage.
 * Initial CONTEXT includes control+integer groups. Guard is an aligned caller-owned
 * 16-byte stack record below registrations: next, admitted guard routine,
 * protected frame, unwind mode. Dispatcher is a writable establisher-frame word.
 * Each recursive dispatch needs distinct live workspace. Caller keeps mappings,
 * stack ownership, ops/request and output stable throughout callbacks.
 *
 * A temporary guard precedes the actual live chain during each invocation.
 * Raise retains every registration; unwind removes a node only AFTER its handler
 * returns, and retains/skips the target. Guard routines/foreign native callback
 * barriers are supplied by the adapter; this layer implements no host longjmp.
 * Initial rejection preserves guest bytes/output. After dispatch begins,
 * exception/context/chain edits and handler effects are not rolled back on error.
 * Only successful CONTINUED/UNWOUND writes the host continuation output. The
 * caller must actually restore/transfer it, or report failure; these functions
 * do not resume live guest execution or raise a second-chance host exception. */
int tsfp_exception_raise(const tsfp_exception_ops *,const tsfp_exception_request *,
                         tsfp_x86_context_state *continuation);
/* ReturnValue is published in context EAX before callbacks. TargetIp is not an
 * input: measured native i386 RtlUnwind ignores it; do not generalize to other
 * architectures. Null target unwinds to chain end and sets EXIT_UNWIND. */
int tsfp_exception_unwind(const tsfp_exception_ops *,const tsfp_exception_request *,
                          uint32_t target_frame,uint32_t return_value,
                          tsfp_x86_context_state *continuation);
#endif
