/* Original recursive critical-section registry; all records are host metadata. */
#ifndef TSFP_GUEST_CRITICAL_H
#define TSFP_GUEST_CRITICAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct tsfp_critical_record {
    struct tsfp_critical_record *next;
    uintptr_t key,owner;
    uint32_t depth,site;
    void *native;
} tsfp_critical_record;
typedef struct tsfp_critical_ops {
    void *context;
    void (*lock)(void *),(*unlock)(void *);
    /* Fresh record with live recursive native lock. Called under metadata guard;
     * must not call guest code, reenter this registry or take a guest lock. */
    tsfp_critical_record *(*create)(void *);
    /* Successful disposal consumes the record. Failure retains it, quarantined
     * without automatic retry. Only called with no owners or API users. */
    int (*dispose)(void *,tsfp_critical_record *);
    /* 1 acquired, 0 busy (try only), -1 host failure. Never holds metadata guard.
     * release is infallible for a valid owner and must not call guest code. */
    int (*acquire)(void *,tsfp_critical_record *,int try_only);
    void (*release)(void *,tsfp_critical_record *);
    uintptr_t (*thread)(void *);
} tsfp_critical_ops;
typedef struct tsfp_critical_registry {
    tsfp_critical_ops ops;
    tsfp_critical_record **buckets,*quarantine;
    size_t bucket_count,live,held,quarantined;
    uint32_t active;
    uint64_t enters,leaves,busy;
    int configured,ready,single;
} tsfp_critical_registry;
typedef struct tsfp_critical_view {
    size_t live,held,quarantined;
    uint32_t active,depth,site;
    uintptr_t owner;
    uint64_t enters,leaves,busy;
    int ready,found;
} tsfp_critical_view;
enum tsfp_critical_result {TSFP_CRITICAL_OK,TSFP_CRITICAL_BUSY,TSFP_CRITICAL_INVALID,
    TSFP_CRITICAL_NO_MEMORY,TSFP_CRITICAL_NOT_OWNER,TSFP_CRITICAL_CLOSED,
    TSFP_CRITICAL_LIMIT,TSFP_CRITICAL_HOST_FAILURE};
/* Zero registry first. Init/reinit and final guard teardown require quiescence.
 * Buckets/ops/records are noncopyable and stay live until successful stop.
 * No delete/reinitialize-in-place of an owned lock; initialize is idempotent.
 * Keys are opaque nonzero identities (the bridge uses mapped host identity,
 * preserving aliases). Single mode deliberately collapses all nonzero keys.
 * These operations do not read/write any guest critical-section bytes. */
int tsfp_critical_init(tsfp_critical_registry *,tsfp_critical_record **,size_t,const tsfp_critical_ops *,int single);
enum tsfp_critical_result tsfp_critical_initialize(tsfp_critical_registry *,uintptr_t);
enum tsfp_critical_result tsfp_critical_enter(tsfp_critical_registry *,uintptr_t,uint32_t site,int try_only);
enum tsfp_critical_result tsfp_critical_leave(tsfp_critical_registry *,uintptr_t);
int tsfp_critical_snapshot(tsfp_critical_registry *,uintptr_t,tsfp_critical_view *);
/* Refuses while owned or any acquisition/release/creator is in flight. Closing
 * admission is serialized with lookup; waiters on the metadata guard see closed.
 * Records dispose outside the guard. Join users before destroying that guard;
 * no concurrent reinit/stop, abandoned-owner recovery or guest callback unwind. */
int tsfp_critical_stop(tsfp_critical_registry *);
#endif
