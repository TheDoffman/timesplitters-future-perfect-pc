#ifndef TSFP_GUEST_DEVICE_FACTORY_H
#define TSFP_GUEST_DEVICE_FACTORY_H
#include "guest_object_services.h"
#include "guest_object_names.h"
enum {TSFP_DEVICE_OK,TSFP_DEVICE_BAD_INPUT,TSFP_DEVICE_BAD_ADDRESS,
    TSFP_DEVICE_NO_MEMORY,TSFP_DEVICE_COLLISION,TSFP_DEVICE_INITIALIZATION,TSFP_DEVICE_CLOSED};
typedef struct tsfp_device_node tsfp_device_node;
/* Stable value ticket: no borrowed host pointer crosses guest execution. */
typedef struct {uint64_t key;uint32_t body,driver;} tsfp_device_finalization;
typedef struct {
    void *context;void (*lock)(void *),(*unlock)(void *);
    void *(*metadata_allocate)(void *,size_t);void (*metadata_free)(void *,void *);
    uint32_t (*allocate)(void *,uint32_t);int (*deallocate)(void *,uint32_t);
    const void *(*input)(void *,uint32_t,uint32_t);
    void *(*output)(void *,uint32_t,uint32_t);
    int (*initialize)(void *,uint32_t body,void *mapping,uint32_t bytes,
        uint32_t driver,uint32_t extension,uint32_t device_type,int named,int exclusive);
    void (*mark_deleted)(void *,uint32_t body,void *mapping,uint32_t bytes);
    /* Defer successfully exposed devices until external callback dispatch is
     * acknowledged. No guest execution occurs inside object disposal. */
    int defer_finalization;
} tsfp_device_factory_ops;
typedef struct {
    tsfp_guest_objects *objects;tsfp_guest_names *names;tsfp_device_factory_ops ops;
    tsfp_device_node *head;size_t live,limit;uint32_t prefix_bytes;int closed;
} tsfp_device_factory;
typedef struct {tsfp_device_factory *factory;tsfp_device_finalization ticket;} tsfp_device_final_pin;
/* Original storage/name transaction layer, NOT the IoCreateDevice kernel ABI.
 * initialize/mark_deleted supply separately validated guest serialization; they
 * never enter guest execution. All callbacks run outside the factory lock.
 * Driver table/mappings/caller buffers remain valid and stable as required by
 * their lifetime; newly allocated spans cannot overlap caller-owned buffers.
 * The lock differs from object/name locks. Context, index, owner and factory
 * storage outlive callers/leases/pending cleanup. Fresh quiescent init only.
 * deallocate:1 consumed,0 retained/retryable,-1 uncertain/never blindly retry.
 * Unique guest allocations and aligned host metadata are callback contracts. */
int tsfp_device_factory_init(tsfp_device_factory *,tsfp_guest_objects *,tsfp_guest_names *,
    uint32_t prefix_bytes,size_t limit,const tsfp_device_factory_ops *);
/* Counted name descriptor is optional; supplied descriptors are8 guest bytes.
 * The factory retains one creator reference until delete (the returned address
 * does not transfer that reference to arbitrary Ob dereference callers). Each
 * name binding owns another; explicit guest references are separately balanced.
 * Failure preserves the caller output; cleanup failures remain counted. */
int tsfp_device_factory_create(tsfp_device_factory *,uint32_t driver,uint32_t extension,
    uint32_t name_descriptor,uint32_t device_type,int exclusive,uint32_t output);
/* Borrow a published device allocation. Caller supplies a zero lease, then
 * releases it once. Deletion removes names/creator but existing references and
 * leases keep storage valid. Returned mapping/span remain valid through lease.
 * Failure preserves all outputs. Callers must already own legal guest body use;
 * this is lifetime validation, not a guest capability/security boundary. */
/* Recognize a still-accounted device's exact embedded lock address. Does not
 * borrow storage; callers must pin before touching its bytes. */
int tsfp_device_factory_has_event(tsfp_device_factory *,uint32_t address);
int tsfp_device_factory_pin(tsfp_device_factory *,uint32_t body,tsfp_guest_object_lease *,void **mapping,uint32_t *bytes);
/* Idempotent creator deletion. Lookup/worker leases and guest references survive
 * name removal. This does not cancel/join queued work or call guest drivers. */
int tsfp_device_factory_delete(tsfp_device_factory *,uint32_t body);
/* ONLY invoke as the configured object-owner body disposal callback. */
int tsfp_device_factory_dispose(tsfp_device_factory *,uint32_t body,uint32_t kind,uint32_t flags);
/* Close admission, retire live creators; in-flight creations roll back. Caller
 * coordinates shutdown before consuming results, joins callers/workers and
 * completes pending work before releasing storage. Not global kernel shutdown. */
void tsfp_device_factory_stop(tsfp_device_factory *);
int tsfp_device_factory_retry_one(tsfp_device_factory *);
/* Final reference disposal queues a ticket when defer_finalization is set.
 * take changes WAITING -> ACTIVE; copied tickets contain only stable values.
 * Caller runs driver callbacks outside all provider locks. finish(completed=1)
 * permits a later object-owner collect to release storage. completed=0 means
 * callback disposition is uncertain: quarantine, never silently retry it.
 * Exactly one finish succeeds. Context/index/owner remain alive throughout.
 * Guest nonlocal hooks must finish an active ticket explicitly. */
/* Active callback tickets may pin storage after ordinary object refs drain.
 * Zero, non-copyable pin transfers only on success; release once. A completed
 * callback cannot create new pins, and READY cleanup waits for existing pins.
 * Uncertain callbacks remain quarantined even after their pins drain. */
int tsfp_device_factory_pin_finalization(tsfp_device_factory *,const tsfp_device_finalization *,tsfp_device_final_pin *,void **,uint32_t *);
int tsfp_device_factory_release_finalization(tsfp_device_final_pin *);
size_t tsfp_device_factory_final_pins(tsfp_device_factory *,const tsfp_device_finalization *);
int tsfp_device_factory_take_finalization(tsfp_device_factory *,tsfp_device_finalization *);
int tsfp_device_factory_finish_finalization(tsfp_device_factory *,const tsfp_device_finalization *,int completed);
void tsfp_device_factory_counts(tsfp_device_factory *,size_t *live,size_t *retained,size_t *uncertain);
#endif
