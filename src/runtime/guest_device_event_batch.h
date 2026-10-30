/* Original bounded parent pins for multi-object guest event operations. */
#ifndef TSFP_GUEST_DEVICE_EVENT_BATCH_H
#define TSFP_GUEST_DEVICE_EVENT_BATCH_H
#include "guest_device_event_scope.h"
enum {TSFP_DEVICE_EVENT_LIMIT=64};
typedef struct {
    tsfp_guest_object_scope scope;
    tsfp_device_event events[TSFP_DEVICE_EVENT_LIMIT];
    size_t count;
} tsfp_device_event_batch;
/* Validate the full counted guest pointer array before reading/pinning. Count
 * must be1..64; unknown non-device entries remain the caller's other provider
 * responsibility. Recognized device events must pin or fail. One cleanup
 * scope releases all acquired pins on normal return, partial failure or native
 * guest unwind. Array, headers and mappings remain stable through operation.
 * Returns guest status0 on success. Uses no64-frame stack chain or host heap. */
uint32_t tsfp_device_event_batch_begin(tsfp_guest_object_scopes *,tsfp_device_event_batch *,tsfp_device_factory *,uint32_t array,uint32_t count);
int tsfp_device_event_batch_end(tsfp_guest_object_scopes *,tsfp_device_event_batch *);
#endif
