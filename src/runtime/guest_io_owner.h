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
int tsfp_io_owner_retain(tsfp_io_owner *);
int tsfp_io_owner_release(tsfp_io_owner *);
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
