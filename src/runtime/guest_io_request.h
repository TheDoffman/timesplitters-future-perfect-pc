#ifndef TSFP_GUEST_IO_REQUEST_H
#define TSFP_GUEST_IO_REQUEST_H
#include "guest_io_stack.h"
enum {TSFP_IO_REQUEST_BYTES=0x68,TSFP_IO_MAX_STACKS=127};
/* Candidate supported-layout codecs, not ownership validation or kernel APIs.
 * Type tag is caller-supplied pending complete object-type ABI acceptance.
 * Target span may be unaligned; invalid arguments leave all outputs unchanged.
 * Packet size can include extra caller-owned storage after stack slots. */
int tsfp_io_request_initialize(void *,size_t span,uint32_t guest_address,
    uint32_t packet_bytes,uint32_t stacks,uint16_t type);
/* Validate stored size/count/location/pointer before consuming one stack slot.
 * Host output pointer must not overlap the raw request bytes. Caller ensures
 * stable memory, synchronization, genuine ownership and valid dispatch policy. */
int tsfp_io_request_advance(void *,size_t span,uint32_t guest_address,uint32_t *stack_address);
#endif
