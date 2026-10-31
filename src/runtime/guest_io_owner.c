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
    if(p) {
        tsfp_io_owner_ops ops=o->ops;uint32_t address=o->address;size_t span=o->span;
        void *attachment=o->attachment;void (*release)(void *)=o->release_attachment;
        o->attachment=NULL;o->release_attachment=NULL;
        if(release)release(attachment);
        ops.dispose(ops.context,address,p,span);
    }
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
int tsfp_io_owner_adopt(tsfp_io_owner *o,const tsfp_io_owner_ops *ops,
    void *target,size_t span,uint32_t address,uint32_t bytes,uint16_t type) {
    tsfp_io_owner value;tsfp_io_request_layout layout;
    if(!o || !ops || !ops->lock || !ops->unlock || !ops->dispose ||
       !tsfp_io_request_validate(target,span,address,type,&layout) || layout.bytes!=bytes)return 0;
    memset(&value,0,sizeof(value));value.ops=*ops;value.bytes=target;value.span=bytes;
    value.address=address;value.clients=1;value.phase=TSFP_IO_READY;*o=value;return 1;
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
static int queue_acquire(tsfp_io_owner *o,void *queue,uint32_t references) {
    int ok=0;if(!o)return 0;
    o->ops.lock(o->ops.context);
    if((o->phase==TSFP_IO_ACTIVE || o->phase==TSFP_IO_PENDING) &&
       !o->waiting_queue && o->clients<=UINT32_MAX-2-references) {
        if(!queue)o->bytes[0x48]=0;
        o->clients+=references;o->waiting_queue=queue;ok=1;
    }
    o->ops.unlock(o->ops.context);return ok;
}
int tsfp_io_owner_queue_claim(tsfp_io_owner *o,void *queue) {return queue?queue_acquire(o,queue,1):0;}
int tsfp_io_owner_queue_start(tsfp_io_owner *o) {return queue_acquire(o,NULL,2);}
int tsfp_io_owner_queue_take(tsfp_io_owner *o,void *queue) {
    int ok=0;if(!o || !queue)return 0;
    o->ops.lock(o->ops.context);
    if(o->waiting_queue==queue) {o->bytes[0x48]=0;o->waiting_queue=NULL;ok=1;}
    o->ops.unlock(o->ops.context);return ok;
}
int tsfp_io_owner_dispatch_begin(tsfp_io_owner *o,tsfp_io_ticket *ticket) {
    if(!o || !ticket || ticket->owner || ticket->kind)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase!=TSFP_IO_READY || o->internal>UINT32_MAX-2) {o->ops.unlock(o->ops.context);return 0;}
    o->phase=TSFP_IO_ACTIVE;o->internal+=2;
    ticket->owner=o;ticket->kind=1;o->ops.unlock(o->ops.context);return 1;
}
int tsfp_io_owner_dispatch_end(tsfp_io_ticket *ticket,int abandoned) {
    tsfp_io_owner *o;
    if(!ticket || (ticket->kind!=1 && ticket->kind!=3 && ticket->kind!=4 && ticket->kind!=5) || !(o=ticket->owner))return 0;
    o->ops.lock(o->ops.context);
    if(abandoned && (o->phase==TSFP_IO_ACTIVE || o->phase==TSFP_IO_PENDING)) {
        o->phase=TSFP_IO_PENDING;o->dispatch_abandoned=1;
    }
    ticket->owner=NULL;ticket->kind=0;o->ops.unlock(o->ops.context);unpin(o);return 1;
}
int tsfp_io_owner_dispatch(tsfp_io_owner *o,tsfp_io_action action,void *context) {
    tsfp_io_ticket ticket={0};
    if(!action || !tsfp_io_owner_dispatch_begin(o,&ticket))return 0;
    if(!tsfp_io_owner_dispatch_publish(&ticket)) {(void)tsfp_io_owner_dispatch_end(&ticket,1);return 0;}
    action(context,o);return tsfp_io_owner_dispatch_end(&ticket,0);
}
int tsfp_io_owner_pending(tsfp_io_owner *o) {
    int ok=0;if(!o)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase==TSFP_IO_ACTIVE) {o->phase=TSFP_IO_PENDING;ok=1;}
    o->ops.unlock(o->ops.context);return ok;
}
int tsfp_io_owner_complete_begin(tsfp_io_owner *o,tsfp_io_ticket *ticket,uint32_t status,uint32_t info) {
    if(!o || !ticket || ticket->owner || ticket->kind)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase!=TSFP_IO_ACTIVE && o->phase!=TSFP_IO_PENDING) {
        o->ops.unlock(o->ops.context);return 0;
    }
    o->phase=TSFP_IO_COMPLETING;
    put(o->bytes+0x10,status);put(o->bytes+0x14,info);
    ticket->owner=o;ticket->kind=2;o->ops.unlock(o->ops.context);return 1;
}
int tsfp_io_owner_complete_end(tsfp_io_ticket *ticket,int abandoned) {
    tsfp_io_owner *o;
    if(!ticket || ticket->kind!=2 || !(o=ticket->owner))return 0;
    o->ops.lock(o->ops.context);
    if(o->phase!=TSFP_IO_COMPLETING) {o->ops.unlock(o->ops.context);return 0;}
    o->phase=abandoned?TSFP_IO_UNCERTAIN:TSFP_IO_COMPLETE;
    ticket->owner=NULL;ticket->kind=0;o->ops.unlock(o->ops.context);
    if(!abandoned)unpin(o);
    return 1;
}
int tsfp_io_owner_complete(tsfp_io_owner *o,uint32_t status,uint32_t info,tsfp_io_action action,void *context) {
    tsfp_io_ticket ticket={0};if(!tsfp_io_owner_complete_begin(o,&ticket,status,info))return 0;
    if(action)action(context,o);
    return tsfp_io_owner_complete_end(&ticket,0);
}

tsfp_io_phase tsfp_io_owner_phase(tsfp_io_owner *o) {
    tsfp_io_phase phase;if(!o)return TSFP_IO_DEAD;
    o->ops.lock(o->ops.context);phase=o->phase;o->ops.unlock(o->ops.context);return phase;
}

int tsfp_io_owner_attach(tsfp_io_owner *o,void *attachment,void (*release)(void *)) {
    int ok=0;if(!o || !attachment || !release)return 0;
    o->ops.lock(o->ops.context);
    if(o->phase==TSFP_IO_READY && !o->release_attachment) {
        o->attachment=attachment;o->release_attachment=release;ok=1;
    }
    o->ops.unlock(o->ops.context);return ok;
}

int tsfp_io_owner_dispatch_publish(tsfp_io_ticket *ticket) {
    tsfp_io_owner *o;int ok=0,forward;
    if(!ticket || (ticket->kind!=1 && ticket->kind!=4) || !(o=ticket->owner))return 0;
    forward=ticket->kind==4;o->ops.lock(o->ops.context);
    if((o->phase==TSFP_IO_ACTIVE || (forward && o->phase==TSFP_IO_PENDING)) && o->internal>=2) {ticket->kind=forward?5:3;ok=1;}
    o->ops.unlock(o->ops.context);return ok;
}
int tsfp_io_owner_dispatch_rollback(tsfp_io_ticket *ticket) {
    tsfp_io_owner *o;unsigned char *bytes;int forward;
    if(!ticket || (ticket->kind!=1 && ticket->kind!=4) || !(o=ticket->owner))return 0;
    forward=ticket->kind==4;o->ops.lock(o->ops.context);
    if(forward) {
        if(o->phase==TSFP_IO_DEAD || !o->internal) {o->ops.unlock(o->ops.context);return 0;}
        --o->internal;
    } else {
        if(o->phase!=TSFP_IO_ACTIVE || o->internal!=2) {o->ops.unlock(o->ops.context);return 0;}
        o->phase=TSFP_IO_READY;o->internal-=2;
    }
    ticket->owner=NULL;ticket->kind=0;bytes=detach(o);
    o->ops.unlock(o->ops.context);dispose(o,bytes);return 1;
}
int tsfp_io_owner_forward_begin(tsfp_io_owner *o,tsfp_io_ticket *ticket) {
    if(!o || !ticket || ticket->owner || ticket->kind)return 0;
    o->ops.lock(o->ops.context);
    if((o->phase!=TSFP_IO_ACTIVE && o->phase!=TSFP_IO_PENDING) || !o->internal || o->internal==UINT32_MAX) {
        o->ops.unlock(o->ops.context);return 0;
    }
    ++o->internal;ticket->owner=o;ticket->kind=4;
    o->ops.unlock(o->ops.context);return 1;
}
