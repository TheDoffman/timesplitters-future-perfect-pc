#include "owned_arena.h"
#include "shared_views.h"
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
enum { PUBLISHED=1, ABANDONED=2 };
static void enter(tsfp_owned_arena *s) {
#ifdef _MSC_VER
    while(_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while(__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_owned_arena *s) {
#ifdef _MSC_VER
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int span(uintptr_t a,size_t n) { return a && n && a<=UINTPTR_MAX-n; }
static int rounded(size_t page,size_t *n) {
    if(!*n || *n>SIZE_MAX-(page-1))return 0;
    *n=(*n+page-1)&~(page-1);return 1;
}
int tsfp_owned_arena_init(tsfp_owned_arena *s,const tsfp_arena_backend *b,size_t bytes,
    size_t page,tsfp_arena_view *v,size_t capacity) {
    uintptr_t a=(uintptr_t)s,c=(uintptr_t)v;
    size_t n;
    tsfp_arena_backend copy;
    void *base;
    if(!s || !b || !b->reserve || !b->release || !b->place || !b->restore ||
       !page || (page&(page-1)) || !rounded(page,&bytes) || !capacity ||
       capacity>SIZE_MAX/sizeof(*v))return 0;
    n=capacity*sizeof(*v);
    if(!span(a,sizeof(*s)) || !span(c,n) || (a<c+n && c<a+sizeof(*s)) ||
       s->base || s->live || s->active)return 0;
    copy=*b;base=copy.reserve(copy.context,bytes);
    if(!base)return 0;
    memset(s,0,sizeof(*s));memset(v,0,n);
    s->backend=copy;s->base=base;s->bytes=bytes;s->page_size=page;s->views=v;
    s->capacity=capacity;s->active=1;return 1;
}
int tsfp_owned_arena_intersects(const tsfp_owned_arena *s,const void *p,size_t n) {
    uintptr_t a=(uintptr_t)p,b;
    if(!s || !s->base || !a)return 0;
    b=(uintptr_t)s->base;
    /* Zero-byte releases still route by their starting address. Overflowing
     * spans that reach the arena route here to be rejected, never to MAP_FIXED. */
    if(a>=b && a-b<s->bytes)return 1;
    return a<b && n>b-a;
}
void *tsfp_owned_arena_map(tsfp_owned_arena *s,void *p,size_t bytes,
    uintptr_t token,size_t offset,unsigned access) {
    uintptr_t a=(uintptr_t)p,b;
    tsfp_arena_view *slot=NULL;
    int result;
    void *out=NULL;
    if(!s)return NULL;
    enter(s);b=(uintptr_t)s->base;
    if(!s->active || s->poisoned || !b || a<s->page_size || a%s->page_size ||
       !rounded(s->page_size,&bytes) || !span(a,bytes) || a<b || a-b>s->bytes ||
       bytes>s->bytes-(a-b) || offset%s->page_size || (!token && offset) ||
       access>7)goto done;
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_arena_view *v=s->views+i;
        if(!v->base) { if(!slot)slot=v;continue; }
        if(a<(uintptr_t)v->base+v->bytes && (uintptr_t)v->base<a+bytes)goto done;
    }
    if(!slot)goto done;
    result=s->backend.place(s->backend.context,p,bytes,token,offset,access);
    if(!result)goto done;
    slot->base=p;slot->bytes=bytes;slot->state=result==1?PUBLISHED:ABANDONED;++s->live;
    if(result!=1) { s->poisoned=1;goto done; }
    out=p;
done:
    leave(s);return out;
}
int tsfp_owned_arena_release(tsfp_owned_arena *s,void *p) {
    int ok=0;
    if(!s || !p)return 0;
    enter(s);
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_arena_view *v=s->views+i;
        if(v->base!=p || v->state!=PUBLISHED)continue;
        int result=s->backend.restore(s->backend.context,p,v->bytes);
        if(result==1) { memset(v,0,sizeof(*v));--s->live;ok=1; }
        else if(result<0)s->poisoned=1;
        break;
    }
    leave(s);return ok;
}
int tsfp_owned_arena_stop(tsfp_owned_arena *s) {
    int ok=0;
    if(!s)return 0;
    enter(s);s->active=0;
    for(size_t i=0;i<s->capacity;++i)
        if(s->views[i].state==PUBLISHED)goto done;
    if(!s->base) { ok=1;goto done; }
    if(!s->backend.release(s->backend.context,s->base,s->bytes))goto done;
    memset(s->views,0,s->capacity*sizeof(*s->views));
    s->base=NULL;s->bytes=0;s->live=0;s->poisoned=0;ok=1;
done:
    leave(s);return ok;
}
