#ifndef TSFP_GUEST_IO_COMPLETE_H
#define TSFP_GUEST_IO_COMPLETE_H
#include "guest_io_scope.h"
typedef struct {
    uint32_t address;
    uint32_t status,information,flags,user_iosb,user_event,thread,original_file;
    uint32_t overlay_low,overlay_high;
    uint32_t user_buffer,segment_array,locked_buffer_length;
    unsigned char pending_returned,cancel;
} tsfp_io_result;
typedef struct {void *cookie;void *iosb;size_t iosb_bytes;} tsfp_io_delivery;
enum {TSFP_IO_DELIVERY_PRIVATE=0,TSFP_IO_DELIVERY_DONE=1,TSFP_IO_DELIVERY_UNCERTAIN=2};
enum {TSFP_IO_COMPLETE_UNCERTAIN=-1,TSFP_IO_COMPLETE_REJECTED=0,TSFP_IO_COMPLETE_DONE=1};
typedef struct {
    void *context;
    /* Normal-return/no-guest preflight. On success pins every notification
     * target and full writable IOSB span until release; on failure owns cleanup
     * and transfers nothing. Guest words alone establish no host capability. */
    int (*prepare)(void *,const tsfp_io_result *,tsfp_io_delivery *);
    /* Called once after IOSB writes, outside owner locks. Implements required
     * release ordering and actual event/thread/APC delivery. Accepted async
     * delivery retains its own references. May perform guest callbacks only
     * with lifetime hooks that unwind scopes before native stack abandonment. */
    int (*notify)(void *,void *,const tsfp_io_result *);
    /* Normal-return/no-guest, once after successful prepare, outside locks.
     * PRIVATE was never committed; DONE finished; UNCERTAIN must account or
     * quarantine persistent references, never blind retry. Return0 reports
     * cleanup uncertainty, with ownership retained/accounted by the backend.
     * May retire a constructor-owned registry ref only on DONE. Must not free
     * the owner/control block or invalidate this call's held client ref. */
    int (*release)(void *,void *,int disposition);
} tsfp_io_complete_ops;
/* Bounded supported IRP prefix decoder. No ownership validation or mutations.
 * address remains0; the coordinator supplies its owned host identity separately.
 * All remaining guest values are data requiring backend validation/pinning. */
int tsfp_io_result_decode(const void *,size_t,tsfp_io_result *);
/* Consumes exactly one caller-held owner client ref on every nonnull path.
 * Caller serializes guest packet mutation and retains stable ops/store/context.
 * Does not implement a kernel import, actual event/APC scheduling or queues;
 * the backend must provide the advertised lifetime/delivery semantics. */
int tsfp_io_complete_request(tsfp_guest_object_scopes *,tsfp_guest_objects *,
    tsfp_io_owner *,const tsfp_io_complete_ops *);
#endif
