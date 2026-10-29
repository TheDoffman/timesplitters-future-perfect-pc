#ifndef TSFP_GUEST_IO_POOL_H
#define TSFP_GUEST_IO_POOL_H
#include "guest_io_registry.h"
typedef struct tsfp_io_pool_node tsfp_io_pool_node;
typedef struct {
    void *context;
    void (*lock)(void *),(*unlock)(void *);
    void (*registry_lock)(void *),(*registry_unlock)(void *);
    void *(*metadata_allocate)(void *,size_t);
    void (*metadata_free)(void *,void *);
    uint32_t (*allocate)(void *,uint32_t);
    void *(*resolve)(void *,uint32_t,uint32_t);
    /* 1 consumes allocation, 0 retains ownership and permits explicit retry;
     * -1 means uncertain ownership, recorded but never automatically retried. */
    int (*deallocate)(void *,uint32_t);
} tsfp_io_pool_ops;
typedef struct {
    tsfp_io_registry registry;tsfp_io_pool_ops ops;
    size_t live;int closed;tsfp_io_pool_node *failed;
} tsfp_io_pool;
/* Original Xbox request type fact, not an implementation dependency. */
enum {TSFP_IO_REQUEST_KIND=6};
/* Fresh/quiescent init; pool, entries and lock contexts outlive all calls,
 * acquired refs, pending operations and retained cleanup records. Locks differ.
 * Configured host metadata allocation must provide suitable C alignment.
 * Guest allocator provides a unique nonzero base and writable complete span;
 * it must not reuse addresses until successful deallocation. */
int tsfp_io_pool_init(tsfp_io_pool *,tsfp_io_registry_entry *,size_t,const tsfp_io_pool_ops *);
/* Registry owns allocated packets until remove/stop. Returns zero on failure.
 * Capacity includes in-flight allocations and failed cleanup, bounding growth. */
uint32_t tsfp_io_pool_allocate(tsfp_io_pool *,uint32_t stacks);
/* Optional trusted initializer runs before registry publication, without locks.
 * It returns normally, executes no guest code, preserves owner/control refs and
 * READY phase, and may construct guest bytes or transfer one owned attachment.
 * On failure the pool disposes that owner/attachment; no request is published.
 * Success still rolls back if concurrent stop prevents publication. */
uint32_t tsfp_io_pool_construct(tsfp_io_pool *,uint32_t stacks,
    int (*prepare)(void *,tsfp_io_owner *),void *context);
/* Stop closes admission and detaches registry entries. In-flight allocations
 * roll back; borrowed and pending requests retain their storage. Caller joins
 * API callers/workers and completes/cancels requests before destroying pool.
 * A concurrently stopped pool does not promise returned guest addresses remain
 * registered: callers must coordinate shutdown before using guest results. */
void tsfp_io_pool_stop(tsfp_io_pool *);
/* Explicitly attempt ONE known-retained cleanup; uncertain frees are excluded.
 * Returns1 only if an allocation was consumed,0 otherwise. Never loops/retries. */
int tsfp_io_pool_retry_one(tsfp_io_pool *);
void tsfp_io_pool_counts(tsfp_io_pool *,size_t *live,size_t *retained,size_t *uncertain);
#endif
