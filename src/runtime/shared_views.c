#include "shared_views.h"
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

static void enter(tsfp_shared_views *s) {
#ifdef _MSC_VER
    while(_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while(__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_shared_views *s) {
#ifdef _MSC_VER
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int span(uintptr_t a,size_t n) { return a && n && a<=UINTPTR_MAX-n; }
static int power_two(size_t n) { return n && !(n&(n-1)); }
static int release_view(tsfp_shared_views *s,tsfp_shared_view *v) {
    if(!s->backend.unmap(s->backend.context,v->base,v->bytes))return 0;
    memset(v,0,sizeof(*v));--s->live;return 1;
}
int tsfp_shared_views_init(tsfp_shared_views *s,const tsfp_shared_backend *backend,
    size_t bytes,size_t page,size_t granularity,tsfp_shared_view *views,size_t capacity) {
    tsfp_shared_backend copy;
    uintptr_t token=0,a=(uintptr_t)s,b=(uintptr_t)views;
    size_t storage;
    if(!s || !backend || !backend->open || !backend->close || !backend->map || !backend->unmap ||
       !power_two(page) || !power_two(granularity) || granularity<page ||
       !bytes || bytes%page || !capacity || capacity>SIZE_MAX/sizeof(*views))return 0;
    storage=capacity*sizeof(*views);
    if(!span(a,sizeof(*s)) || !span(b,storage) || (a<b+storage && b<a+sizeof(*s)) ||
       s->backing || s->active || s->live)return 0;
    copy=*backend;
    if(!copy.open(copy.context,bytes,&token))return 0;
    if(!token)return 0; /* backend contract violation; no token to consume */
    memset(s,0,sizeof(*s));memset(views,0,storage);
    s->backend=copy;s->backing=token;s->bytes=bytes;s->page_size=page;
    s->granularity=granularity;s->views=views;s->capacity=capacity;s->active=1;
    return 1;
}
void *tsfp_shared_views_map(tsfp_shared_views *s,void *hint,size_t offset,size_t bytes,unsigned access) {
    tsfp_shared_view *slot=NULL;
    void *base=NULL;
    uintptr_t requested=(uintptr_t)hint;
    if(!s)return NULL;
    enter(s);
    if(!s->active || !s->backing || !bytes || bytes%s->page_size ||
       offset%s->granularity || offset>s->bytes || bytes>s->bytes-offset ||
       (access!=TSFP_VIEW_READ && access!=TSFP_VIEW_WRITE) ||
       (hint && (!span(requested,bytes) || requested%s->granularity)))goto done;
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_shared_view *v=s->views+i;
        if(!v->base) { if(!slot)slot=v; continue; }
        if(hint && requested<(uintptr_t)v->base+v->bytes &&
           (uintptr_t)v->base<requested+bytes)goto done;
    }
    if(!slot)goto done; /* Never map storage that cannot be recorded. */
    base=s->backend.map(s->backend.context,s->backing,offset,bytes,hint,access);
    if(!base)goto done;
    slot->base=base;slot->bytes=bytes;slot->offset=offset;slot->published=0;++s->live;
    if(hint && base!=hint) {
        (void)release_view(s,slot);
        base=NULL;goto done;
    }
    slot->published=1;
done:
    leave(s);return base;
}
int tsfp_shared_views_release(tsfp_shared_views *s,void *base) {
    int result=0;
    if(!s || !base)return 0;
    enter(s);
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_shared_view *v=s->views+i;
        if(v->base==base && v->published) { result=release_view(s,v);break; }
    }
    leave(s);return result;
}
int tsfp_shared_views_reap(tsfp_shared_views *s) {
    int result=1;
    if(!s)return 0;
    enter(s);
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_shared_view *v=s->views+i;
        if(v->base && !v->published && !release_view(s,v))result=0;
    }
    leave(s);return result;
}
int tsfp_shared_views_stop(tsfp_shared_views *s) {
    int result;
    if(!s)return 0;
    enter(s);s->active=0;
    for(size_t i=0;i<s->capacity;++i)
        if(s->views[i].base)(void)release_view(s,s->views+i);
    result=s->live==0;
    if(result && s->backing) {
        s->backend.close(s->backend.context,s->backing);s->backing=0;
    }
    leave(s);return result;
}
