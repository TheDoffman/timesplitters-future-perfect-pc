#include "guest_io_scheduler.h"
#include <string.h>
static void enter(tsfp_io_scheduler *m) {m->lock(m->context);}
static void leave(tsfp_io_scheduler *m) {m->unlock(m->context);}
static void queue_enter(void *context) {enter(context);}
static void queue_leave(void *context) {leave(context);}
static tsfp_io_schedule_record *find(tsfp_io_scheduler *m,uint32_t body,size_t *index) {
    size_t i;for(i=0;i<m->capacity;++i)if(m->records[i].state!=TSFP_IO_QUEUE_FREE && m->records[i].body==body) {
        *index=i;return &m->records[i];
    }
    return NULL;
}
static tsfp_io_schedule_record *ticket_record(tsfp_io_queue_ticket *t) {
    tsfp_io_schedule_record *r;
    if(!t || !t->active || !t->manager || t->index>=t->manager->capacity)return NULL;
    r=&t->manager->records[t->index];
    return r->state==TSFP_IO_QUEUE_LIVE && r->generation==t->generation && r->references?r:NULL;
}
/* LIVE/PREPARING -> RETIRING under lock, device cleanup outside. The caller has
 * already dropped the last API reference; no ticket can access retiring slots. */
static int retire_locked(tsfp_io_scheduler *m,tsfp_io_schedule_record *r) {
    int clean;
    if(!r->closed || r->references || r->queue.current || r->queue.count)return 1;
    r->state=TSFP_IO_QUEUE_RETIRING;leave(m);
    clean=!r->device.owner || tsfp_guest_objects_release(&r->device);
    enter(m);
    if(clean) {r->state=TSFP_IO_QUEUE_FREE;r->body=0;r->wire.device=NULL;}
    else r->state=TSFP_IO_QUEUE_QUARANTINED;
    return clean;
}
int tsfp_io_scheduler_init(tsfp_io_scheduler *m,tsfp_device_factory *factory,
    tsfp_io_schedule_record *records,size_t capacity,tsfp_io_owner **entries,size_t waiting,
    void *context,void (*lock)(void *),void (*unlock)(void *)) {
    tsfp_io_scheduler value;
    if(!m || !factory || !factory->objects || !records || !entries || !capacity || !waiting ||
       capacity>SIZE_MAX/sizeof(*records) || waiting>SIZE_MAX/sizeof(*entries)/capacity || !lock || !unlock)return 0;
    memset(&value,0,sizeof(value));value.factory=factory;value.records=records;
    value.entries=entries;value.capacity=capacity;value.waiting_capacity=waiting;
    value.context=context;value.lock=lock;value.unlock=unlock;
    memset(records,0,capacity*sizeof(*records));memset(entries,0,capacity*waiting*sizeof(*entries));*m=value;return 1;
}
int tsfp_io_scheduler_acquire(tsfp_io_scheduler *m,uint32_t body,int create,tsfp_io_queue_ticket *out) {
    tsfp_io_schedule_record *r;tsfp_io_queue_ticket value;size_t index=0,i;void *mapping=NULL;uint32_t bytes=0;int prepared;
    if(!m || !body || !out || out->active || out->manager)return 0;
    enter(m);r=find(m,body,&index);
    if(r) {
        if(r->state!=TSFP_IO_QUEUE_LIVE || (create && (m->closed || r->closed)) || r->references==UINT32_MAX)goto rejected;
        ++r->references;
    } else {
        if(!create || m->closed)goto rejected;
        for(i=0;i<m->capacity;++i)if(m->records[i].state==TSFP_IO_QUEUE_FREE && m->records[i].generation!=UINT64_MAX)break;
        if(i==m->capacity)goto rejected;
        index=i;r=&m->records[i];++r->generation;r->body=body;r->references=1;r->closed=0;
        r->state=TSFP_IO_QUEUE_PREPARING;memset(&r->queue,0,sizeof(r->queue));memset(&r->wire,0,sizeof(r->wire));
        leave(m);
        prepared=tsfp_device_factory_pin(m->factory,body,&r->device,&mapping,&bytes);
        if(prepared)prepared=bytes>=72 && !((unsigned char *)mapping)[0x1f] &&
            tsfp_io_queue_init(&r->queue,m->entries+index*m->waiting_capacity,m->waiting_capacity,m,queue_enter,queue_leave) &&
            tsfp_io_queue_wire_init(&r->wire,&r->queue,mapping,bytes,body);
        enter(m);
        if(!prepared || m->closed || r->closed) {
            r->closed=1;r->references=0;(void)retire_locked(m,r);leave(m);return 0;
        }
        r->state=TSFP_IO_QUEUE_LIVE;
    }
    memset(&value,0,sizeof(value));value.manager=m;value.index=index;value.generation=r->generation;value.active=1;
    *out=value;leave(m);return 1;
rejected:
    leave(m);return 0;
}
int tsfp_io_scheduler_release(tsfp_io_queue_ticket *t) {
    tsfp_io_scheduler *m;tsfp_io_schedule_record *r;int clean;
    if(!t || !(m=t->manager))return 0;
    enter(m);r=ticket_record(t);if(!r) {leave(m);return 0;}
    --r->references;memset(t,0,sizeof(*t));clean=retire_locked(m,r);leave(m);return clean;
}
static tsfp_io_schedule_record *borrow(tsfp_io_queue_ticket *t) {
    tsfp_io_schedule_record *r;tsfp_io_scheduler *m;
    if(!t || !(m=t->manager))return NULL;
    enter(m);r=ticket_record(t);leave(m);return r;
}
int tsfp_io_scheduler_submit(tsfp_io_queue_ticket *t,tsfp_io_owner *owner,tsfp_io_devices *devices,tsfp_io_owner **selected) {
    tsfp_io_schedule_record *r=borrow(t);void *mapping;uint32_t bytes;
    if(!r || !owner || !devices || devices->owner!=owner || devices->factory!=t->manager->factory ||
       !tsfp_io_devices_pin(devices,r->body,&mapping,&bytes) || mapping!=r->wire.device || bytes<72)return 0;
    return tsfp_io_queue_submit(&r->queue,owner,selected);
}
int tsfp_io_scheduler_next(tsfp_io_queue_ticket *t,tsfp_io_owner *expected,tsfp_io_owner **selected) {
    tsfp_io_schedule_record *r=borrow(t);return r?tsfp_io_queue_next(&r->queue,expected,selected):0;
}
int tsfp_io_scheduler_close(tsfp_io_scheduler *m,uint32_t body) {
    tsfp_io_schedule_record *r;tsfp_io_queue_ticket t;size_t index=0;int clean;
    if(!m || !body)return 0;
    enter(m);r=find(m,body,&index);
    if(!r) {leave(m);return 0;}
    if(r->state==TSFP_IO_QUEUE_PREPARING) {r->closed=1;leave(m);return 1;}
    if(r->state!=TSFP_IO_QUEUE_LIVE) {leave(m);return 0;}
    r->closed=1;r->queue.closed=1;
    if(r->references==UINT32_MAX) {leave(m);return 0;}
    ++r->references;
    memset(&t,0,sizeof(t));t.manager=m;t.index=index;t.generation=r->generation;t.active=1;
    leave(m);tsfp_io_queue_stop(&r->queue);clean=tsfp_io_scheduler_release(&t);return clean;
}
void tsfp_io_scheduler_stop(tsfp_io_scheduler *m) {
    size_t i;uint32_t body;if(!m)return;
    enter(m);m->closed=1;
    for(i=0;i<m->capacity;++i) {
        if(m->records[i].state==TSFP_IO_QUEUE_PREPARING)m->records[i].closed=1;
        if(m->records[i].state==TSFP_IO_QUEUE_LIVE) {
            m->records[i].closed=1;m->records[i].queue.closed=1;
        }
    }
    leave(m);
    for(i=0;i<m->capacity;++i) {
        enter(m);body=m->records[i].state==TSFP_IO_QUEUE_PREPARING || m->records[i].state==TSFP_IO_QUEUE_LIVE?m->records[i].body:0;leave(m);
        if(body)(void)tsfp_io_scheduler_close(m,body);
    }
}
void tsfp_io_scheduler_counts(tsfp_io_scheduler *m,size_t *live,size_t *quarantined) {
    size_t i,a=0,b=0;if(!m)return;
    enter(m);for(i=0;i<m->capacity;++i)if(m->records[i].state!=TSFP_IO_QUEUE_FREE) {
        if(m->records[i].state==TSFP_IO_QUEUE_QUARANTINED)++b;else ++a;
    }
    if(live)*live=a;if(quarantined)*quarantined=b;leave(m);
}
static int release_scope(void *context,int abandoned) {
    tsfp_io_queue_scope *frame=context;(void)abandoned;frame->stack=NULL;
    return frame->ticket.active?tsfp_io_scheduler_release(&frame->ticket):1;
}
int tsfp_io_queue_scope_begin(tsfp_guest_object_scopes *s,tsfp_io_queue_scope *frame,tsfp_io_scheduler *m,uint32_t body,int create) {
    if(!s || !frame || !m || !tsfp_guest_scope_enter(s,&frame->scope,m->factory->objects))return 0;
    memset(&frame->ticket,0,sizeof(frame->ticket));frame->stack=s;
    if(!tsfp_guest_scope_on_leave(s,&frame->scope,release_scope,frame))goto failed;
    if(tsfp_io_scheduler_acquire(m,body,create,&frame->ticket))return 1;
failed:
    (void)tsfp_guest_scope_leave(s,&frame->scope);frame->stack=NULL;return 0;
}
int tsfp_io_queue_scope_end(tsfp_guest_object_scopes *s,tsfp_io_queue_scope *frame) {return frame && frame->stack==s && tsfp_guest_scope_leave(s,&frame->scope);}
int tsfp_io_scheduler_startio(tsfp_guest_object_scopes *s,tsfp_io_queue_scope *frame,
    tsfp_io_owner *owner,tsfp_io_devices *devices,const tsfp_io_call_ops *ops) {
    tsfp_io_schedule_record *r;
    if(!frame || frame->stack!=s || !(r=borrow(&frame->ticket)) || !devices || devices->factory!=frame->ticket.manager->factory) {
        if(owner)(void)tsfp_io_owner_release(owner);return TSFP_IO_CALL_NOT_ENTERED;
    }
    return tsfp_io_startio(s,owner,devices,r->body,ops);
}
