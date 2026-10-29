/* Original ownership for a device's embedded synchronization event. */
#ifndef TSFP_GUEST_DEVICE_EVENT_H
#define TSFP_GUEST_DEVICE_EVENT_H
#include "guest_device_factory.h"
typedef struct {
    tsfp_guest_object_lease lease;
    tsfp_device_final_pin final;
    unsigned char *bytes;
    uint32_t address;
} tsfp_device_event;
/* Zero-initialized, non-copyable pin; release once. Recognizes only the exact
 * DeviceLock at body+0x34, validates its complete auto-event header and aligned
 * signal word, then pins the parent allocation. No standalone embedded free.
 * Header/mapping remains stable while pinned. Guest signal updates use atomic
 * host operations, not a second host event that could diverge from guest bytes.
 * These are nonblocking primitives; wait scheduling/timeouts/APCs are separate. */
int tsfp_device_event_pin(tsfp_device_factory *,uint32_t address,tsfp_device_event *);
/* Exact active driver ticket supplies an independent finalization pin. It can
 * outlive ordinary callback completion; releasing it permits later collection. */
int tsfp_device_event_pin_finalization(tsfp_device_factory *,const tsfp_device_finalization *,uint32_t address,tsfp_device_event *);
int tsfp_device_event_release(tsfp_device_event *);
/* 1 success / 0 no signal for try_wait; set/reset return previous0/1 state.
 * -1 invalid/unpinned state. No signal is consumed by publication or pinning. */
int tsfp_device_event_try_wait(tsfp_device_event *);
int tsfp_device_event_set(tsfp_device_event *);
int tsfp_device_event_reset(tsfp_device_event *);
#endif
