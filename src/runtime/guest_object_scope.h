/* Original operation-scoped borrowing. One scope stack per host thread. */
#ifndef TSFP_GUEST_OBJECT_SCOPE_H
#define TSFP_GUEST_OBJECT_SCOPE_H
#include "guest_objects.h"
#define TSFP_GUEST_SCOPE_HANDLES 128
typedef struct tsfp_guest_object_scope {
    struct tsfp_guest_object_scope *previous;
    tsfp_guest_objects *store;
    uint64_t serial;
    size_t count;
    uint32_t tokens[TSFP_GUEST_SCOPE_HANDLES];
    tsfp_guest_object_lease leases[TSFP_GUEST_SCOPE_HANDLES];
} tsfp_guest_object_scope;
typedef struct tsfp_guest_object_scopes {
    tsfp_guest_object_scope *current;
    uint64_t serial;
} tsfp_guest_object_scopes;
/* Frame storage is caller-owned and must outlive leave/abort. Never enter the
 * same frame twice. Thread-affine; provider ownership itself is thread-safe. */
int tsfp_guest_scope_enter(tsfp_guest_object_scopes *,tsfp_guest_object_scope *,tsfp_guest_objects *);
const tsfp_guest_object_lease *tsfp_guest_scope_borrow(tsfp_guest_object_scopes *,uint32_t);
/* Pin a retained identity behind a pseudo handle. Cache includes both alias
 * and key, so rebinding the current identity cannot reuse the previous lease. */
const tsfp_guest_object_lease *tsfp_guest_scope_borrow_key(tsfp_guest_object_scopes *,uint32_t alias,uint64_t key);
int tsfp_guest_scope_leave(tsfp_guest_object_scopes *,tsfp_guest_object_scope *);
/* Explicitly call before a host thread exits or abandons its C stack. */
int tsfp_guest_scope_abort(tsfp_guest_object_scopes *);
uint64_t tsfp_guest_scope_mark(const tsfp_guest_object_scopes *);
/* Unwind only scopes entered after an existing mark; zero means no scopes.
 * Missing/retired marks fail without releasing any ownership. */
int tsfp_guest_scope_unwind(tsfp_guest_object_scopes *,uint64_t);
#endif
