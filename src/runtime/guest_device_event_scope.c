#include "guest_device_event_scope.h"
#include <string.h>
static int release_event(void *context,int abandoned) {
    tsfp_device_event_scope *frame=(tsfp_device_event_scope *)context;(void)abandoned;
    if(!frame->active)return 1;
    frame->active=0;return tsfp_device_event_release(&frame->event);
}
int tsfp_device_event_scope_begin(tsfp_guest_object_scopes *stack,tsfp_device_event_scope *frame,
    tsfp_device_factory *factory,uint32_t address) {
    tsfp_device_finalization ticket;
    if(!stack || !frame || !factory || !factory->objects || address<0x34)return 0;
    if(!tsfp_guest_scope_enter(stack,&frame->scope,factory->objects))return 0;
    frame->active=0;memset(&frame->event,0,sizeof(frame->event));
    if(!tsfp_guest_scope_on_leave(stack,&frame->scope,release_event,frame))goto failed;
    if(tsfp_device_event_pin(factory,address,&frame->event) ||
       (tsfp_device_callback_ticket(stack,factory,address-0x34,&ticket) &&
        tsfp_device_event_pin_finalization(factory,&ticket,address,&frame->event))) {
        frame->active=1;return 1;
    }
failed:
    (void)tsfp_guest_scope_leave(stack,&frame->scope);return 0;
}
int tsfp_device_event_scope_end(tsfp_guest_object_scopes *stack,tsfp_device_event_scope *frame) {
    return frame && tsfp_guest_scope_leave(stack,&frame->scope);
}
