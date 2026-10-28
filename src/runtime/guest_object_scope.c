#include "guest_object_scope.h"
#include <string.h>
int tsfp_guest_scope_enter(tsfp_guest_object_scopes *stack,tsfp_guest_object_scope *frame,tsfp_guest_objects *store) {
    tsfp_guest_object_scope *p;
    if(!stack || !frame || !store || stack->serial==UINT64_MAX) return 0;
    for(p=stack->current;p;p=p->previous) if(p==frame) return 0;
    memset(frame,0,sizeof(*frame));frame->store=store;frame->previous=stack->current;
    frame->serial=++stack->serial;
    stack->current=frame;return 1;
}
const tsfp_guest_object_lease *tsfp_guest_scope_borrow(tsfp_guest_object_scopes *stack,uint32_t token) {
    tsfp_guest_object_scope *frame;size_t i;
    if(!stack || !(frame=stack->current)) return NULL;
    for(i=0;i<frame->count;++i) if(frame->tokens[i]==token) return &frame->leases[i];
    if(frame->count==TSFP_GUEST_SCOPE_HANDLES) return NULL;
    i=frame->count;
    if(!tsfp_guest_objects_acquire(frame->store,token,&frame->leases[i])) return NULL;
    frame->tokens[i]=token;++frame->count;return &frame->leases[i];
}
const tsfp_guest_object_lease *tsfp_guest_scope_borrow_key(tsfp_guest_object_scopes *stack,uint32_t alias,uint64_t key) {
    tsfp_guest_object_scope *frame;size_t i;
    if(!stack || !(frame=stack->current)) return NULL;
    for(i=0;i<frame->count;++i)
        if(frame->tokens[i]==alias && frame->leases[i].key==key) return &frame->leases[i];
    if(frame->count>=TSFP_GUEST_SCOPE_HANDLES) return NULL;
    i=frame->count;
    if(!tsfp_guest_objects_acquire_key(frame->store,key,&frame->leases[i])) return NULL;
    frame->tokens[i]=alias;++frame->count;return &frame->leases[i];
}
int tsfp_guest_scope_leave(tsfp_guest_object_scopes *stack,tsfp_guest_object_scope *frame) {
    int ok=1;
    if(!stack || !frame || stack->current!=frame) return 0;
    stack->current=frame->previous;frame->previous=NULL;
    while(frame->count) {
        --frame->count;
        if(!tsfp_guest_objects_release(&frame->leases[frame->count])) ok=0;
    }
    frame->store=NULL;return ok;
}
int tsfp_guest_scope_abort(tsfp_guest_object_scopes *stack) {
    int ok=1;
    if(!stack) return 0;
    while(stack->current) if(!tsfp_guest_scope_leave(stack,stack->current)) ok=0;
    return ok;
}
uint64_t tsfp_guest_scope_mark(const tsfp_guest_object_scopes *stack) {
    return stack && stack->current?stack->current->serial:0;
}
int tsfp_guest_scope_unwind(tsfp_guest_object_scopes *stack,uint64_t mark) {
    tsfp_guest_object_scope *p;int ok=1;
    if(!stack) return 0;
    if(mark) {
        for(p=stack->current;p && p->serial!=mark;p=p->previous) { }
        if(!p) return 0;
    }
    while(stack->current && stack->current->serial!=mark)
        if(!tsfp_guest_scope_leave(stack,stack->current)) ok=0;
    return ok;
}
