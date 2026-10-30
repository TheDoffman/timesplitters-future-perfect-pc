#include "guest_io_complete.h"
#include <string.h>
static uint32_t word(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put(unsigned char *p,uint32_t value) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));}
int tsfp_io_result_decode(const void *source,size_t span,tsfp_io_result *out) {
    const unsigned char *p=(const unsigned char *)source;uint32_t bytes;tsfp_io_result value={0};
    if(!p || !out || span<TSFP_IO_REQUEST_BYTES || p[0]!=6 || p[1])return 0;
    bytes=(uint32_t)p[2]|((uint32_t)p[3]<<8);
    if(bytes<TSFP_IO_REQUEST_BYTES || bytes>span)return 0;
    value.status=word(p+0x10);value.information=word(p+0x14);value.flags=word(p+4);
    value.user_iosb=word(p+0x1c);value.user_event=word(p+0x20);
    value.overlay_low=word(p+0x28);value.overlay_high=word(p+0x2c);
    value.user_buffer=word(p+0x30);value.segment_array=word(p+0x34);value.locked_buffer_length=word(p+0x38);
    value.pending_returned=p[0x1a];value.cancel=p[0x1b];
    value.thread=word(p+0x50);value.original_file=word(p+0x60);*out=value;return 1;
}
typedef struct {
    tsfp_guest_object_scope scope;tsfp_io_owner *owner;tsfp_io_complete_ops ops;
    tsfp_io_delivery delivery;int prepared,disposition;
} completion_guard;
static int release_guard(void *context,int abandoned) {
    completion_guard *guard=(completion_guard *)context;tsfp_io_owner *owner=guard->owner;
    int clean=1,released,prepared=guard->prepared,disposition=guard->disposition;
    guard->owner=NULL;guard->prepared=0;
    if(abandoned)disposition=TSFP_IO_DELIVERY_UNCERTAIN;
    if(prepared)clean=guard->ops.release(guard->ops.context,guard->delivery.cookie,disposition);
    released=owner?tsfp_io_owner_release(owner):1;return clean && released;
}
static int close_guard(tsfp_guest_object_scopes *scopes,completion_guard *guard,int result) {
    if(!guard->owner)return TSFP_IO_COMPLETE_UNCERTAIN;
    if(scopes->current!=&guard->scope) {
        guard->disposition=TSFP_IO_DELIVERY_UNCERTAIN;
        (void)tsfp_guest_scope_unwind(scopes,guard->scope.serial);result=TSFP_IO_COMPLETE_UNCERTAIN;
    }
    if(scopes->current!=&guard->scope)return TSFP_IO_COMPLETE_UNCERTAIN;
    return tsfp_guest_scope_leave(scopes,&guard->scope)?result:TSFP_IO_COMPLETE_UNCERTAIN;
}
int tsfp_io_complete_request(tsfp_guest_object_scopes *scopes,tsfp_guest_objects *store,
    tsfp_io_owner *owner,const tsfp_io_complete_ops *ops) {
    completion_guard guard;tsfp_io_scope frame;tsfp_io_result value={0};int ok,result;
    if(!owner)return TSFP_IO_COMPLETE_REJECTED;
    if(!scopes || !store || !ops || !ops->prepare || !ops->notify || !ops->release) {
        (void)tsfp_io_owner_release(owner);return TSFP_IO_COMPLETE_REJECTED;
    }
    memset(&guard,0,sizeof(guard));guard.owner=owner;guard.ops=*ops;
    if(!tsfp_guest_scope_enter(scopes,&guard.scope,store)) {
        (void)tsfp_io_owner_release(owner);return TSFP_IO_COMPLETE_REJECTED;
    }
    if(!tsfp_guest_scope_on_leave(scopes,&guard.scope,release_guard,&guard)) {
        (void)tsfp_guest_scope_leave(scopes,&guard.scope);(void)tsfp_io_owner_release(owner);return TSFP_IO_COMPLETE_REJECTED;
    }
    owner->ops.lock(owner->ops.context);
    ok=(owner->phase==TSFP_IO_ACTIVE || owner->phase==TSFP_IO_PENDING) && tsfp_io_result_decode(owner->bytes,owner->span,&value);
    owner->ops.unlock(owner->ops.context);
    value.address=owner->address;
    if(!ok || !ops->prepare(ops->context,&value,&guard.delivery))return close_guard(scopes,&guard,TSFP_IO_COMPLETE_REJECTED);
    guard.prepared=1;
    if(value.user_iosb && (!guard.delivery.iosb || guard.delivery.iosb_bytes<8))return close_guard(scopes,&guard,TSFP_IO_COMPLETE_REJECTED);
    if(!tsfp_io_completion_scope_begin(scopes,&frame,store,owner,value.status,value.information))return close_guard(scopes,&guard,TSFP_IO_COMPLETE_REJECTED);
    guard.disposition=TSFP_IO_DELIVERY_UNCERTAIN;
    if(value.user_iosb) {put((unsigned char *)guard.delivery.iosb,value.status);put((unsigned char *)guard.delivery.iosb+4,value.information);}
    ok=ops->notify(ops->context,guard.delivery.cookie,&value);
    if(scopes->current!=&frame.scope)return close_guard(scopes,&guard,TSFP_IO_COMPLETE_UNCERTAIN);
    if(!ok)frame.scope.abandoned=1;
    result=tsfp_io_scope_end(scopes,&frame) && ok?TSFP_IO_COMPLETE_DONE:TSFP_IO_COMPLETE_UNCERTAIN;
    if(result==TSFP_IO_COMPLETE_DONE)guard.disposition=TSFP_IO_DELIVERY_DONE;
    return close_guard(scopes,&guard,result);
}
