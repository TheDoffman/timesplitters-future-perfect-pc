#include "mmio_views.h"
#include <string.h>
static int span(uintptr_t a,size_t n) { return a && n && a<=UINTPTR_MAX-n; }
int tsfp_mmio_views_init(tsfp_mmio_views *s,const tsfp_shared_backend *backend,
    size_t page,size_t granularity,tsfp_mmio_view *views,size_t capacity) {
    uintptr_t a=(uintptr_t)s,b=(uintptr_t)views;
    size_t n;
    tsfp_shared_backend copy;
    if(!s || !backend || !backend->open || !backend->close || !backend->map || !backend->unmap ||
       !page || (page&(page-1)) || granularity<page || (granularity&(granularity-1)) ||
       !capacity || capacity>SIZE_MAX/sizeof(*views) || s->active || s->views)return 0;
    n=capacity*sizeof(*views);
    if(!span(a,sizeof(*s)) || !span(b,n) || (a<b+n && b<a+sizeof(*s)))return 0;
    copy=*backend;memset(s,0,sizeof(*s));memset(views,0,n);
    s->backend=copy;s->page_size=page;s->granularity=granularity;s->views=views;
    s->capacity=capacity;s->active=1;return 1;
}
void *tsfp_mmio_views_map(tsfp_mmio_views *s,void *hint,size_t bytes) {
    tsfp_mmio_view *slot=NULL;
    uintptr_t a=(uintptr_t)hint;
    size_t rounded;
    if(!s || !s->active || !bytes || bytes>SIZE_MAX-(s->page_size-1))return NULL;
    rounded=(bytes+s->page_size-1)&~(s->page_size-1);
    if(hint && (!span(a,rounded) || a%s->granularity))return NULL;
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_mmio_view *v=s->views+i;
        if(!v->store.backing) { if(!slot)slot=v;continue; }
        if(hint && v->guest && a<(uintptr_t)v->guest+v->store.bytes &&
           (uintptr_t)v->guest<a+rounded)return NULL;
    }
    if(!slot || !tsfp_shared_views_init(&slot->store,&s->backend,rounded,s->page_size,
        s->granularity,slot->views,2))return NULL;
    slot->guest=tsfp_shared_views_map(&slot->store,hint,0,rounded,TSFP_VIEW_WRITE);
    if(slot->guest)slot->alias=tsfp_shared_views_map(&slot->store,NULL,0,rounded,TSFP_VIEW_WRITE);
    if(!slot->alias) {
        if(tsfp_shared_views_stop(&slot->store))memset(slot,0,sizeof(*slot));
        return NULL;
    }
    slot->bytes=bytes;slot->visible=1;return slot->guest;
}
void *tsfp_mmio_views_alias(const tsfp_mmio_views *s,const void *p,size_t bytes) {
    uintptr_t a=(uintptr_t)p;
    if(!s || !span(a,bytes))return NULL;
    for(size_t i=0;i<s->capacity;++i) {
        const tsfp_mmio_view *v=s->views+i;
        uintptr_t b=(uintptr_t)v->guest;
        if(v->visible && a>=b && a-b<v->bytes && bytes<=v->bytes-(a-b))
            return (unsigned char *)v->alias+(a-b);
    }
    return NULL;
}
int tsfp_mmio_views_reset(tsfp_mmio_views *s) {
    int result=1;
    if(!s)return 0;
    s->active=0;
    for(size_t i=0;i<s->capacity;++i)s->views[i].visible=0;
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_mmio_view *v=s->views+i;
        if(v->store.backing && !tsfp_shared_views_stop(&v->store))result=0;
        else memset(v,0,sizeof(*v));
    }
    if(result)memset(s,0,sizeof(*s));
    return result;
}
