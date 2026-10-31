#include "guest_objects.h"
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#define TOKEN_TAG UINT32_C(0x48000000)
#define SLOT_MASK UINT32_C(0x3fff)
#define LAST_GENERATION UINT32_C(0x3ff)

static void enter(tsfp_guest_objects *s) {
#if defined(_MSC_VER)
    while (_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while (__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_guest_objects *s) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int span(const void *p,size_t n) {
    return p && n && (uintptr_t)p<=UINTPTR_MAX-(n-1);
}
static int overlap(const void *a,size_t an,const void *b,size_t bn) {
    return (uintptr_t)a<=(uintptr_t)b+bn-1 && (uintptr_t)b<=(uintptr_t)a+an-1;
}
int tsfp_guest_objects_init(tsfp_guest_objects *s,tsfp_guest_handle_record *h,
    tsfp_guest_object_record *o,size_t n,int (*close_host)(void *,uintptr_t),
    int (*free_dispatcher)(void *,uint32_t,uint32_t,uint32_t),void *ctx) {
    size_t hn,on;
    if(!n || n>SLOT_MASK || !close_host) return 0;
    hn=n*sizeof(*h);on=n*sizeof(*o);
    if(!span(s,sizeof(*s)) || !span(h,hn) || !span(o,on) ||
       overlap(s,sizeof(*s),h,hn) || overlap(s,sizeof(*s),o,on) || overlap(h,hn,o,on)) return 0;
    memset(s,0,sizeof(*s));memset(h,0,hn);memset(o,0,on);
    s->handles=h;s->objects=o;s->capacity=n;s->close_host=close_host;s->context=ctx;
    s->free_dispatcher=free_dispatcher;
    return 1;
}
static tsfp_guest_handle_record *handle(tsfp_guest_objects *s,uint32_t token) {
    uint32_t slot=token&SLOT_MASK;
    tsfp_guest_handle_record *h;
    if((token&UINT32_C(0xff000000))!=TOKEN_TAG || !slot || slot>s->capacity) return NULL;
    h=&s->handles[slot-1];
    return h->used==1 && h->generation==((token>>14)&LAST_GENERATION)?h:NULL;
}
static uint64_t object_key(tsfp_guest_objects *s,tsfp_guest_object_record *o) {
    return ((uint64_t)o->generation<<32)|(uint32_t)(o-s->objects+1);
}
static tsfp_guest_object_record *object(tsfp_guest_objects *s,uint64_t key) {
    uint32_t slot=(uint32_t)key;
    tsfp_guest_object_record *o;
    if(!slot || slot>s->capacity) return NULL;
    o=&s->objects[slot-1];
    return o->state==TSFP_OBJECT_OPEN && o->generation==(uint32_t)(key>>32)?o:NULL;
}
static tsfp_guest_handle_record *vacant(tsfp_guest_objects *s) {
    size_t i;
    for(i=0;i<s->capacity;++i) if(!s->handles[i].used && !s->handles[i].exhausted) return &s->handles[i];
    return NULL;
}
static uint32_t install(tsfp_guest_objects *s,tsfp_guest_handle_record *h,tsfp_guest_object_record *o) {
    h->object=(uint32_t)(o-s->objects);h->used=1;++o->handles;
    return TOKEN_TAG|(h->generation<<14)|(uint32_t)(h-s->handles+1);
}
static void consume(tsfp_guest_objects *s,tsfp_guest_handle_record *h) {
    --s->objects[h->object].handles;h->used=0;h->object=0;
    if(h->generation==LAST_GENERATION) h->exhausted=1;
    else ++h->generation;
}
static void cleanup(tsfp_guest_objects *s,size_t index,int retry) {
    tsfp_guest_object_record *o=&s->objects[index];
    uintptr_t host;
    int result;
    enter(s);
    if((o->state!=TSFP_OBJECT_OPEN && !(retry && o->state==TSFP_OBJECT_RETRY)) ||
       o->handles || o->refs || o->leases) { leave(s);return; }
    o->state=TSFP_OBJECT_CLOSING;host=o->host;leave(s);
    result=host?s->close_host(s->context,host):1;
    /* A successful host close is durable even if guest storage cleanup fails.
     * Keep the object unavailable throughout both callbacks and any retry. */
    if(result==1) {
        uint32_t dispatcher;
        enter(s);o->host=0;dispatcher=o->dispatcher;leave(s);
        if(dispatcher) result=s->free_dispatcher(s->context,dispatcher,o->kind,o->flags);
    }
    enter(s);
    if(result==1) { uint32_t generation=o->generation;memset(o,0,sizeof(*o));o->generation=generation; }
    else o->state=result==0?TSFP_OBJECT_RETRY:TSFP_OBJECT_UNCERTAIN;
    leave(s);
}
int tsfp_guest_objects_adopt(tsfp_guest_objects *s,uintptr_t host,uint32_t kind,uint32_t flags,uint32_t *token) {
    tsfp_guest_handle_record *h;
    size_t i;
    if(!s || !host || !token) return 0;
    enter(s);h=vacant(s);
    if(h) for(i=0;i<s->capacity;++i) {
        tsfp_guest_object_record *o=&s->objects[i];
        if(!o->state && o->generation!=UINT32_MAX) {
            ++o->generation;o->state=TSFP_OBJECT_OPEN;o->host=host;o->kind=kind;o->flags=flags;
            *token=install(s,h,o);leave(s);return 1;
        }
    }
    leave(s);return 0;
}
int tsfp_guest_objects_adopt_body(tsfp_guest_objects *s,uint32_t body,
    uint32_t kind,uint32_t flags,uint64_t *key) {
    size_t i;
    if(!s || !body || !key || !s->free_dispatcher)return 0;
    enter(s);
    for(i=0;i<s->capacity;++i)if(s->objects[i].state && s->objects[i].dispatcher==body) {
        leave(s);return 0;
    }
    for(i=0;i<s->capacity;++i) {
        tsfp_guest_object_record *o=&s->objects[i];
        if(!o->state && o->generation!=UINT32_MAX) {
            ++o->generation;o->state=TSFP_OBJECT_OPEN;o->host=0;
            o->kind=kind;o->flags=flags;o->dispatcher=body;o->refs=1;
            *key=object_key(s,o);leave(s);return 1;
        }
    }
    leave(s);return 0;
}
int tsfp_guest_objects_prepare(tsfp_guest_objects *s,uint32_t kind,uint32_t flags,uint32_t *ticket) {
    tsfp_guest_handle_record *h;size_t i;
    if(!s || !ticket) return 0;
    enter(s);h=vacant(s);
    if(h) for(i=0;i<s->capacity;++i) {
        tsfp_guest_object_record *o=&s->objects[i];
        if(!o->state && o->generation!=UINT32_MAX) {
            ++o->generation;o->state=TSFP_OBJECT_PREPARING;o->kind=kind;o->flags=flags;
            *ticket=install(s,h,o);h->used=2;leave(s);return 1;
        }
    }
    leave(s);return 0;
}
static tsfp_guest_handle_record *prepared(tsfp_guest_objects *s,uint32_t token) {
    uint32_t slot=token&SLOT_MASK;tsfp_guest_handle_record *h;
    if((token&UINT32_C(0xff000000))!=TOKEN_TAG || !slot || slot>s->capacity) return NULL;
    h=&s->handles[slot-1];
    return h->used==2 && h->generation==((token>>14)&LAST_GENERATION) &&
        s->objects[h->object].state==TSFP_OBJECT_PREPARING?h:NULL;
}
int tsfp_guest_objects_commit(tsfp_guest_objects *s,uint32_t token,uintptr_t host) {
    tsfp_guest_handle_record *h;
    if(!s || !host) return 0;
    enter(s);h=prepared(s,token);
    if(!h) { leave(s);return 0; }
    s->objects[h->object].host=host;s->objects[h->object].state=TSFP_OBJECT_OPEN;
    h->used=1;leave(s);return 1;
}
int tsfp_guest_objects_cancel(tsfp_guest_objects *s,uint32_t token) {
    tsfp_guest_handle_record *h;tsfp_guest_object_record *o;uint32_t generation;
    if(!s) return 0;
    enter(s);h=prepared(s,token);
    if(!h) { leave(s);return 0; }
    o=&s->objects[h->object];generation=o->generation;consume(s,h);
    memset(o,0,sizeof(*o));o->generation=generation;leave(s);return 1;
}
int tsfp_guest_objects_duplicate_checked(tsfp_guest_objects *s,uint32_t source,int close_source,uint32_t *token) {
    tsfp_guest_handle_record *h,*target;
    tsfp_guest_object_record *o;
    size_t index;
    int ok=0;
    if(!s || !token) return 0;
    enter(s);h=handle(s,source);
    if(!h) { leave(s);return 0; }
    index=h->object;o=&s->objects[index];
    /* Consume first so an exhausted table may reuse the released slot. */
    if(close_source) consume(s,h);
    target=vacant(s);
    if(target && o->handles!=UINT32_MAX) { *token=install(s,target,o);ok=1; }
    leave(s);cleanup(s,index,0);return ok?1:-1;
}
int tsfp_guest_objects_duplicate(tsfp_guest_objects *s,uint32_t source,int close_source,uint32_t *token) {
    return tsfp_guest_objects_duplicate_checked(s,source,close_source,token)==1;
}
int tsfp_guest_objects_open_key(tsfp_guest_objects *s,uint64_t key,uint32_t *token) {
    tsfp_guest_object_record *o;tsfp_guest_handle_record *h;
    if(!s || !token) return 0;
    enter(s);o=object(s,key);h=vacant(s);
    if(!o || (!o->refs && !o->leases) || !h || o->handles==UINT32_MAX) {
        leave(s);return 0;
    }
    *token=install(s,h,o);leave(s);return 1;
}
int tsfp_guest_objects_close(tsfp_guest_objects *s,uint32_t token) {
    tsfp_guest_handle_record *h;size_t index;
    if(!s) return 0;
    enter(s);h=handle(s,token);
    if(!h) { leave(s);return 0; }
    index=h->object;consume(s,h);leave(s);cleanup(s,index,0);return 1;
}
int tsfp_guest_objects_acquire(tsfp_guest_objects *s,uint32_t token,tsfp_guest_object_lease *lease) {
    tsfp_guest_handle_record *h;tsfp_guest_object_record *o;
    if(!s || !lease || lease->owner) return 0;
    enter(s);h=handle(s,token);
    if(!h) { leave(s);return 0; }
    o=&s->objects[h->object];
    if(o->leases==UINT32_MAX) { leave(s);return 0; }
    ++o->leases;lease->owner=s;lease->key=object_key(s,o);lease->host=o->host;
    lease->kind=o->kind;lease->flags=o->flags;lease->dispatcher=o->dispatcher;
    leave(s);return 1;
}
int tsfp_guest_objects_acquire_key(tsfp_guest_objects *s,uint64_t key,tsfp_guest_object_lease *lease) {
    tsfp_guest_object_record *o;
    if(!s || !lease || lease->owner) return 0;
    enter(s);o=object(s,key);
    if(!o || (!o->handles && !o->refs) || o->leases==UINT32_MAX) { leave(s);return 0; }
    ++o->leases;lease->owner=s;lease->key=key;lease->host=o->host;
    lease->kind=o->kind;lease->flags=o->flags;lease->dispatcher=o->dispatcher;
    leave(s);return 1;
}
int tsfp_guest_objects_release(tsfp_guest_object_lease *lease) {
    tsfp_guest_objects *s;tsfp_guest_object_record *o;size_t index;
    if(!lease || !(s=lease->owner)) return 0;
    enter(s);o=object(s,lease->key);
    if(!o || !o->leases) { leave(s);return 0; }
    index=(size_t)(o-s->objects);--o->leases;memset(lease,0,sizeof(*lease));
    leave(s);cleanup(s,index,0);return 1;
}
int tsfp_guest_objects_reference(tsfp_guest_objects *s,uint32_t token,uint64_t *key) {
    tsfp_guest_handle_record *h;tsfp_guest_object_record *o;
    if(!s || !key) return 0;
    enter(s);h=handle(s,token);
    if(!h) { leave(s);return 0; }
    o=&s->objects[h->object];
    if(o->refs==UINT32_MAX) { leave(s);return 0; }
    ++o->refs;*key=object_key(s,o);leave(s);return 1;
}
int tsfp_guest_objects_reference_dispatcher(tsfp_guest_objects *s,uint32_t dispatcher,uint64_t *key) {
    size_t i;
    if(!s || !dispatcher || !key) return 0;
    enter(s);
    for(i=0;i<s->capacity;++i) {
        tsfp_guest_object_record *o=&s->objects[i];
        if(o->state==TSFP_OBJECT_OPEN && o->dispatcher==dispatcher &&
           (o->handles || o->refs || o->leases) && o->refs!=UINT32_MAX) {
            ++o->refs;*key=object_key(s,o);leave(s);return 1;
        }
    }
    leave(s);return 0;
}
int tsfp_guest_objects_dereference_dispatcher(tsfp_guest_objects *s,uint32_t dispatcher) {
    size_t i;
    if(!s || !dispatcher) return 0;
    enter(s);
    for(i=0;i<s->capacity;++i) {
        tsfp_guest_object_record *o=&s->objects[i];
        if(o->state==TSFP_OBJECT_OPEN && o->dispatcher==dispatcher && o->refs) {
            --o->refs;leave(s);cleanup(s,i,0);return 1;
        }
    }
    leave(s);return 0;
}
int tsfp_guest_objects_retain(tsfp_guest_objects *s,uint64_t key) {
    tsfp_guest_object_record *o;
    if(!s) return 0;
    enter(s);o=object(s,key);
    if(!o || (!o->handles && !o->refs && !o->leases) || o->refs==UINT32_MAX) { leave(s);return 0; }
    ++o->refs;leave(s);return 1;
}
int tsfp_guest_objects_dereference(tsfp_guest_objects *s,uint64_t key) {
    tsfp_guest_object_record *o;size_t index;
    if(!s) return 0;
    enter(s);o=object(s,key);
    if(!o || !o->refs) { leave(s);return 0; }
    index=(size_t)(o-s->objects);--o->refs;leave(s);cleanup(s,index,0);return 1;
}
int tsfp_guest_objects_publish(tsfp_guest_objects *s,uint64_t key,uint32_t candidate,uint32_t *winner) {
    tsfp_guest_object_record *o;size_t i;
    if(!s || !s->free_dispatcher || !candidate || !winner) return 0;
    enter(s);o=object(s,key);
    if(!o || (!o->refs && !o->leases)) { leave(s);return 0; }
    if(!o->dispatcher) for(i=0;i<s->capacity;++i) {
        tsfp_guest_object_record *other=&s->objects[i];
        if(other!=o && other->state && other->dispatcher==candidate) { leave(s);return 0; }
    }
    if(!o->dispatcher) o->dispatcher=candidate;
    *winner=o->dispatcher;leave(s);return 1;
}
size_t tsfp_guest_objects_collect(tsfp_guest_objects *s) {
    size_t i,pending=0;
    if(!s) return 0;
    for(i=0;i<s->capacity;++i) cleanup(s,i,1);
    enter(s);
    for(i=0;i<s->capacity;++i) if(s->objects[i].state && !s->objects[i].handles &&
        !s->objects[i].refs && !s->objects[i].leases) ++pending;
    leave(s);return pending;
}
