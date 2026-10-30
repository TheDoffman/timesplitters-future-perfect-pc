#include "guest_io_targets.h"
#include <string.h>
static void enter(tsfp_io_targets *t) {t->ops.lock(t->ops.context);}
static void leave(tsfp_io_targets *t) {t->ops.unlock(t->ops.context);}
static int region_valid(const tsfp_io_target_region *r,uint32_t need) {
    if(!r->address)return !r->key && !r->base && !r->bytes && !r->kind;
    return r->key && r->base && r->kind && r->address>=r->base && r->bytes>=need &&
        (uint64_t)r->base+r->bytes<=UINT64_C(0x100000000) &&
        (uint64_t)r->address+need<=(uint64_t)r->base+r->bytes;
}
static int guest_overlap(const tsfp_io_target_region *a,const tsfp_io_target_region *b) {
    return a->address && b->address && (uint64_t)a->address<(uint64_t)b->address+16 &&
        (uint64_t)b->address<(uint64_t)a->address+8;
}
static int release_leases(tsfp_io_targets *t) {
    unsigned i;int clean=1;
    for(i=0;i<2;++i)if(t->lease[i].owner && !tsfp_guest_objects_release(&t->lease[i]))clean=0;
    return clean;
}
int tsfp_io_targets_init(tsfp_io_targets *t,tsfp_guest_objects *objects,uint32_t request,
    const tsfp_io_target_region *iosb,const tsfp_io_target_region *event,const tsfp_io_targets_ops *ops) {
    unsigned i;
    if(!t || !objects || !request || !iosb || !event || !ops || !ops->lock || !ops->unlock || !ops->output ||
       !region_valid(iosb,8) || !region_valid(event,16) || guest_overlap(iosb,event))return 0;
    memset(t,0,sizeof(*t));t->objects=objects;t->request=request;t->ops=*ops;
    t->region[0]=*iosb;t->region[1]=*event;t->state=TSFP_IO_TARGET_BOUND;
    for(i=0;i<2;++i)if(t->region[i].key) {
        if(!tsfp_guest_objects_retain(objects,t->region[i].key)) {(void)tsfp_io_targets_close(t);return 0;}
        t->held[i]=1;
    }
    return 1;
}
static void *pin_span(tsfp_io_targets *t,unsigned i,uint32_t bytes) {
    const tsfp_io_target_region *r=&t->region[i];tsfp_guest_object_lease *pin=&t->lease[i];
    if(!r->address)return NULL;
    if(!tsfp_guest_objects_acquire_key(t->objects,r->key,pin))return NULL;
    if(pin->dispatcher!=r->base || pin->kind!=r->kind)return NULL;
    return t->ops.output(t->ops.context,r->address,bytes);
}
static int host_overlap(void *iosb,void *event) {
    uintptr_t a=(uintptr_t)iosb,b=(uintptr_t)event;
    if(!a || !b)return 0;
    if(a>UINTPTR_MAX-8 || b>UINTPTR_MAX-16)return 1;
    return a<b+16 && b<a+8;
}
int tsfp_io_targets_prepare(tsfp_io_targets *t,const tsfp_io_result *result,tsfp_io_delivery *out) {
    void *iosb=NULL,*event=NULL;tsfp_event_view view={0};tsfp_io_delivery value;int clean;
    if(!t || !result || !out)return 0;
    enter(t);
    if(t->state!=TSFP_IO_TARGET_BOUND || result->address!=t->request ||
       result->user_iosb!=t->region[0].address || result->user_event!=t->region[1].address) {leave(t);return 0;}
    t->state=TSFP_IO_TARGET_PREPARING;leave(t);
    if(t->region[0].address && !(iosb=pin_span(t,0,8)))goto failed;
    if(t->region[1].address) {
        if(!(event=pin_span(t,1,16)) || !tsfp_event_bind(&view,event,16,t->region[1].address))goto failed;
        if(t->lease[1].kind==TSFP_KIND_EVENT_MANUAL || t->lease[1].kind==TSFP_KIND_EVENT_AUTO) {
            if(t->region[1].address!=t->region[1].base ||
               view.synchronization!=(unsigned)(t->lease[1].kind==TSFP_KIND_EVENT_AUTO))goto failed;
        }
    }
    if(host_overlap(iosb,event))goto failed;
    enter(t);t->iosb=iosb;t->event=view;t->state=TSFP_IO_TARGET_PREPARED;leave(t);
    memset(&value,0,sizeof(value));value.cookie=t;value.iosb=iosb;value.iosb_bytes=iosb?8:0;*out=value;return 1;
failed:
    clean=release_leases(t);enter(t);t->state=clean?TSFP_IO_TARGET_BOUND:TSFP_IO_TARGET_QUARANTINED;leave(t);return 0;
}
int tsfp_io_targets_signal(tsfp_io_targets *t,
    int (*wake)(void *,uint32_t,void *,uintptr_t),void *context) {
    uintptr_t host;int ok;
    if(!t || !wake)return 0;
    enter(t);
    if(t->state!=TSFP_IO_TARGET_PREPARED || !t->event.signal) {leave(t);return 0;}
    t->state=TSFP_IO_TARGET_SIGNALING;
    host=(t->lease[1].kind==TSFP_KIND_EVENT_MANUAL || t->lease[1].kind==TSFP_KIND_EVENT_AUTO)?t->lease[1].host:0;
    leave(t);
    ok=tsfp_event_set(&t->event)>=0 && wake(context,t->event.address,t->event.signal,host);
    enter(t);if(t->state!=TSFP_IO_TARGET_SIGNALING)ok=0;
    if(ok)t->state=TSFP_IO_TARGET_SIGNALED;
    leave(t);return ok;
}
int tsfp_io_targets_finish(tsfp_io_targets *t,int disposition) {
    int clean;if(!t)return 0;
    enter(t);
    if(disposition==TSFP_IO_DELIVERY_UNCERTAIN &&
       (t->state==TSFP_IO_TARGET_PREPARED || t->state==TSFP_IO_TARGET_SIGNALING || t->state==TSFP_IO_TARGET_SIGNALED)) {
        t->state=TSFP_IO_TARGET_QUARANTINED;leave(t);return 1;
    }
    if(!((disposition==TSFP_IO_DELIVERY_PRIVATE && t->state==TSFP_IO_TARGET_PREPARED) ||
         (disposition==TSFP_IO_DELIVERY_DONE && t->state==TSFP_IO_TARGET_SIGNALED))) {leave(t);return 0;}
    t->state=TSFP_IO_TARGET_RELEASING;leave(t);clean=release_leases(t);
    enter(t);
    if(clean) {
        t->iosb=NULL;memset(&t->event,0,sizeof(t->event));
        t->state=disposition==TSFP_IO_DELIVERY_PRIVATE?TSFP_IO_TARGET_BOUND:TSFP_IO_TARGET_DELIVERED;
    } else t->state=TSFP_IO_TARGET_QUARANTINED;
    leave(t);return clean;
}
int tsfp_io_targets_close(tsfp_io_targets *t) {
    unsigned i;int clean=1,consumed;if(!t)return 0;
    enter(t);
    if(t->state==TSFP_IO_TARGET_CLOSED) {leave(t);return 1;}
    if(t->state!=TSFP_IO_TARGET_BOUND && t->state!=TSFP_IO_TARGET_DELIVERED) {leave(t);return 0;}
    t->state=TSFP_IO_TARGET_CLOSING;leave(t);
    for(i=0;i<2;++i)if(t->held[i]) {
        t->held[i]=2;consumed=tsfp_guest_objects_dereference(t->objects,t->region[i].key);
        if(consumed)t->held[i]=0;else clean=0;
    }
    enter(t);t->state=clean?TSFP_IO_TARGET_CLOSED:TSFP_IO_TARGET_QUARANTINED;leave(t);return clean;
}
unsigned tsfp_io_targets_state(tsfp_io_targets *t) {
    unsigned state;if(!t)return TSFP_IO_TARGET_EMPTY;
    enter(t);state=t->state;leave(t);return state;
}
