/* Original persistent device FIFO registry and scoped API lifetime. */
#ifndef TSFP_GUEST_IO_SCHEDULER_H
#define TSFP_GUEST_IO_SCHEDULER_H
#include "guest_io_queue_wire.h"
#include "guest_io_startio.h"
enum {TSFP_IO_QUEUE_FREE,TSFP_IO_QUEUE_PREPARING,TSFP_IO_QUEUE_LIVE,
    TSFP_IO_QUEUE_RETIRING,TSFP_IO_QUEUE_QUARANTINED};
typedef struct {
    tsfp_io_queue queue;tsfp_io_queue_wire wire;tsfp_guest_object_lease device;
    uint64_t generation;uint32_t body,references;unsigned state,closed;
} tsfp_io_schedule_record;
typedef struct {
    tsfp_device_factory *factory;tsfp_io_schedule_record *records;
    tsfp_io_owner **entries;size_t capacity,waiting_capacity;unsigned closed;
    void *context;void (*lock)(void *);void (*unlock)(void *);
} tsfp_io_scheduler;
typedef struct {
    tsfp_io_scheduler *manager;size_t index;uint64_t generation;unsigned active;
} tsfp_io_queue_ticket;
typedef struct {
    tsfp_guest_object_scope scope;tsfp_io_queue_ticket ticket;
    tsfp_guest_object_scopes *stack;
} tsfp_io_queue_scope;
/* Fresh disjoint caller-owned storage: records[capacity] and
 * entries[capacity*waiting_capacity]. Context/factory/objects/storage outlive all
 * tickets, preparing/current/pending work and quarantined cleanup. One metadata
 * lock protects registry and all queues; it differs from request/object/factory
 * locks. Queue -> owner ordering. No guest code executes under metadata locks.
 * Device leases are released outside locks. No allocation/free callbacks needed.
 */
int tsfp_io_scheduler_init(tsfp_io_scheduler *,tsfp_device_factory *,
    tsfp_io_schedule_record *,size_t,tsfp_io_owner **,size_t,
    void *,void (*)(void *),void (*)(void *));
/* Zero noncopyable ticket; release once. create permits new queue registration
 * only on an undeleted, owned device. Preparing/retiring records reject lookup;
 * a closed live record remains accessible with create=0 for final next/cleanup.
 * No success is fabricated on capacity, ref/generation exhaustion or contention
 * with preparation. Failure preserves output and transfers no ticket.
 * A live registry holds a device lease until close + empty + all API refs drain.
 * Call close for device deletion, even when idle, before destroying the factory.
 */
int tsfp_io_scheduler_acquire(tsfp_io_scheduler *,uint32_t device,int create,tsfp_io_queue_ticket *);
int tsfp_io_scheduler_release(tsfp_io_queue_ticket *);
/* Borrow caller request ref. Require matching request-owned bindings and pin
 * this device there before admission, so draining a closed queue cannot release
 * the last device lease of an independently pending operation. */
int tsfp_io_scheduler_submit(tsfp_io_queue_ticket *,tsfp_io_owner *,tsfp_io_devices *,tsfp_io_owner **selected);
int tsfp_io_scheduler_next(tsfp_io_queue_ticket *,tsfp_io_owner *expected,tsfp_io_owner **selected);
/* Close admission and drain waiters, retaining current and API refs. No fabricated
 * cancellation/completion. Preparing creators observe close before publication.
 * Stop closes every record; active tickets still may retire a known current.
 * Callers independently complete/cancel operations and join users before teardown.
 * Failed device-lease release quarantines the record; never automatically retry.
 */
int tsfp_io_scheduler_close(tsfp_io_scheduler *,uint32_t device);
void tsfp_io_scheduler_stop(tsfp_io_scheduler *);
void tsfp_io_scheduler_counts(tsfp_io_scheduler *,size_t *live,size_t *quarantined);
/* Thread-affine scope guards an API ticket across guest callbacks/nonlocal exits.
 * Lifetime hooks must unwind before C frames disappear; normal end still required.
 * Helpers return normally without guest code except explicit startio below.
 */
int tsfp_io_queue_scope_begin(tsfp_guest_object_scopes *,tsfp_io_queue_scope *,tsfp_io_scheduler *,uint32_t,int create);
int tsfp_io_queue_scope_end(tsfp_guest_object_scopes *,tsfp_io_queue_scope *);
/* Consumes caller request client ref, including invalid-frame paths. FIFO
 * selection/next return that extra ref. Requires matching request-owned device
 * bindings and external guest serialization. Does not select work or implement
 * keyed/cancel/event/APC/storage policies, adoption or a kernel import route.
 */
int tsfp_io_scheduler_startio(tsfp_guest_object_scopes *,tsfp_io_queue_scope *,
    tsfp_io_owner *,tsfp_io_devices *,const tsfp_io_call_ops *);
#endif
