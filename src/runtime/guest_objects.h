/* Original guest handle/object ownership provider. */
#ifndef TSFP_GUEST_OBJECTS_H
#define TSFP_GUEST_OBJECTS_H
#include <stddef.h>
#include <stdint.h>

enum { TSFP_OBJECT_OPEN=1, TSFP_OBJECT_CLOSING, TSFP_OBJECT_RETRY,
       TSFP_OBJECT_UNCERTAIN, TSFP_OBJECT_PREPARING };
typedef struct tsfp_guest_handle_record {
    uint32_t generation, object;
    unsigned used, exhausted;
} tsfp_guest_handle_record;
typedef struct tsfp_guest_object_record {
    uintptr_t host;
    uint32_t generation, handles, refs, leases, kind, flags, dispatcher;
    unsigned state;
} tsfp_guest_object_record;
typedef struct tsfp_guest_objects {
    tsfp_guest_handle_record *handles;
    tsfp_guest_object_record *objects;
    size_t capacity;
    void *context;
    /* 1 consumed; 0 definitely retained/retryable; -1 ownership uncertain.
     * Called outside metadata lock. Never retry an uncertain resource. */
    int (*close_host)(void *, uintptr_t);
    /* Kind/flags stay available through cleanup and retries. The adapter can
     * retain thread-memory-lifetime bodies while freeing owned event bodies. */
    int (*free_dispatcher)(void *, uint32_t address,uint32_t kind,uint32_t flags);
#if defined(_MSC_VER)
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_guest_objects;
typedef struct tsfp_guest_object_lease {
    tsfp_guest_objects *owner;
    uint64_t key;
    uintptr_t host;
    uint32_t kind, flags, dispatcher;
} tsfp_guest_object_lease;

/* Initialization requires quiescent, distinct caller-owned storage. No reset
 * or implicit shutdown: outstanding ownership must be drained explicitly. */
int tsfp_guest_objects_init(tsfp_guest_objects *, tsfp_guest_handle_record *,
    tsfp_guest_object_record *, size_t, int (*)(void *,uintptr_t),
    int (*)(void *,uint32_t,uint32_t,uint32_t), void *);
/* Adoption consumes exactly one host reference on success, none on failure.
 * No raw-pointer deduplication. Call only for a separately owned reference. */
int tsfp_guest_objects_adopt(tsfp_guest_objects *, uintptr_t, uint32_t kind,
    uint32_t flags, uint32_t *token);
/* Reserve both capacities before an OS creation with observable side effects.
 * Tickets are not resolvable handles until commit; cancellation advances the
 * token generation. Commit consumes one host reference only on success. */
int tsfp_guest_objects_prepare(tsfp_guest_objects *,uint32_t kind,uint32_t flags,uint32_t *ticket);
int tsfp_guest_objects_commit(tsfp_guest_objects *,uint32_t ticket,uintptr_t host);
int tsfp_guest_objects_cancel(tsfp_guest_objects *,uint32_t ticket);
/* Same-process guest duplication shares an object and its host reference.
 * close_source consumes a valid source even if token capacity is exhausted. */
int tsfp_guest_objects_duplicate(tsfp_guest_objects *, uint32_t, int close_source,
    uint32_t *token);
/* Same operation, with 1 success, 0 invalid source, -1 exhausted capacity. */
int tsfp_guest_objects_duplicate_checked(tsfp_guest_objects *,uint32_t,int,uint32_t *);
/* Open a real token for an already retained object (including a thread's
 * current-object pseudo handle). Caller holds a reference or lease. */
int tsfp_guest_objects_open_key(tsfp_guest_objects *, uint64_t, uint32_t *token);
int tsfp_guest_objects_close(tsfp_guest_objects *, uint32_t);
/* Lease storage must initially be zero, must not be copied, and is released
 * exactly once. A closed token cannot acquire new leases. */
int tsfp_guest_objects_acquire(tsfp_guest_objects *, uint32_t, tsfp_guest_object_lease *);
int tsfp_guest_objects_acquire_key(tsfp_guest_objects *, uint64_t, tsfp_guest_object_lease *);
int tsfp_guest_objects_release(tsfp_guest_object_lease *);
int tsfp_guest_objects_reference(tsfp_guest_objects *, uint32_t, uint64_t *key);
int tsfp_guest_objects_reference_dispatcher(tsfp_guest_objects *, uint32_t, uint64_t *key);
int tsfp_guest_objects_dereference_dispatcher(tsfp_guest_objects *, uint32_t);
int tsfp_guest_objects_retain(tsfp_guest_objects *, uint64_t);
int tsfp_guest_objects_dereference(tsfp_guest_objects *, uint64_t);
/* Caller keeps a reference or lease while publishing. Returns existing winner
 * if another caller won; caller owns disposal of an unused candidate. */
int tsfp_guest_objects_publish(tsfp_guest_objects *, uint64_t, uint32_t candidate,
    uint32_t *winner);
/* Retry definitely retained closes once per object. Report remaining detached
 * resources, including uncertain ones. No callback runs under the lock. */
size_t tsfp_guest_objects_collect(tsfp_guest_objects *);
#endif
