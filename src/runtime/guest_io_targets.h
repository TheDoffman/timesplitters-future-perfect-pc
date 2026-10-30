/* Original ownership and publication of request completion target spans. */
#ifndef TSFP_GUEST_IO_TARGETS_H
#define TSFP_GUEST_IO_TARGETS_H
#include "guest_io_complete.h"
#include "guest_event.h"
#include "guest_object_services.h"
typedef struct {
    uint64_t key;uint32_t base,bytes,address,kind;
} tsfp_io_target_region;
typedef struct {
    void *context;void (*lock)(void *);void (*unlock)(void *);
    void *(*output)(void *,uint32_t address,uint32_t bytes);
} tsfp_io_targets_ops;
enum {TSFP_IO_TARGET_EMPTY,TSFP_IO_TARGET_BOUND,TSFP_IO_TARGET_PREPARING,
    TSFP_IO_TARGET_PREPARED,TSFP_IO_TARGET_SIGNALING,TSFP_IO_TARGET_SIGNALED,
    TSFP_IO_TARGET_RELEASING,TSFP_IO_TARGET_DELIVERED,TSFP_IO_TARGET_CLOSING,
    TSFP_IO_TARGET_QUARANTINED,TSFP_IO_TARGET_CLOSED};
typedef struct {
    tsfp_guest_objects *objects;tsfp_io_targets_ops ops;
    tsfp_io_target_region region[2];tsfp_guest_object_lease lease[2];
    unsigned held[2],state;uint32_t request;void *iosb;tsfp_event_view event;
} tsfp_io_targets;
/* Fresh/quiescent, disjoint caller-owned persistent metadata. Region key is an
 * already-owned object identity, NOT a guest-supplied key. Its dispatcher must
 * own the declared base/span until references/leases drain; caller guarantees
 * this relationship, mapping permissions and no host-metadata alias. All-zero
 * region means absent. Init captures independent object refs, so original guest
 * handles/refs may close afterwards. No guest storage is adopted/freed here.
 * Validation failure transfers nothing. Acquisition rollback leaves CLOSED or
 * QUARANTINED state; a quarantined context must remain accounted, never freed.
 * Lock differs from owner/object locks; output is normal-return/no-guest.
 */
int tsfp_io_targets_init(tsfp_io_targets *,tsfp_guest_objects *,uint32_t request,
    const tsfp_io_target_region *iosb,const tsfp_io_target_region *event,const tsfp_io_targets_ops *);
/* Preflight before completion admission: match exact captured addresses, pin
 * their object generation/kind/base, resolve full writable8/16-byte spans,
 * reject host/guest overlap and validate event header/alignment/state. Failure
 * preserves delivery and target bytes. No lock spans mapping/object callbacks.
 * Persistent delivery leases survive native unwind. Only one preparation wins.
 */
int tsfp_io_targets_prepare(tsfp_io_targets *,const tsfp_io_result *,tsfp_io_delivery *);
/* Signal guest event with release ordering after IOSB publication, then invoke
 * actual wake scheduling outside locks. host is supplied only for a native
 * event owner at its base; embedded/raw events pass0. Wake may enter guest code
 * only with completion lifetime hooks armed. Failure/nonlocal exit requires
 * finish(UNCERTAIN), retaining pins instead of retrying. No event returns0;
 * caller must select a different validated completion route before admission.
 */
int tsfp_io_targets_signal(tsfp_io_targets *,
    int (*wake)(void *,uint32_t address,void *signal,uintptr_t host),void *context);
/* PRIVATE is allowed only before signal; DONE only after acknowledged wake.
 * UNCERTAIN retains all delivery leases/object refs in QUARANTINED state.
 * Failed lease release also quarantines; never retry automatically. Registry
 * retirement, request completion, APC/thread/file/buffer policy belong to the
 * completion backend, not this target-span owner. */
int tsfp_io_targets_finish(tsfp_io_targets *,int disposition);
/* Release captured refs only when BOUND/DELIVERED. Active/quarantined contexts
 * cannot close. Idempotent CLOSED. Callback/context/storage must outlive every
 * active or quarantined resource; metadata callbacks return normally. */
int tsfp_io_targets_close(tsfp_io_targets *);
unsigned tsfp_io_targets_state(tsfp_io_targets *);
#endif
