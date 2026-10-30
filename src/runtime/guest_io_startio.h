/* Original StartIo callback composition for a selected, already active request. */
#ifndef TSFP_GUEST_IO_STARTIO_H
#define TSFP_GUEST_IO_STARTIO_H
#include "guest_io_call.h"
/* Consumes exactly one caller-owned request client ref on every nonnull path.
 * Caller supplies matching request-owned device leases and serialized stable
 * mappings/driver table. Selects DRIVER_OBJECT.DriverStartIo at offset0, permits
 * only allowlisted translated/manual entries and calls stdcall(Device,Request).
 * READY/COMPLETE/UNCERTAIN requests cannot start: this forwards an existing
 * ACTIVE/PENDING operation with a separate callback pin, without advancing or
 * modifying any request/slot bytes. Normal return ignores guest EAX (void ABI).
 * Native exits require lifetime hooks to unwind before discarding C frames;
 * malformed/abandoned entry retains pending ownership rather than completion.
 *
 * Queue/device adapters must own and publish CurrentIrp/Busy/list state before
 * this call, serialize selection/cancellation and independently retain queued
 * work. This composer does not select the next request, hold locks over guest
 * execution, establish guest wire mirrors or implement kernel StartPacket/Next.
 * All helpers preserve guest architectural state and return normally. Ops and
 * external contexts outlive callbacks, including nested/unwound dispatch.
 * Result uses TSFP_IO_CALL_NOT_ENTERED/RETURNED/UNCERTAIN; NOT_ENTERED may retain
 * an already pending operation and never authorizes dropping its operation pin.
 */
int tsfp_io_startio(tsfp_guest_object_scopes *,tsfp_io_owner *,tsfp_io_devices *,
    uint32_t device,const tsfp_io_call_ops *);
#endif
