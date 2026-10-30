#include "guest_io_startio.h"
typedef struct {tsfp_guest_object_scope scope;tsfp_io_owner *owner;} start_guard;
typedef struct {tsfp_guest_object_scopes *scopes;tsfp_io_scope *frame;int published;} start_admission;
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int release_client(void *context,int abandoned) {
    start_guard *g=(start_guard *)context;tsfp_io_owner *owner=g->owner;(void)abandoned;
    g->owner=NULL;return owner?tsfp_io_owner_release(owner):1;
}
static int close_guard(tsfp_guest_object_scopes *scopes,start_guard *g,int result) {
    if(!g->owner)return TSFP_IO_CALL_UNCERTAIN;
    if(scopes->current!=&g->scope) {
        (void)tsfp_guest_scope_unwind(scopes,g->scope.serial);result=TSFP_IO_CALL_UNCERTAIN;
    }
    if(scopes->current!=&g->scope)return TSFP_IO_CALL_UNCERTAIN;
    return tsfp_guest_scope_leave(scopes,&g->scope)?result:TSFP_IO_CALL_UNCERTAIN;
}
static int admit_start(void *context) {
    start_admission *a=(start_admission *)context;
    a->published=tsfp_io_dispatch_scope_publish(a->scopes,a->frame);return a->published;
}
int tsfp_io_startio(tsfp_guest_object_scopes *scopes,tsfp_io_owner *owner,tsfp_io_devices *devices,
    uint32_t body,const tsfp_io_call_ops *ops) {
    start_guard guard;tsfp_io_scope frame;start_admission admission;
    tsfp_guest_callback_plan callback;void *device;const unsigned char *driver;
    uint32_t span,address,routine,args[2];int ok,result;
    if(!owner)return TSFP_IO_CALL_NOT_ENTERED;
    if(!scopes || !devices || devices->owner!=owner || !devices->factory || !ops || !ops->input) {
        (void)tsfp_io_owner_release(owner);return TSFP_IO_CALL_NOT_ENTERED;
    }
    guard.owner=owner;
    if(!tsfp_guest_scope_enter(scopes,&guard.scope,devices->factory->objects)) {
        (void)tsfp_io_owner_release(owner);return TSFP_IO_CALL_NOT_ENTERED;
    }
    if(!tsfp_guest_scope_on_leave(scopes,&guard.scope,release_client,&guard)) {
        (void)tsfp_guest_scope_leave(scopes,&guard.scope);(void)tsfp_io_owner_release(owner);return TSFP_IO_CALL_NOT_ENTERED;
    }
    if(!tsfp_io_devices_pin(devices,body,&device,&span) || span<72)goto rejected;
    address=word((const unsigned char *)device+8);
    if(!address || address>UINT32_MAX-67)goto rejected;
    driver=(const unsigned char *)ops->input(ops->context,address,68);
    if(!driver || !(routine=word(driver)))goto rejected;
    args[0]=body;args[1]=owner->address;
    if(!tsfp_guest_callback_prepare(&ops->callback,routine,args,2,&callback))goto rejected;
    if(!tsfp_io_forward_scope_begin(scopes,&frame,devices->factory->objects,owner))goto rejected;
    admission.scopes=scopes;admission.frame=&frame;admission.published=0;
    ok=tsfp_guest_callback_invoke(&callback,admit_start,&admission,NULL);
    if(scopes->current!=&frame.scope)return close_guard(scopes,&guard,TSFP_IO_CALL_UNCERTAIN);
    if(!ok && !admission.published) {
        result=tsfp_io_dispatch_scope_cancel(scopes,&frame)?TSFP_IO_CALL_NOT_ENTERED:TSFP_IO_CALL_UNCERTAIN;
        return close_guard(scopes,&guard,result);
    }
    if(!ok)frame.scope.abandoned=1;
    result=tsfp_io_scope_end(scopes,&frame) && ok?TSFP_IO_CALL_RETURNED:TSFP_IO_CALL_UNCERTAIN;
    return close_guard(scopes,&guard,result);
rejected:
    return close_guard(scopes,&guard,TSFP_IO_CALL_NOT_ENTERED);
}
