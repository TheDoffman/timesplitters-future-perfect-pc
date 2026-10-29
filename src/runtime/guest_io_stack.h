#ifndef TSFP_GUEST_IO_STACK_H
#define TSFP_GUEST_IO_STACK_H
#include <stdint.h>
#include <stddef.h>
enum {TSFP_IO_STACK_BYTES=24};
/* Read/write view of the supported guest stack bytes. Host structure layout
 * is never serialized. Key interpretation/operation validation is separate. */
typedef struct {
    unsigned char major,minor,flags,control;
    uint32_t length,key;
    uint64_t offset;
    uint32_t device;
} tsfp_io_stack;
/* Entire supplied spans/host records remain valid and stable for the call.
 * Raw spans may be unaligned. Invalid arguments preserve the destination. */
int tsfp_io_stack_decode(const void *,size_t,tsfp_io_stack *);
int tsfp_io_stack_encode(void *,size_t,const tsfp_io_stack *);
#endif
