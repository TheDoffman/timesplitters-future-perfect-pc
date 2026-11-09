#include "guest_execution_scope.h"
#ifdef _MSC_VER
#define EXECUTION_SCOPE_TLS __declspec(thread)
#else
#define EXECUTION_SCOPE_TLS _Thread_local
#endif
typedef struct execution_unwind {
    struct execution_unwind *previous;
    tsfp_guest_object_scopes *stack;
    tsfp_guest_execution_snapshot target;
} execution_unwind;
static EXECUTION_SCOPE_TLS execution_unwind *unwinding;
static int restore_execution(void *opaque,int abandoned) {
    tsfp_guest_execution_scope *frame=opaque;
    if(abandoned) {
        if(!unwinding || unwinding->stack!=frame->stack)return 0;
        unwinding->target=frame->saved;return 1;
    }
    return tsfp_guest_execution_restore(frame->saved.domain,&frame->saved);
}
int tsfp_guest_execution_scope_enter(tsfp_guest_object_scopes *stack,tsfp_guest_execution_scope *frame,
                                    tsfp_guest_objects *objects,const tsfp_guest_execution_ops *ops) {
    tsfp_guest_execution_snapshot saved,empty;
    if(!frame || !tsfp_guest_execution_capture(ops,&saved) ||
       !tsfp_guest_scope_enter(stack,&frame->objects,objects))return 0;
    frame->saved=saved;frame->stack=stack;
    if(!tsfp_guest_scope_on_leave(stack,&frame->objects,restore_execution,frame)) {
        tsfp_guest_scope_leave(stack,&frame->objects);return 0;
    }
    empty=saved;empty.lock_depth=empty.active_depth=0;
    if(!tsfp_guest_execution_restore(ops,&empty)) {
        tsfp_guest_scope_leave(stack,&frame->objects);return 0;
    }
    return 1;
}
int tsfp_guest_execution_scope_leave(tsfp_guest_object_scopes *stack,tsfp_guest_execution_scope *frame) {
    tsfp_guest_execution_snapshot discarded;
    if(!stack || !frame || stack->current!=&frame->objects ||
       !tsfp_guest_execution_suspend(frame->saved.domain,&discarded))return 0;
    return tsfp_guest_scope_leave(stack,&frame->objects);
}
static int unwind_to(tsfp_guest_object_scopes *stack,uint64_t mark,
                      const tsfp_guest_execution_ops *ops,int resume) {
    execution_unwind batch;tsfp_guest_object_scope *cursor;
    tsfp_guest_execution_snapshot empty;int ok,restored;
    if(!stack)return 0;
    if(mark) {
        for(cursor=stack->current;cursor && cursor->serial!=mark;cursor=cursor->previous){}
        if(!cursor)return 0;
    }
    if(!tsfp_guest_execution_capture(ops,&batch.target))return 0;
    if(resume && (!stack->current || stack->current->serial==mark))return 1;
    empty=batch.target;empty.lock_depth=empty.active_depth=0;
    if(!tsfp_guest_execution_restore(ops,&empty))return 0;
    batch.stack=stack;batch.previous=unwinding;unwinding=&batch;
    ok=tsfp_guest_scope_unwind(stack,mark);
    unwinding=batch.previous;
    restored=tsfp_guest_execution_restore(ops,resume?&batch.target:&empty);
    return ok && restored;
}
int tsfp_guest_execution_scope_unwind(tsfp_guest_object_scopes *stack,uint64_t mark,
                                     const tsfp_guest_execution_ops *ops) {
    return unwind_to(stack,mark,ops,1);
}
int tsfp_guest_execution_scope_abort(tsfp_guest_object_scopes *stack,const tsfp_guest_execution_ops *ops) {
    return unwind_to(stack,0,ops,0);
}
