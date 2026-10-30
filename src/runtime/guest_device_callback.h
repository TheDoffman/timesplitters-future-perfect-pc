/* Original device finalization ownership across guest callback execution. */
#ifndef TSFP_GUEST_DEVICE_CALLBACK_H
#define TSFP_GUEST_DEVICE_CALLBACK_H
#include "guest_device_factory.h"
#include "guest_object_scope.h"
typedef struct {
    tsfp_guest_object_scope scope;
    tsfp_device_factory *factory;
    tsfp_device_finalization ticket;
    unsigned active;
} tsfp_device_callback;
/* Thread-affine caller-owned frame; never re-enter an active frame. The caller
 * enters guest code only after success. Normal end acknowledges the callback;
 * existing scope unwind/abort quarantines an abandoned callback's storage.
 * Hooks must unwind before discarding the C stack. No worker join or actual
 * driver callback dispatch is provided here. Factory/owner/context outlive it. */
/* Find the innermost active callback for this factory/body on this host thread.
 * This copies an identity ticket, not a new ownership reference. */
int tsfp_device_callback_ticket(const tsfp_guest_object_scopes *,tsfp_device_factory *,uint32_t body,tsfp_device_finalization *);
int tsfp_device_callback_begin(tsfp_guest_object_scopes *,tsfp_device_callback *,tsfp_device_factory *);
/* Explicit rejection after taking a ticket, without unwinding outer scopes. */
int tsfp_device_callback_abandon(tsfp_guest_object_scopes *,tsfp_device_callback *);
int tsfp_device_callback_end(tsfp_guest_object_scopes *,tsfp_device_callback *);
#endif
