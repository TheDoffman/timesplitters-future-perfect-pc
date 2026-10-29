#include "guest_io_call.h"
typedef struct {tsfp_guest_object_scope scope;tsfp_io_owner *owner;} call_guard;
typedef struct {tsfp_guest_object_scopes *scopes;tsfp_io_scope *frame;int published;} call_admission;
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
static int release_client(void *context,int abandoned) {
    call_guard *g=(call_guard *)context;tsfp_io_owner *owner=g->owner;(void)abandoned;
    g->owner=NULL;return owner?tsfp_io_owner_release(owner):1;
}
static int close_guard(tsfp_guest_object_scopes *scopes,call_guard *g,int result) {
    if(!g->owner)return TSFP_IO_CALL_UNCERTAIN;
    if(scopes->current!=&g->scope) {
        (void)tsfp_guest_scope_unwind(scopes,g->scope.serial);result=TSFP_IO_CALL_UNCERTAIN;
    }
    if(scopes->current!=&g->scope)return TSFP_IO_CALL_UNCERTAIN;
    return tsfp_guest_scope_leave(scopes,&g->scope)?result:TSFP_IO_CALL_UNCERTAIN;
}
static int admit_call(void *context) {
    call_admission *a=(call_admission *)context;
    a->published=tsfp_io_dispatch_scope_publish(a->scopes,a->frame);return a->published;
}
static int restore_slot(tsfp_io_owner *owner,uint32_t expected,unsigned location,uint32_t current) {
    unsigned char *p=owner->bytes;size_t offset;
    if(!p || owner->span<TSFP_IO_REQUEST_BYTES || expected<owner->address || !location || p[0x19]!=location-1 || word(p+0x5c)!=expected)return 0;
    offset=(size_t)(expected-owner->address);
    if(offset>owner->span || owner->span-offset<TSFP_IO_STACK_BYTES)return 0;
    p[0x19]=(unsigned char)location;put(p+0x5c,current);
    return 1;
}
int tsfp_io_call_driver(tsfp_guest_object_scopes *scopes,tsfp_io_owner *owner,tsfp_io_devices *devices,
    uint32_t body,const tsfp_io_call_ops *ops,uint32_t *status) {
    call_guard guard;tsfp_io_scope frame;call_admission admission;
    tsfp_guest_callback_plan callback;tsfp_io_target target;
    void *device;const void *driver;uint32_t span,address,args[2],stack,value,old_stack;
    unsigned old_location;int ok,result;
    if(!owner)return TSFP_IO_CALL_NOT_ENTERED;
    if(!scopes || !devices || devices->owner!=owner || !devices->factory || !ops || !ops->input || !status) {
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
    driver=ops->input(ops->context,address,68);
    if(!tsfp_io_target_prepare(owner->bytes,owner->span,owner->address,device,span,body,driver,68,address,&target))goto rejected;
    args[0]=body;args[1]=owner->address;
    if(!tsfp_guest_callback_prepare(&ops->callback,target.routine,args,2,&callback))goto rejected;
    old_location=owner->bytes[0x19];old_stack=word(owner->bytes+0x5c);
    if(tsfp_io_owner_phase(owner)==TSFP_IO_READY)ok=tsfp_io_dispatch_scope_begin(scopes,&frame,devices->factory->objects,owner);
    else ok=tsfp_io_forward_scope_begin(scopes,&frame,devices->factory->objects,owner);
    if(!ok)goto rejected;
    ok=tsfp_io_request_advance(owner->bytes,owner->span,owner->address,&stack);
    if(!ok) {
        result=tsfp_io_dispatch_scope_cancel(scopes,&frame)?TSFP_IO_CALL_NOT_ENTERED:TSFP_IO_CALL_UNCERTAIN;
        return close_guard(scopes,&guard,result);
    }
    if(stack!=target.stack) {
        if(restore_slot(owner,target.stack,old_location,old_stack))
            result=tsfp_io_dispatch_scope_cancel(scopes,&frame)?TSFP_IO_CALL_NOT_ENTERED:TSFP_IO_CALL_UNCERTAIN;
        else {frame.scope.abandoned=1;(void)tsfp_io_scope_end(scopes,&frame);result=TSFP_IO_CALL_UNCERTAIN;}
        return close_guard(scopes,&guard,result);
    }
    admission.scopes=scopes;admission.frame=&frame;admission.published=0;
    ok=tsfp_guest_callback_invoke(&callback,admit_call,&admission,&value);
    if(scopes->current!=&frame.scope)return close_guard(scopes,&guard,TSFP_IO_CALL_UNCERTAIN);
    if(!ok && !admission.published) {
        if(restore_slot(owner,target.stack,old_location,old_stack))
            result=tsfp_io_dispatch_scope_cancel(scopes,&frame)?TSFP_IO_CALL_NOT_ENTERED:TSFP_IO_CALL_UNCERTAIN;
        else {frame.scope.abandoned=1;(void)tsfp_io_scope_end(scopes,&frame);result=TSFP_IO_CALL_UNCERTAIN;}
        return close_guard(scopes,&guard,result);
    }
    if(!ok)frame.scope.abandoned=1;
    result=tsfp_io_scope_end(scopes,&frame) && ok?TSFP_IO_CALL_RETURNED:TSFP_IO_CALL_UNCERTAIN;
    result=close_guard(scopes,&guard,result);
    if(result==TSFP_IO_CALL_RETURNED)*status=value;
    return result;
rejected:
    return close_guard(scopes,&guard,TSFP_IO_CALL_NOT_ENTERED);
}
