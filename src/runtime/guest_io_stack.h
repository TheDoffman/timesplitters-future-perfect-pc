#ifndef TSFP_GUEST_IO_STACK_H
#define TSFP_GUEST_IO_STACK_H
#include <stdint.h>
#include <stddef.h>
enum {TSFP_IO_STACK_BYTES=24};
/* Only the four-byte dispatch prefix is interpreted. The rest includes guest
 * operation-specific storage and stays opaque in this generic view. The
 * separate supported-US transfer view interprets its established device/buffer
 * fields; guest values never establish host object ownership.
 * Host structure layout is never serialized. */
typedef struct {
    unsigned char major,minor,flags,control;
    unsigned char opaque[20];
} tsfp_io_stack;
/* Entire supplied spans/host records remain valid and stable for the call.
 * Raw spans may be unaligned. Invalid arguments preserve the destination. */
int tsfp_io_stack_decode(const void *,size_t,tsfp_io_stack *);
int tsfp_io_stack_encode(void *,size_t,const tsfp_io_stack *);
#endif
