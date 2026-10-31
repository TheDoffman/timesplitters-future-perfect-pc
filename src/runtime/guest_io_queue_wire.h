/* Original Xbox queue mirror publisher backed exclusively by host ownership. */
#ifndef TSFP_GUEST_IO_QUEUE_WIRE_H
#define TSFP_GUEST_IO_QUEUE_WIRE_H
#include "guest_io_queue.h"
typedef struct {unsigned char *device;uint32_t address;} tsfp_io_queue_wire;
/* Quiescent empty queue only; failure preserves queue, wire and guest bytes.
 * Caller owns/pins stable device/request mappings, serializes guest access and
 * keeps wire/queue/context live until all users join. All output spans must be
 * disjoint from host metadata and each other. Binding does not acquire a lease.
 * Installs a non-failing metadata publisher under the queue lock. It writes
 * CurrentIrp, Busy and list links/Inserted from retained host identities, never
 * follows guest links and never executes guest code or acquires another lock.
 * Queue entry is IRP+3c, Inserted+48; preserves SortKey, fifth DriverContext word,
 * thread, payload and every unrelated device byte. Removal clears Inserted but
 * leaves old link words unspecified; do not infer ownership from their values.
 * FIFO only: keyed scheduling/cancellation, device generation/lease retention,
 * shutdown coordination and kernel/StartIo routing remain adapter duties.
 */
int tsfp_io_queue_wire_init(tsfp_io_queue_wire *,tsfp_io_queue *,void *device,
    size_t span,uint32_t address);
#endif
