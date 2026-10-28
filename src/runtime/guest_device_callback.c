#include "guest_device_callback.h"
static int finish(void *context,int abandoned) {
    tsfp_device_callback *frame=(tsfp_device_callback *)context;
    if(!frame->active)return 1;
    frame->active=0;
    return tsfp_device_factory_finish_finalization(frame->factory,&frame->ticket,!abandoned);
}
int tsfp_device_callback_ticket(const tsfp_guest_object_scopes *stack,tsfp_device_factory *factory,
    uint32_t body,tsfp_device_finalization *out) {
    tsfp_guest_object_scope *scope;
    if(!stack || !factory || !body || !out)return 0;
    for(scope=stack->current;scope;scope=scope->previous)if(scope->cleanup==finish && scope->cleanup_context) {
        tsfp_device_callback *frame=(tsfp_device_callback *)scope->cleanup_context;
        if(frame->active && frame->factory==factory && frame->ticket.body==body) {*out=frame->ticket;return 1;}
    }
    return 0;
}
int tsfp_device_callback_begin(tsfp_guest_object_scopes *stack,tsfp_device_callback *frame,tsfp_device_factory *factory) {
    if(!stack || !frame || !factory || !factory->objects)return 0;
    if(!tsfp_guest_scope_enter(stack,&frame->scope,factory->objects))return 0;
    frame->active=0;frame->factory=factory;
    if(!tsfp_guest_scope_on_leave(stack,&frame->scope,finish,frame) ||
       !tsfp_device_factory_take_finalization(factory,&frame->ticket)) {
        (void)tsfp_guest_scope_leave(stack,&frame->scope);return 0;
    }
    frame->active=1;return 1;
}
int tsfp_device_callback_end(tsfp_guest_object_scopes *stack,tsfp_device_callback *frame) {
    return frame && tsfp_guest_scope_leave(stack,&frame->scope);
}
int tsfp_device_callback_abandon(tsfp_guest_object_scopes *stack,tsfp_device_callback *frame) {
    if(!stack || !frame || stack->current!=&frame->scope)return 0;
    frame->scope.abandoned=1;return tsfp_guest_scope_leave(stack,&frame->scope);
}
