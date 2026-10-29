#include "guest_io_owner.h"
#include <string.h>
static void put(unsigned char *p,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));
}
/* Called with lock held; marking DEAD makes only one caller responsible for
 * disposal. Metadata lock must not be held across host/guest cleanup. */
static unsigned char *detach(tsfp_io_owner *o) {
    unsigned char *p=NULL;
    if(!o->clients && !o->internal && o->phase!=TSFP_IO_DEAD) {
        p=o->bytes;o->bytes=NULL;o->phase=TSFP_IO_DEAD;
    }
    return p;
}
static void dispose(tsfp_io_owner *o,unsigned char *p) {
    if(p)o->ops.dispose(o->ops.context,o->address,p,o->span);
}
static void unpin(tsfp_io_owner *o) {
    unsigned char *p;
    o->ops.lock(o->ops.context);--o->internal;p=detach(o);
    o->ops.unlock(o->ops.context);dispose(o,p);
}
int tsfp_io_owner_init(tsfp_io_owner *o,const tsfp_io_owner_ops *ops,
    void *target,size_t span,uint32_t address,uint32_t bytes,uint32_t stacks,uint16_t type) {
    tsfp_io_owner candidate;
    if(!o || !ops || !ops->lock || !ops->unlock || !ops->dispose)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.ops=*ops;
    if(!tsfp_io_request_initialize(target,span,address,bytes,stacks,type))return 0;
    candidate.bytes=(unsigned char *)target;candidate.span=span;candidate.address=address;
    candidate.clients=1;candidate.phase=TSFP_IO_READY;*o=candidate;return 1;
}
int tsfp_io_owner_retain(tsfp_io_owner *o) {
    int ok=0;if(!o)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase!=TSFP_IO_DEAD && o->clients<UINT32_MAX-2) {++o->clients;ok=1;}
    o->ops.unlock(o->ops.context);return ok;
}
int tsfp_io_owner_release(tsfp_io_owner *o) {
    int ok=0;unsigned char *p=NULL;if(!o)return 0;
    o->ops.lock(o->ops.context);
    if(o->clients) {--o->clients;ok=1;p=detach(o);}
    o->ops.unlock(o->ops.context);dispose(o,p);return ok;
}
int tsfp_io_owner_dispatch(tsfp_io_owner *o,tsfp_io_action action,void *context) {
    if(!o || !action)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase!=TSFP_IO_READY) {o->ops.unlock(o->ops.context);return 0;}
    o->phase=TSFP_IO_ACTIVE;o->internal+=2;
    o->ops.unlock(o->ops.context);action(context,o);unpin(o);return 1;
}
int tsfp_io_owner_pending(tsfp_io_owner *o) {
    int ok=0;if(!o)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase==TSFP_IO_ACTIVE) {o->phase=TSFP_IO_PENDING;ok=1;}
    o->ops.unlock(o->ops.context);return ok;
}
int tsfp_io_owner_complete(tsfp_io_owner *o,uint32_t status,uint32_t info,
    tsfp_io_action action,void *context) {
    if(!o)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase!=TSFP_IO_ACTIVE && o->phase!=TSFP_IO_PENDING) {
        o->ops.unlock(o->ops.context);return 0;
    }
    o->phase=TSFP_IO_COMPLETING;
    put(o->bytes+0x10,status);put(o->bytes+0x14,info);
    o->ops.unlock(o->ops.context);
    if(action)action(context,o);
    o->ops.lock(o->ops.context);o->phase=TSFP_IO_COMPLETE;
    o->ops.unlock(o->ops.context);unpin(o);return 1;
}
tsfp_io_phase tsfp_io_owner_phase(tsfp_io_owner *o) {
    tsfp_io_phase phase;if(!o)return TSFP_IO_DEAD;
    o->ops.lock(o->ops.context);phase=o->phase;o->ops.unlock(o->ops.context);return phase;
}
