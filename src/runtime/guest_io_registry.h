#ifndef TSFP_GUEST_IO_REGISTRY_H
#define TSFP_GUEST_IO_REGISTRY_H
#include "guest_io_owner.h"
typedef struct {uint32_t address;tsfp_io_owner *owner;} tsfp_io_registry_entry;
typedef struct {
    tsfp_io_registry_entry *entries;size_t count;int closed;
    void *context;void (*lock)(void *);void (*unlock)(void *);
} tsfp_io_registry;
/* Fresh quiescent initialization; registry storage and entry array must be
 * disjoint and outlive all calls. Locks must differ from each owner's lock.
 * Owner control blocks remain valid through their client/internal refs; their
 * final disposal may reclaim the control block. Registry code never accesses
 * the owner after releasing its reference. See guest_io_owner.h for API lifetime.
 * Lock order is registry -> owner. No owner callback executes under its lock.
 * Published entries each hold a client ref; lookup returns another owned ref.
 * Caller transfers no refs on publish failure and releases its own ref normally.
 * Entries/owner addresses may not be externally modified while registered.
 * Guest addresses are pointer identities, not generation-bearing handles. The
 * actual guest allocator must prevent reuse until disposal completes. */
int tsfp_io_registry_init(tsfp_io_registry *,tsfp_io_registry_entry *,size_t,
    void *,void (*)(void *),void (*)(void *));
int tsfp_io_registry_publish(tsfp_io_registry *,tsfp_io_owner *);
tsfp_io_owner *tsfp_io_registry_acquire(tsfp_io_registry *,uint32_t);
int tsfp_io_registry_remove(tsfp_io_registry *,uint32_t);
/* Close admission, detach entries, then release outside the lock. Existing
 * acquired refs and pending operations survive. Caller must separately complete
 * or cancel operations and join workers; this is not whole-runtime shutdown. */
void tsfp_io_registry_stop(tsfp_io_registry *);
#endif
