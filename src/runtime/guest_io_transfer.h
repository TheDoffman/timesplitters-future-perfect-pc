#ifndef TSFP_GUEST_IO_TRANSFER_H
#define TSFP_GUEST_IO_TRANSFER_H
#include "guest_io_stack.h"
/* Original supported-US read/write slot view, established from title accesses.
 * This is not the generic slot schema and does not establish device ownership,
 * buffer access permissions, a backend, or the rest of the Xbox request ABI. */
typedef struct {
    unsigned char major,minor,flags,control;
    uint32_t length,buffer_word,device;
    uint64_t byte_offset;
} tsfp_io_transfer;
/* Major2(read)/3(write) only. Stable disjoint host outputs, full valid spans;
 * unaligned bytes are permitted. Failure changes no output/destination. */
int tsfp_io_transfer_decode(const void *,size_t,tsfp_io_transfer *);
int tsfp_io_transfer_encode(void *,size_t,const tsfp_io_transfer *);
/* Flags bit80 selects an absolute buffer word; otherwise add IRP.UserBuffer.
 * Reject address/range wrap and nonempty null buffers; empty null is a value.
 * This computes an address without mapping it or mutating the request. */
int tsfp_io_transfer_address(const tsfp_io_transfer *,uint32_t user_buffer,uint32_t *);
#endif
