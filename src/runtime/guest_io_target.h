/* Original read-only selection of an Xbox driver dispatch target. */
#ifndef TSFP_GUEST_IO_TARGET_H
#define TSFP_GUEST_IO_TARGET_H
#include "guest_io_request.h"
typedef struct {uint32_t device,request,stack,routine;unsigned major;} tsfp_io_target;
/* Supplied mappings are stable and fully accessible, and output is disjoint.
 * Caller owns/pins device and packet and serializes mutation of request/table.
 * Guest words alone are not ownership or security authority. This only selects
 * a value target: no slot consumption, host locks, guest calls or fabricated
 * driver implementation. Call adapter must allowlist lookup before committing
 * the slot, and retain ownership/scope cleanup across the guest invocation. */
int tsfp_io_target_prepare(const void *packet,size_t packet_span,uint32_t request,
    const void *device,size_t device_span,uint32_t body,
    const void *driver,size_t driver_span,uint32_t driver_address,tsfp_io_target *);
#endif
