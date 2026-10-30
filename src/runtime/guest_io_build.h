#ifndef TSFP_GUEST_IO_BUILD_H
#define TSFP_GUEST_IO_BUILD_H
#include "guest_io_transfer.h"
#include "guest_io_request.h"
typedef struct {
    tsfp_io_transfer transfer;
    uint32_t flags,user_buffer,user_iosb,user_event,thread;
} tsfp_io_build_plan;
/* Original byte construction for a fresh, privately owned read/write packet.
 * Caller owns stable target storage and the lifetimes/permissions of referenced
 * guest buffers, device, status block, event and thread. This only encodes their
 * values, not ownership or synchronization. Invalid arguments preserve target.
 * Parameters/source and output storage are stable/disjoint; raw bytes may be
 * unaligned. No allocation, registration, callback, queue or notification occurs.
 * READY owner serialization is required when constructing in an existing owner. */
int tsfp_io_build_packet(void *,size_t,uint32_t address,uint32_t bytes,uint32_t stacks,
                         const tsfp_io_build_plan *);
#endif
