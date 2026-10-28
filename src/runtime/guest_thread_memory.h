/* Project-owned construction and ownership for guest thread storage. */
#ifndef TSFP_GUEST_THREAD_MEMORY_H
#define TSFP_GUEST_THREAD_MEMORY_H
#include "guest_allocation.h"
#include "guest_image.h"

enum { TSFP_THREAD_STACK_BYTES = 512 * 1024, TSFP_THREAD_STACK_COUNT = 8,
       TSFP_WORKER_STACK_BYTES = 256 * 1024, TSFP_WORKER_STACK_COUNT = 16 };

typedef struct tsfp_thread_memory {
    tsfp_allocations *allocations;
    unsigned char *ram;
    uint32_t ram_size, low_shift, tls_total, next_id;
    uint32_t stacks[TSFP_THREAD_STACK_COUNT];
    uint32_t worker_mask;
    unsigned active;
#if defined(_MSC_VER)
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_thread_memory;

/* Init/reinit requires quiescent callers and a fresh guest memory lifetime.
 * The validated image must already be loaded and the project allocator active
 * on that RAM, with its heap above the bootstrap stack. No output changes on
 * rejection. Main TIB/object/PRCB/TLS are initialized; unrelated RAM is retained.
 * This layer neither maps memory nor starts/stops host threads. */
int tsfp_thread_memory_init(tsfp_thread_memory *, tsfp_allocations *,
                            const tsfp_image_info *);
void tsfp_thread_memory_stop(tsfp_thread_memory *);

/* Clone the current main TIB/TLS/object, rebinding SEH, private TLS slot zero,
 * TLS end, object identity and TIB->object as one allocation. Other fields,
 * including the template's self/stack-limit/object-data pointers, deliberately
 * retain the existing title ABI. Caller must synchronize changes to templates;
 * this provider serializes constructors and metadata, not arbitrary guest writes.
 * No-TLS images return zero, matching the existing caller convention.
 * Published TIB/object memory remains valid for the guest-memory lifetime:
 * guest identity pointers are not reference counted by this layer. */
uint32_t tsfp_thread_tib_create(tsfp_thread_memory *);
uint32_t tsfp_thread_current_object(tsfp_thread_memory *, uint32_t fs_base);

/* Heap-backed stacks track exact returned tops; unmatched releases are ignored.
 * Top is end minus 16. Only the owner may release: this address ABI cannot
 * distinguish a stale release after the same address has been reused. */
uint32_t tsfp_thread_stack_acquire(tsfp_thread_memory *);
int tsfp_thread_stack_release(tsfp_thread_memory *, uint32_t top);

/* Fixed lower-main-stack slices retain index ABI for timer/USB callbacks.
 * Only the owner may release a live index; indices carry no generation token.
 * Acquisition does not clear guest stack bytes. Returns -1 on exhaustion. */
int tsfp_worker_stack_acquire(tsfp_thread_memory *);
int tsfp_worker_stack_release(tsfp_thread_memory *, int index);
#endif
