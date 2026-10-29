/* Original host-owned FIFO request scheduling; no guest links are trusted. */
#ifndef TSFP_GUEST_IO_QUEUE_H
#define TSFP_GUEST_IO_QUEUE_H
#include "guest_io_owner.h"
typedef struct tsfp_io_queue {
    tsfp_io_owner **entries;
    size_t capacity,head,count;
    tsfp_io_owner *current;
    unsigned closed;
    void *context;void (*lock)(void *);void (*unlock)(void *);
    /* Optional trusted metadata publisher, installed only while quiescent and
     * empty. Called under queue lock after transitions, before old refs drop.
     * Writes only still-owned waiting entries and device state; never touches
     * a removed/current entry, which may already wait at another device. Must
     * not fail, reenter locks, execute guest code or change ownership. All mapped
     * output spans/context must be prevalidated, stable and externally pinned.
     */
    void *publish_context;
    void (*publish)(void *,const struct tsfp_io_queue *);
} tsfp_io_queue;
/* Fresh, disjoint queue/entry storage must outlive every call. Caller joins all
 * users before freeing it. Locks are distinct from owner locks; ordering is
 * queue -> owner. Release/disposal runs outside the queue lock, returns normally
 * and may reenter this queue. No guest callback/nonlocal transfer is allowed.
 * Storage and fields are private host authority, never guest-writable mirrors.
 */
int tsfp_io_queue_init(tsfp_io_queue *,tsfp_io_owner **,size_t,void *,void (*)(void *),void (*)(void *));
/* Borrow a caller-owned client ref; success takes an additional queue ref.
 * Reject duplicate owners, closed/full queues and READY/nonlive operations.
 * Completion can race admission: queued/current ownership remains valid even
 * when an operation completes; the caller must serialize cancellation policy.
 * Capacity bounds waiting entries only; current has its own queue reference.
 */
int tsfp_io_queue_push(tsfp_io_queue *,tsfp_io_owner *);
/* Atomically start an idle empty queue or append behind current. On success,
 * selected is an extra caller-owned ref when immediately current, else NULL.
 * Borrow caller ref; failure preserves queue/output. Do not mix idle waiting
 * work from push with submit: select it with next(NULL) first. */
int tsfp_io_queue_submit(tsfp_io_queue *,tsfp_io_owner *,tsfp_io_owner **selected);
/* Compare current identity, retire it and select the oldest waiting owner in
 * one locked transition. expected=NULL starts an idle queue. Caller holds a
 * valid client ref to nonnull expected, preventing identity reuse. On success
 * *selected is an additional owned client ref, or NULL when no work remains.
 * Output is disjoint host storage, never an alias of queue/entries/owner fields.
 * Failure preserves queue and output. Closed queues retire current but select
 * no waiting work. Selected owners may already be COMPLETE: no guest entry is
 * authorized here. StartIo needs its own callback/operation scope, device lease,
 * allowlisted routine and guest wire-state adapter before calling guest code.
 */
int tsfp_io_queue_next(tsfp_io_queue *,tsfp_io_owner *expected,tsfp_io_owner **selected);
/* Close admission and detach waiting refs outside the lock. Does not complete
 * or cancel operations, and retains current until explicit next(expected).
 * Idempotent; concurrent callers may help drain. Whole shutdown requires user
 * quiescence and independently accounting for pending/uncertain operations.
 */
void tsfp_io_queue_stop(tsfp_io_queue *);
#endif
