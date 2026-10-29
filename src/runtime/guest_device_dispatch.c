#include "guest_device_dispatch.h"
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int tsfp_device_dispatch_one(tsfp_device_factory *factory,tsfp_guest_object_scopes *scopes,const tsfp_guest_callback_ops *ops) {
    tsfp_device_callback frame;const unsigned char *driver;uint32_t routine;
    if(!factory || !scopes || !ops || !ops->lookup || !ops->output)return TSFP_DEVICE_DISPATCH_NOT_STARTED;
    if(!tsfp_device_callback_begin(scopes,&frame,factory))return TSFP_DEVICE_DISPATCH_NOT_STARTED;
    driver=frame.ticket.driver && frame.ticket.driver<=UINT32_MAX-67
        ?(const unsigned char *)factory->ops.input(factory->ops.context,frame.ticket.driver,68):NULL;
    if(!driver)goto rejected;
    routine=word(driver+4);
    if(routine && !tsfp_guest_callback_one(ops,routine,frame.ticket.body))goto rejected;
    if(!tsfp_device_callback_end(scopes,&frame))return TSFP_DEVICE_DISPATCH_QUARANTINED;
    return TSFP_DEVICE_DISPATCH_COMPLETE;
rejected:
    (void)tsfp_device_callback_abandon(scopes,&frame);return TSFP_DEVICE_DISPATCH_QUARANTINED;
}
