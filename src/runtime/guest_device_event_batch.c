#include "guest_device_event_batch.h"
#include <string.h>
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int release_batch(void *context,int abandoned) {
    tsfp_device_event_batch *frame=(tsfp_device_event_batch *)context;int ok=1;(void)abandoned;
    while(frame->count) {
        --frame->count;if(!tsfp_device_event_release(&frame->events[frame->count]))ok=0;
    }
    return ok;
}
uint32_t tsfp_device_event_batch_begin(tsfp_guest_object_scopes *stack,tsfp_device_event_batch *frame,
    tsfp_device_factory *factory,uint32_t array,uint32_t count) {
    const unsigned char *source;uint32_t i,address;tsfp_device_finalization ticket;
    if(!stack || !frame || !factory || !factory->objects || !factory->ops.input ||
       !count || count>TSFP_DEVICE_EVENT_LIMIT)return TSFP_OBJECT_BAD_PARAMETER;
    if(!array || array>UINT32_MAX-(count*4-1) ||
       !(source=(const unsigned char *)factory->ops.input(factory->ops.context,array,count*4)))return TSFP_OBJECT_BAD_ADDRESS;
    if(!tsfp_guest_scope_enter(stack,&frame->scope,factory->objects))return TSFP_OBJECT_BAD_PARAMETER;
    frame->count=0;memset(frame->events,0,sizeof(frame->events));
    if(!tsfp_guest_scope_on_leave(stack,&frame->scope,release_batch,frame))goto failed;
    for(i=0;i<count;++i) {
        address=word(source+i*4);
        if(!tsfp_device_factory_has_event(factory,address))continue;
        if(tsfp_device_event_pin(factory,address,&frame->events[frame->count]) ||
           (tsfp_device_callback_ticket(stack,factory,address-0x34,&ticket) &&
            tsfp_device_event_pin_finalization(factory,&ticket,address,&frame->events[frame->count]))) {
            ++frame->count;continue;
        }
        goto failed;
    }
    return TSFP_OBJECT_SUCCESS;
failed:
    (void)tsfp_guest_scope_leave(stack,&frame->scope);return TSFP_OBJECT_BAD_PARAMETER;
}
int tsfp_device_event_batch_end(tsfp_guest_object_scopes *stack,tsfp_device_event_batch *frame) {
    return frame && tsfp_guest_scope_leave(stack,&frame->scope);
}
