/* Original lifetime wrappers for project device embedded locks. Existing
 * wait/pulse scheduling remains transitional in the renamed legacy routes. */
#include "guest_device_event_batch.h"
static void bridge_KeSetEvent_legacy(void);
static void bridge_KeResetEvent_legacy(void);
static void bridge_KePulseEvent_legacy(void);
static void bridge_KeSetEventBoostPriority_legacy(void);
static void bridge_KeWaitForSingleObject_legacy(void);
static void bridge_KeWaitForMultipleObjects_legacy(void);
static void event_wake_waiters(uint32_t);
static int bridge_device_pin_event(tsfp_device_event_scope *frame,uint32_t address) {
    if(!bridge_devices_initialized || !tsfp_device_factory_has_event(&bridge_devices,address))return 0;
    if(bridge_device_event_begin(frame,address))return 1;
    g_eax=TSFP_OBJECT_BAD_PARAMETER;return -1;
}
static void bridge_device_event_forward(void (*legacy)(void)) {
    tsfp_device_event_scope frame;int state=bridge_device_pin_event(&frame,STACK_ARG(0));
    if(state<0)return;
    legacy();
    if(state && !tsfp_device_event_scope_end(&bridge_object_scopes,&frame))abort();
}
static void bridge_KeSetEvent(void) {bridge_device_event_forward(bridge_KeSetEvent_legacy);}
static void bridge_KePulseEvent(void) {bridge_device_event_forward(bridge_KePulseEvent_legacy);}
static void bridge_KeWaitForSingleObject(void) {bridge_device_event_forward(bridge_KeWaitForSingleObject_legacy);}
static void bridge_KeResetEvent(void) {
    tsfp_device_event_scope frame;int state=bridge_device_pin_event(&frame,STACK_ARG(0)),value;
    if(state<0)return;
    if(!state) {bridge_KeResetEvent_legacy();return;}
    value=tsfp_device_event_reset(&frame.event);g_eax=value<0?TSFP_OBJECT_BAD_PARAMETER:(uint32_t)value;
    if(!tsfp_device_event_scope_end(&bridge_object_scopes,&frame))abort();
}
static void bridge_KeSetEventBoostPriority(void) {
    uint32_t address=STACK_ARG(0);tsfp_device_event_scope frame;int state=bridge_device_pin_event(&frame,address);
    if(state<0)return;
    if(!state) {bridge_KeSetEventBoostPriority_legacy();return;}
    if(tsfp_device_event_set(&frame.event)<0)g_eax=TSFP_OBJECT_BAD_PARAMETER;
    else {event_wake_waiters(address);g_eax=0;}
    if(!tsfp_device_event_scope_end(&bridge_object_scopes,&frame))abort();
}
static void bridge_KeWaitForMultipleObjects(void) {
    tsfp_device_event_batch frame;uint32_t count=STACK_ARG(0),array=STACK_ARG(1),status;
    if(!bridge_devices_initialized) {bridge_KeWaitForMultipleObjects_legacy();return;}
    status=tsfp_device_event_batch_begin(&bridge_object_scopes,&frame,&bridge_devices,array,count);
    if(status) {g_eax=status;return;}
    bridge_KeWaitForMultipleObjects_legacy();
    if(!tsfp_device_event_batch_end(&bridge_object_scopes,&frame))abort();
}
