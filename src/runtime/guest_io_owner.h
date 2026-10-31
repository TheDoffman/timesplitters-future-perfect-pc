#ifndef TSFP_GUEST_IO_OWNER_H
#define TSFP_GUEST_IO_OWNER_H
#include "guest_io_request.h"
typedef enum {TSFP_IO_READY,TSFP_IO_ACTIVE,TSFP_IO_PENDING,
    TSFP_IO_COMPLETING,TSFP_IO_COMPLETE,TSFP_IO_DEAD,TSFP_IO_UNCERTAIN} tsfp_io_phase;
typedef struct tsfp_io_owner tsfp_io_owner;
typedef void (*tsfp_io_action)(void *,tsfp_io_owner *);
typedef struct {
    void *context;
    void (*lock)(void *);
    void (*unlock)(void *);
    void (*dispose)(void *,uint32_t,void *,size_t);
} tsfp_io_owner_ops;
struct tsfp_io_owner {
    tsfp_io_owner_ops ops;
    unsigned char *bytes;
    size_t span;
    uint32_t address,clients,internal;
    tsfp_io_phase phase;
    void *attachment;void (*release_attachment)(void *);
    unsigned dispatch_abandoned;
    void *waiting_queue;
};
/* Caller owns a fresh, quiescent control block and stable lock context. The
 * transferred mapping remains valid until dispose, which runs outside the lock.
 * Lock context remains valid through disposal. Disposal may reclaim the owner
 * control block: no API accesses it after invoking disposal. Every API caller
 * needs a valid client/internal reference or otherwise stable control storage;
 * never access a raw owner after releasing its last reference. Calls made only
 * for debugging after release require separately retained control storage.
 * Guest bytes never establish host ownership. No address registry is supplied.
 * A successful init transfers mapping ownership and creates one client ref;
 * failure changes neither the control block nor the caller's mapping. */
int tsfp_io_owner_init(tsfp_io_owner *,const tsfp_io_owner_ops *,void *,size_t,
    uint32_t address,uint32_t bytes,uint32_t stacks,uint16_t type);
/* Attach fresh READY host ownership to an already initialized, caller-owned
 * packet without modifying it. Validates exact declared bytes/type/layout.
 * Caller guarantees a new quiescent packet and stable mapping until disposal;
 * guest fields cannot establish that guarantee. dispose releases host metadata
 * and borrowed mapping pins ONLY, never this caller-owned packet storage.
 * Rejection preserves both control block and packet. No operation is inferred
 * from an advanced guest stack location or status/pending field. */
int tsfp_io_owner_adopt(tsfp_io_owner *,const tsfp_io_owner_ops *,void *,size_t,
    uint32_t address,uint32_t bytes,uint16_t type);
int tsfp_io_owner_retain(tsfp_io_owner *);
int tsfp_io_owner_release(tsfp_io_owner *);
/* Queue -> owner lock order. Claim adds one queue client and exclusively owns
 * the packet's embedded waiting-list entry. Start adds two clients (current and
 * returned caller) without claiming a list entry. Both require ACTIVE/PENDING,
 * no waiting membership and bounded ref counts. The caller already holds a
 * valid client ref. Take clears exactly the matching waiting identity without
 * dropping its queue client. It clears Inserted before releasing membership;
 * old queues must not touch the detached entry afterward. Queue code releases
 * that ref outside queue locks. Start clears Inserted under the same owner lock.
 * A request may be current at an upper device while waiting at a lower device;
 * it can never be in two waiting lists. Tags are persistent host identities.
 */
int tsfp_io_owner_queue_claim(tsfp_io_owner *,void *queue);
int tsfp_io_owner_queue_take(tsfp_io_owner *,void *queue);
int tsfp_io_owner_queue_start(tsfp_io_owner *);
typedef struct {tsfp_io_owner *owner;unsigned kind;} tsfp_io_ticket;
/* Zero, non-copyable host ticket; end once. Dispatch owns a separate callback
 * pin and operation pin. Abandoning dispatch drops only the callback pin and
 * leaves a pending operation that may still legitimately complete. Abandoning
 * completion retains an UNCERTAIN operation; never invent successful completion
 * or free storage whose callback disposition is unknown. Scope storage/context
 * stays live; every caller already owns valid owner control lifetime. */
int tsfp_io_owner_dispatch_begin(tsfp_io_owner *,tsfp_io_ticket *);
/* Forward an existing ACTIVE/PENDING operation: exactly one new callback pin,
 * no second operation pin. Private rollback drops only this callback and leaves
 * the parent phase untouched, including completion concurrent with admission. */
int tsfp_io_owner_forward_begin(tsfp_io_owner *,tsfp_io_ticket *);
/* Publish immediately before guest entry. A published ticket cannot roll back.
 * Initial private rollback restores READY and consumes both initial pins.
 * Forward rollback consumes only its callback pin and preserves the parent phase.
 * Either rollback may dispose an otherwise unreferenced packet. Caller must not access the owner after successful rollback without a
 * separate retained client. Failed rollback preserves ticket/state. */
int tsfp_io_owner_dispatch_publish(tsfp_io_ticket *);
int tsfp_io_owner_dispatch_rollback(tsfp_io_ticket *);
int tsfp_io_owner_dispatch_end(tsfp_io_ticket *,int abandoned);
int tsfp_io_owner_complete_begin(tsfp_io_owner *,tsfp_io_ticket *,uint32_t status,uint32_t information);
int tsfp_io_owner_complete_end(tsfp_io_ticket *,int abandoned);
/* Dispatch and completion callbacks run outside the lock and may reenter.
 * Dispatch gets its own pin as well as a pending-operation pin. The callback
 * must eventually arrange complete; pending operations deliberately retain the
 * packet even if every client releases it. Cancellation must complete too.
 * Callbacks must return normally; nonlocal guest exits need scope integration. */
int tsfp_io_owner_dispatch(tsfp_io_owner *,tsfp_io_action,void *);
int tsfp_io_owner_pending(tsfp_io_owner *);
int tsfp_io_owner_complete(tsfp_io_owner *,uint32_t status,uint32_t information,
    tsfp_io_action,void *);
tsfp_io_phase tsfp_io_owner_phase(tsfp_io_owner *);
/* Transfer one host-owned attachment while READY. Callback runs once outside
 * the lock after final client/internal refs drain and before mapping disposal.
 * It must return normally, execute no guest code and must not free the owner.
 * Failure retains caller ownership. No attachment pointer is borrowed by guest
 * code; aggregate multiple device leases in separately owned host metadata. */
int tsfp_io_owner_attach(tsfp_io_owner *,void *,void (*)(void *));
#endif
