/* Original request callback ownership through operation-scope unwinding. */
#ifndef TSFP_GUEST_IO_SCOPE_H
#define TSFP_GUEST_IO_SCOPE_H
#include "guest_io_owner.h"
#include "guest_object_scope.h"
typedef struct {tsfp_guest_object_scope scope;tsfp_io_ticket ticket;unsigned active;} tsfp_io_scope;
/* Thread-affine caller-owned frame. Existing lifetime hooks must unwind before
 * discarding its C stack. Store/owner/lock context remain valid until cleanup.
 * Normal wrappers still require normal callbacks; guest adapters use these
 * explicit scopes when callbacks can transfer control nonlocally. */
int tsfp_io_dispatch_scope_begin(tsfp_guest_object_scopes *,tsfp_io_scope *,tsfp_guest_objects *,tsfp_io_owner *);
int tsfp_io_forward_scope_begin(tsfp_guest_object_scopes *,tsfp_io_scope *,tsfp_guest_objects *,tsfp_io_owner *);
int tsfp_io_completion_scope_begin(tsfp_guest_object_scopes *,tsfp_io_scope *,tsfp_guest_objects *,tsfp_io_owner *,uint32_t,uint32_t);
int tsfp_io_scope_end(tsfp_guest_object_scopes *,tsfp_io_scope *);
/* Current-frame publication; cancellation only consumes a private unpublished
 * dispatch frame. A published/wrong-kind/noncurrent frame is left unchanged.
 * If a prepared frame cannot safely roll back, cancellation unwinds it as
 * abandoned and returns0, retaining any operation whose disposition is unknown. */
int tsfp_io_dispatch_scope_publish(tsfp_guest_object_scopes *,tsfp_io_scope *);
int tsfp_io_dispatch_scope_cancel(tsfp_guest_object_scopes *,tsfp_io_scope *);
#endif
