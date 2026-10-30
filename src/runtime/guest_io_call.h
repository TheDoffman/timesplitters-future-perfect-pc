/* Original initial/forwarded-driver call composition; not a kernel thunk. */
#ifndef TSFP_GUEST_IO_CALL_H
#define TSFP_GUEST_IO_CALL_H
#include "guest_io_devices.h"
#include "guest_io_scope.h"
#include "guest_io_target.h"
#include "guest_callback_call.h"
enum {TSFP_IO_CALL_UNCERTAIN=-1,TSFP_IO_CALL_NOT_ENTERED=0,TSFP_IO_CALL_RETURNED=1};
typedef struct {
    tsfp_guest_callback_ops callback;
    void *context;const void *(*input)(void *,uint32_t,uint32_t);
} tsfp_io_call_ops;
/* Consumes exactly one caller-owned client ref on every nonnull-owner path,
 * including pre-entry failure and native unwind. Caller supplies the matching
 * request-owned device binding, stable guest mappings/driver table and external
 * request serialization. Output status is disjoint stable host storage, written
 * only on RETURNED. Helpers return normally without guest execution/state edits.
 * Lifetime hooks must unwind these scopes before discarding the C stack.
 * Pending work retains request/device ownership; no fabricated completion.
 * Dispatch consumes only the four-byte operation prefix and preserves every
 * stack-slot payload byte. Device identity is passed as an argument and retained
 * in host-owned leases; no unestablished guest DeviceObject field is written.
 * This dispatch composer does not implement queues,
 * cancellation, completion/APCs, shutdown or a usable filesystem/disc backend.
 */
int tsfp_io_call_driver(tsfp_guest_object_scopes *,tsfp_io_owner *,tsfp_io_devices *,
    uint32_t device,const tsfp_io_call_ops *,uint32_t *status);
#endif
