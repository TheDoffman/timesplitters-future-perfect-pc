/* Original deferred device driver callback dispatcher. */
#ifndef TSFP_GUEST_DEVICE_DISPATCH_H
#define TSFP_GUEST_DEVICE_DISPATCH_H
#include "guest_device_callback.h"
#include "guest_callback_call.h"
enum {TSFP_DEVICE_DISPATCH_NOT_STARTED,TSFP_DEVICE_DISPATCH_COMPLETE,TSFP_DEVICE_DISPATCH_QUARANTINED};
/* Processes at most one pending finalization. NOT_STARTED means no ticket was
 * taken (none available or scope cannot enter). A null callback is completed.
 * Invalid driver/target/stack quarantines instead of freeing or retrying guest
 * execution. Caller runs object collect separately, outside guest dispatch.
 * Driver table and guest mappings stay valid; no provider lock crosses guest
 * entry. Existing lifetime hooks must unwind these scopes before nonlocal exit.
 * Guest lookup/output callbacks preserve architectural state on return. */
int tsfp_device_dispatch_one(tsfp_device_factory *,tsfp_guest_object_scopes *,const tsfp_guest_callback_ops *);
#endif
