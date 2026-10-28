#include "native_reservations.h"
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
enum { FRESH=0,COMMITTED=1,DISCARDED=2 };
static void enter(tsfp_native_reservations *s) {
#ifdef _MSC_VER
    while(_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while(__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_native_reservations *s) {
#ifdef _MSC_VER
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int range(uintptr_t p,size_t n) { return p && n && p<=UINTPTR_MAX-n; }
static int access_valid(uint32_t p) {
    return p==1 || p==2 || p==4 || p==8 || p==16 || p==32 || p==64;
}
static int align_range(size_t page,uintptr_t *base,size_t *bytes) {
    size_t offset=*base%page,n=*bytes;
    if(!n || *base>UINTPTR_MAX-n || n>SIZE_MAX-offset)return 0;
    n+=offset;
    if(n>SIZE_MAX-(page-1))return 0;
    n=(n+page-1)&~(page-1);
    *base-=offset;
    if(*base>UINTPTR_MAX-n)return 0;
    *bytes=n;return 1;
}
static tsfp_native_reservation *find(tsfp_native_reservations *s,uintptr_t p,size_t n) {
    if(!range(p,n))return NULL;
    for(size_t i=0;i<s->capacity;++i) {
        tsfp_native_reservation *r=s->records+i;
        uintptr_t base=(uintptr_t)r->base;
        if(base && p>=base && p-base<r->bytes && n<=r->bytes-(p-base))return r;
    }
    return NULL;
}
static int release_record(tsfp_native_reservations *s,tsfp_native_reservation *r) {
    if(!s->backend.release(s->backend.context,r->base,r->bytes))return 0;
    free(r->pages);memset(r,0,sizeof(*r));--s->live;return 1;
}
int tsfp_native_reservations_init(tsfp_native_reservations *s,
    const tsfp_reservation_backend *b,size_t page,tsfp_native_reservation *records,size_t capacity) {
    uintptr_t begin=(uintptr_t)records,owner=(uintptr_t)s;
    size_t n;
    tsfp_reservation_backend copy;
    if(!s || !b || !b->reserve || !b->release || !b->commit || !b->decommit || !b->query ||
       !page || (page&(page-1)) || !capacity || capacity>SIZE_MAX/sizeof(*records))return 0;
    n=capacity*sizeof(*records);
    if(!range(begin,n) || !range(owner,sizeof(*s)) ||
       (owner<begin+n && begin<owner+sizeof(*s)) || s->active || s->live)return 0;
    copy=*b;memset(s,0,sizeof(*s));memset(records,0,n);
    s->backend=copy;s->records=records;s->capacity=capacity;s->page_size=page;s->active=1;
    return 1;
}
static int commit_range(tsfp_native_reservations *s,tsfp_native_reservation *r,
    uintptr_t base,size_t bytes,uint32_t protection) {
    size_t first=(base-(uintptr_t)r->base)/s->page_size,end=first+bytes/s->page_size;
    for(size_t i=first;i<end;) {
        size_t next=i+1;
        unsigned char state=r->pages[i];
        int result;
        while(next<end && r->pages[next]==state)++next;
        result=s->backend.commit(s->backend.context,
            (unsigned char *)r->base+i*s->page_size,(next-i)*s->page_size,
            protection,state==DISCARDED);
        if(result!=1) { if(result<0)r->quarantined=1;return 0; }
        memset(r->pages+i,COMMITTED,next-i);i=next;
    }
    return 1;
}
void *tsfp_native_reserve(tsfp_native_reservations *s,void *hint,size_t bytes,
    uint32_t type,uint32_t protection) {
    uintptr_t base=(uintptr_t)hint;
    tsfp_native_reservation *r=NULL;
    unsigned char *pages=NULL;
    void *result=NULL;
    int creating;
    if(!s || (type!=0x1000 && type!=0x2000 && type!=0x3000) || !access_valid(protection))return NULL;
    enter(s);
    if(!s->active || !align_range(s->page_size,&base,&bytes))goto done;
    creating=(type&0x2000)!=0 || !hint;
    if(creating) {
        if(s->live==s->capacity)goto done;
        for(size_t i=0;i<s->capacity;++i)if(!s->records[i].base) { r=s->records+i;break; }
        if(!r)goto done;
        pages=(unsigned char *)calloc(bytes/s->page_size,1);
        if(!pages)goto done;
        r->base=s->backend.reserve(s->backend.context,(void *)base,bytes);
        if(!r->base) { free(pages);goto done; }
        r->bytes=bytes;r->pages=pages;r->allocation_protection=protection;++s->live;
        if(!range((uintptr_t)r->base,bytes) || (uintptr_t)r->base%s->page_size ||
           (hint && (uintptr_t)r->base!=base)) {
            r->quarantined=1;(void)release_record(s,r);goto done;
        }
        base=(uintptr_t)r->base;
    } else {
        r=find(s,base,bytes);
        if(!r || r->quarantined)goto done;
    }
    if((type&0x1000) && !commit_range(s,r,base,bytes,protection)) {
        if(creating) { r->quarantined=1;(void)release_record(s,r); }
        goto done;
    }
    result=(void *)base;
done:
    leave(s);return result;
}
int tsfp_native_reserve_free(tsfp_native_reservations *s,void *address,size_t bytes,uint32_t type) {
    uintptr_t base=(uintptr_t)address;
    tsfp_native_reservation *r;
    int result=0;
    if(!s || !address || (type!=0x4000 && type!=0x8000))return 0;
    enter(s);
    if(!s->active)goto done;
    r=find(s,base,1);
    if(!r)goto done;
    if(type==0x8000) {
        if(base==(uintptr_t)r->base && !bytes)result=release_record(s,r);
        goto done;
    }
    if(r->quarantined)goto done;
    if(!bytes) {
        if(base!=(uintptr_t)r->base)goto done;
        bytes=r->bytes;
    }
    if(!align_range(s->page_size,&base,&bytes) || find(s,base,bytes)!=r)goto done;
    {
        int changed=s->backend.decommit(s->backend.context,(void *)base,bytes);
        if(changed!=1) { if(changed<0)r->quarantined=1;goto done; }
    }
    memset(r->pages+(base-(uintptr_t)r->base)/s->page_size,DISCARDED,bytes/s->page_size);
    result=1;
done:
    leave(s);return result;
}
int tsfp_native_reserve_query(tsfp_native_reservations *s,const void *address,tsfp_native_vm_region *out) {
    uintptr_t p=(uintptr_t)address,begin,end;
    tsfp_native_reservation *r;
    tsfp_host_page_region host;
    tsfp_native_vm_region result={0};
    int ok=0;
    if(!s || !p || !out)return 0;
    enter(s);
    if(!s->active)goto done;
    r=find(s,p,1);
    if(r && r->quarantined)goto done;
    if(!s->backend.query(s->backend.context,address,&host) || !range(host.base,host.bytes) ||
       p<host.base || p-host.base>=host.bytes)goto done;
    begin=host.base;end=begin+host.bytes;
    result.state=0x1000;result.protection=host.protection;
    if(r) {
        size_t index=(p-(uintptr_t)r->base)/s->page_size,first=index,last=index+1;
        int committed=r->pages[index]==COMMITTED;
        while(first && (r->pages[first-1]==COMMITTED)==committed)--first;
        while(last<r->bytes/s->page_size && (r->pages[last]==COMMITTED)==committed)++last;
        if(begin<(uintptr_t)r->base+first*s->page_size)begin=(uintptr_t)r->base+first*s->page_size;
        if(end>(uintptr_t)r->base+last*s->page_size)end=(uintptr_t)r->base+last*s->page_size;
        result.allocation_base=(uintptr_t)r->base;
        result.allocation_protection=r->allocation_protection;
        result.state=committed?0x1000:0x2000;
        if(!committed)result.protection=0;
        result.type=0x20000;
    }
    result.base=begin;result.bytes=end-begin;*out=result;ok=1;
done:
    leave(s);return ok;
}
int tsfp_native_reserve_protect(tsfp_native_reservations *s,void *address,size_t bytes,uint32_t protection) {
    uintptr_t base=(uintptr_t)address;
    tsfp_native_reservation *r;
    int changed,result=0;
    if(!s || !address || !access_valid(protection))return 0;
    enter(s);
    if(!s->active || !align_range(s->page_size,&base,&bytes))goto done;
    r=find(s,base,bytes);
    if(r) {
        size_t first=(base-(uintptr_t)r->base)/s->page_size,last=first+bytes/s->page_size;
        if(r->quarantined)goto done;
        for(size_t i=first;i<last;++i)if(r->pages[i]!=COMMITTED)goto done;
    } else {
        for(size_t i=0;i<s->capacity;++i) {
            const tsfp_native_reservation *other=s->records+i;
            if(other->base && base<(uintptr_t)other->base+other->bytes &&
               (uintptr_t)other->base<base+bytes)goto done;
        }
    }
    changed=s->backend.commit(s->backend.context,(void *)base,bytes,protection,0);
    if(changed<0 && r)r->quarantined=1;
    result=changed==1;
done:
    leave(s);return result;
}
int tsfp_native_reservations_stop(tsfp_native_reservations *s) {
    int result;
    if(!s)return 0;
    enter(s);s->active=0;
    for(size_t i=0;i<s->capacity;++i)if(s->records[i].base)(void)release_record(s,s->records+i);
    result=!s->live;leave(s);return result;
}
