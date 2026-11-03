#include "guest_execution_state.h"
#include <limits.h>
#include <stddef.h>

#ifdef _MSC_VER
#define EXECUTION_TLS __declspec(thread)
#else
#define EXECUTION_TLS _Thread_local
#endif
typedef struct execution_state {
    const tsfp_guest_execution_ops *domain;
    uint32_t lock_depth,active_depth;
} execution_state;
static EXECUTION_TLS execution_state current;

static int valid(const tsfp_guest_execution_ops *ops) {
    return ops && ops->lock_enabled && ops->lock && ops->unlock &&
        ops->activate && ops->deactivate && (!current.domain || current.domain==ops);
}
static void bind(const tsfp_guest_execution_ops *ops) {current.domain=ops;}
static uintptr_t owner(void) {return (uintptr_t)&current;}
int tsfp_guest_execution_capture(const tsfp_guest_execution_ops *ops,tsfp_guest_execution_snapshot *out) {
    tsfp_guest_execution_snapshot saved;
    if(!out || !valid(ops))return 0;
    saved.domain=ops;saved.owner=owner();saved.lock_depth=current.lock_depth;
    saved.active_depth=current.active_depth;*out=saved;return 1;
}
int tsfp_guest_execution_lock_enter(const tsfp_guest_execution_ops *ops) {
    if(!valid(ops) || current.lock_depth==INT_MAX)return 0;
    bind(ops);
    if(ops->lock_enabled(ops->opaque)){ops->lock(ops->opaque);++current.lock_depth;}
    return 1;
}
int tsfp_guest_execution_lock_leave(const tsfp_guest_execution_ops *ops) {
    if(!valid(ops))return 0;bind(ops);
    if(current.lock_depth){ops->unlock(ops->opaque);--current.lock_depth;}
    return 1;
}
int tsfp_guest_execution_lock_drop(const tsfp_guest_execution_ops *ops) {
    unsigned depth;
    if(!valid(ops))return -1;bind(ops);depth=current.lock_depth;
    while(current.lock_depth){ops->unlock(ops->opaque);--current.lock_depth;}
    return (int)depth;
}
int tsfp_guest_execution_lock_restore(const tsfp_guest_execution_ops *ops,unsigned depth) {
    if(!valid(ops) || depth>(unsigned)INT_MAX-current.lock_depth ||
       (depth && !ops->lock_enabled(ops->opaque)))return 0;
    bind(ops);
    while(depth--){ops->lock(ops->opaque);++current.lock_depth;}
    return 1;
}
int tsfp_guest_execution_enter(const tsfp_guest_execution_ops *ops) {
    if(!valid(ops) || current.active_depth==INT_MAX)return 0;bind(ops);
    if(!current.active_depth)ops->activate(ops->opaque);
    ++current.active_depth;return 1;
}
int tsfp_guest_execution_leave(const tsfp_guest_execution_ops *ops) {
    if(!valid(ops))return 0;bind(ops);
    if(current.active_depth && !--current.active_depth)ops->deactivate(ops->opaque);
    return 1;
}
int tsfp_guest_execution_restore(const tsfp_guest_execution_ops *ops,const tsfp_guest_execution_snapshot *snapshot) {
    tsfp_guest_execution_snapshot saved;int change;
    if(!snapshot || !valid(ops))return 0;saved=*snapshot;
    if(saved.domain!=ops || saved.owner!=owner() || saved.lock_depth>INT_MAX ||
       saved.active_depth>INT_MAX || (saved.lock_depth>current.lock_depth &&
       !ops->lock_enabled(ops->opaque)))return 0;
    bind(ops);change=current.lock_depth!=saved.lock_depth;
    if(current.active_depth && (change || !saved.active_depth)) {
        ops->deactivate(ops->opaque);current.active_depth=0;
    }
    while(current.lock_depth>saved.lock_depth){ops->unlock(ops->opaque);--current.lock_depth;}
    while(current.lock_depth<saved.lock_depth){ops->lock(ops->opaque);++current.lock_depth;}
    if(saved.active_depth && !current.active_depth)ops->activate(ops->opaque);
    current.active_depth=saved.active_depth;return 1;
}
int tsfp_guest_execution_suspend(const tsfp_guest_execution_ops *ops,tsfp_guest_execution_snapshot *out) {
    tsfp_guest_execution_snapshot saved,empty;
    if(!out || !tsfp_guest_execution_capture(ops,&saved))return 0;
    empty=saved;empty.lock_depth=empty.active_depth=0;
    if(!tsfp_guest_execution_restore(ops,&empty))return 0;
    *out=saved;return 1;
}
