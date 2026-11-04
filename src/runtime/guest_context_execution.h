/* Original native execution checkpoints for validated guest-PC continuations. */
#ifndef TSFP_GUEST_CONTEXT_EXECUTION_H
#define TSFP_GUEST_CONTEXT_EXECUTION_H
#include "guest_x86_context.h"
#include <setjmp.h>
typedef struct tsfp_context_execution_ops {
    void *opaque;
    /* Except step, integer callbacks return nonzero for success/true. Admission and
     * ownership callbacks do not execute guest code or reenter this engine.
     * Admission preserves all live guest state and ownership. */
    int (*allow_pc)(void *,uint32_t);
    int (*allow_state)(void *,const tsfp_x86_context_state *);
    /* A prevalidated state is applied without failure or guest calls. */
    void (*apply)(void *,const tsfp_x86_context_state *);
    /* Return exactly STEP_ERROR, STEP_YIELD or STEP_RETURN. Only RETURN to
     * the caller stop PC completes a root; a jump/transfer to that address
     * executes it normally. No automatic native scope may outlive a step. */
    int (*step)(void *,uint32_t,uint32_t *);
    /* Both NULL only when steps own no abandonable native resources. */
    uint64_t (*mark)(void *);
    int (*rewind)(void *,uint64_t);
    int (*same_stack)(void *,uintptr_t native_tag);
    /* Optional foreign-fiber adapter. Must transfer to the supplied checkpoint
     * without returning or later reentering abandoned native frames. */
    void (*foreign_jump)(void *,jmp_buf *,int,uintptr_t native_tag);
} tsfp_context_execution_ops;
enum {TSFP_CONTEXT_STEP_ERROR=0,TSFP_CONTEXT_STEP_YIELD=1,
      TSFP_CONTEXT_STEP_RETURN=2};
enum {TSFP_CONTEXT_EXECUTE_REJECTED=0,TSFP_CONTEXT_EXECUTE_COMPLETE=1,
      TSFP_CONTEXT_EXECUTE_STEP_FAILED=-1,TSFP_CONTEXT_EXECUTE_BAD_PC=-2,
      TSFP_CONTEXT_EXECUTE_CLEANUP_FAILED=-3};
/* Bounds are a caller-owned guest-stack domain, not a host-pointer interval.
 * Its entry ESP must hold a complete four-byte return slot. Nested roots admit
 * continuation ESP<=their entry ESP; the oldest root can admit the full domain.
 * Host ops/opaque storage and admitted code/mappings remain stable until return.
 * Abandoned native frames must permit C longjmp; this layer supplies no C++/
 * Objective-C destructor unwinding. Rewind owns their explicit resources.
 * Nested roots abandoned by a transfer never return. Native metadata is private,
 * noncopyable and cannot be constructed from guest bytes. No ISA decoding,
 * snapshot capture, live-register adapter or SEH dispatcher is supplied here. */
int tsfp_context_execute(const tsfp_context_execution_ops *,
    const tsfp_x86_context_state *,uint64_t stack_low,uint64_t stack_high,uint32_t stop_pc);
/* Reject returns0 without changing guest state/scopes/root metadata. An admitted
 * request never returns: cleanup happens before native transfer; apply occurs at
 * the selected live checkpoint. Scope ledgers of each abandoned root and the
 * destination are rewound deepest first. Cleanup failure exits that root with a distinct
 * error and never resumes either abandoned code or the requested guest target.
 * Caller state is copied before callbacks; no guest address becomes a host PC. */
int tsfp_context_transfer(const tsfp_x86_context_state *);
unsigned tsfp_context_execution_depth(void);
#endif
