/* Original scoped ownership for embedded-device event operations. */
#ifndef TSFP_GUEST_DEVICE_EVENT_SCOPE_H
#define TSFP_GUEST_DEVICE_EVENT_SCOPE_H
#include "guest_device_event.h"
#include "guest_device_callback.h"
typedef struct {tsfp_guest_object_scope scope;tsfp_device_event event;unsigned active;} tsfp_device_event_scope;
/* Caller frame is thread-affine and non-copyable. Acquires ordinary parent pin
 * or innermost active deletion callback ticket. Normal return and pre-longjmp
 * abort both release the event before abandoning its C stack. All providers and
 * caller storage outlive cleanup; no guest execution from cleanup callbacks. */
int tsfp_device_event_scope_begin(tsfp_guest_object_scopes *,tsfp_device_event_scope *,tsfp_device_factory *,uint32_t address);
int tsfp_device_event_scope_end(tsfp_guest_object_scopes *,tsfp_device_event_scope *);
#endif
