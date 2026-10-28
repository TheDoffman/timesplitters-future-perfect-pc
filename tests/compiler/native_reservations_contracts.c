#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "native_reservations.h"
static int fail_metadata;
static void *checked_calloc(size_t n,size_t bytes) {
    return fail_metadata?NULL:calloc(n,bytes);
}
#define calloc checked_calloc
#include "native_reservations.c"
#undef calloc

static unsigned char *arena,*storage;
static size_t page;
static struct { int used;size_t bytes;uint32_t access[8]; } maps[4];
static unsigned reserve_calls,release_calls,commit_calls,decommit_calls;
static int commit_failure,decommit_failure,release_failure;
static unsigned fail_commit_at;
static tsfp_native_reservations owner;
static tsfp_native_reservation records[3];
static int map_index(const void *p) {
    uintptr_t address=(uintptr_t)p,base=(uintptr_t)arena;
    if(address<base || address-base>=32*page)return -1;
    return (int)((address-base)/(8*page));
}
static void *map_base(int i) { return arena+(size_t)i*8*page; }
static void *reserve(void *context,void *hint,size_t bytes) {
    int choice=map_index(hint);
    assert(context==&owner && bytes && bytes<=8*page && !(bytes%page));
    ++reserve_calls;
    if(choice<0 || hint!=map_base(choice) || maps[choice].used) {
        choice=0;while(choice<4 && maps[choice].used)++choice;
    }
    if(choice==4)return NULL;
    maps[choice].used=1;maps[choice].bytes=bytes;
    memset(maps[choice].access,0,sizeof(maps[choice].access));
    memset(map_base(choice),0,bytes);return map_base(choice);
}
static int release(void *context,void *base,size_t bytes) {
    int i=map_index(base);
    assert(context==&owner && i>=0 && maps[i].used && base==map_base(i) && bytes==maps[i].bytes);
    ++release_calls;
    if(release_failure)return 0;
    maps[i].used=0;return 1;
}
static int commit(void *context,void *base,size_t bytes,uint32_t protection,int zero) {
    int i=map_index(base);
    size_t offset=(unsigned char *)base-(unsigned char *)map_base(i);
    assert(context==&owner && i>=0 && maps[i].used && !(offset%page) && !(bytes%page));
    assert(offset+bytes<=maps[i].bytes);
    ++commit_calls;
    if(commit_failure && (!fail_commit_at || commit_calls==fail_commit_at))return commit_failure==2?0:commit_failure;
    if(zero)memset(base,0,bytes);
    for(size_t j=offset/page;j<(offset+bytes)/page;++j)maps[i].access[j]=protection;
    return 1;
}
static int decommit(void *context,void *base,size_t bytes) {
    int i=map_index(base);
    size_t offset=(unsigned char *)base-(unsigned char *)map_base(i);
    assert(context==&owner && i>=0 && maps[i].used && !(offset%page) && !(bytes%page));
    assert(offset+bytes<=maps[i].bytes);++decommit_calls;
    if(decommit_failure)return decommit_failure==2?0:decommit_failure;
    for(size_t j=offset/page;j<(offset+bytes)/page;++j)maps[i].access[j]=1;
    return 1;
}
static int query(void *context,const void *address,tsfp_host_page_region *out) {
    int i=map_index(address);
    size_t index,first,last;
    assert(context==&owner);
    if(i<0 || !maps[i].used)return 0;
    index=((const unsigned char *)address-(unsigned char *)map_base(i))/page;
    if(index>=maps[i].bytes/page)return 0;
    first=index;last=index+1;
    while(first && maps[i].access[first-1]==maps[i].access[index])--first;
    while(last<maps[i].bytes/page && maps[i].access[last]==maps[i].access[index])++last;
    out->base=(uintptr_t)map_base(i)+first*page;out->bytes=(last-first)*page;
    out->protection=maps[i].access[index]?maps[i].access[index]:1;return 1;
}
static tsfp_native_vm_region inspect(void *p,uint32_t state,size_t bytes) {
    tsfp_native_vm_region result;
    assert(tsfp_native_reserve_query(&owner,p,&result));
    assert(result.state==state && result.bytes==bytes);
    return result;
}
static void initialize(size_t size) {
    tsfp_reservation_backend backend={&owner,reserve,release,commit,decommit,query};
    page=size;storage=(unsigned char *)malloc(33*page);assert(storage);
    arena=(unsigned char *)(((uintptr_t)storage+page-1)&~(page-1));
    memset(&owner,0,sizeof(owner));memset(maps,0,sizeof(maps));
    reserve_calls=release_calls=commit_calls=decommit_calls=0;
    commit_failure=decommit_failure=release_failure=fail_metadata=0;fail_commit_at=0;
    assert(!tsfp_native_reservations_init(&owner,&backend,3,records,3));
    assert(tsfp_native_reservations_init(&owner,&backend,page,records,3));
    assert(!tsfp_native_reservations_init(&owner,&backend,page,records,3));
}
static void states_and_ranges(void) {
    unsigned char *p=tsfp_native_reserve(&owner,NULL,4*page,0x2000,4);
    tsfp_native_vm_region info,before;
    unsigned count;
    assert(p && owner.live==1 && commit_calls==0);
    assert(!tsfp_native_reserve_protect(&owner,p,1,4) && commit_calls==0);
    info=inspect(p,0x2000,4*page);
    assert(info.base==(uintptr_t)p && info.allocation_base==(uintptr_t)p &&
        info.allocation_protection==4 && !info.protection && info.type==0x20000);
    assert(tsfp_native_reserve(&owner,p+page+3,page-3,0x1000,4)==p+page);
    info=inspect(p+page+7,0x1000,page);assert(info.protection==4);
    assert(tsfp_native_reserve_protect(&owner,p+page+1,1,2));
    info=inspect(p+page+7,0x1000,page);assert(info.protection==2);
    count=commit_calls;
    assert(!tsfp_native_reserve_protect(&owner,p+3*page,2*page,4) && commit_calls==count);
    memset(p+page,0x7b,page);
    assert(tsfp_native_reserve(&owner,p+page,page,0x1000,2)==p+page);
    assert(p[page]==0x7b && p[2*page-1]==0x7b); /* recommit must not erase live data */
    assert(tsfp_native_reserve_free(&owner,p+page+1,page-1,0x4000));
    info=inspect(p+page,0x2000,page);assert(!info.protection);
    assert(tsfp_native_reserve(&owner,p+page,page,0x1000,4)==p+page);
    for(size_t j=page;j<2*page;++j)assert(!p[j]);
    p[page]=93;
    assert(tsfp_native_reserve(&owner,p,4*page,0x1000,4)==p && p[page]==93);
    info=inspect(p,0x1000,4*page);assert(info.allocation_base==(uintptr_t)p);
    count=decommit_calls;
    assert(!tsfp_native_reserve_free(&owner,p+1,0,0x4000) && decommit_calls==count);
    assert(tsfp_native_reserve_free(&owner,p,0,0x4000));
    (void)inspect(p,0x2000,4*page);
    assert(tsfp_native_reserve(&owner,p,4*page,0x1000,4)==p);
    for(size_t j=0;j<4*page;++j)assert(!p[j]);
    assert(!tsfp_native_reserve_free(&owner,p+page,0,0x8000));
    assert(!tsfp_native_reserve_free(&owner,p,1,0x8000));
    assert(!tsfp_native_reserve_free(&owner,p,0,0xc000));
    assert(!tsfp_native_reserve(&owner,p+3*page,2*page,0x1000,4));
    count=reserve_calls;
    assert(!tsfp_native_reserve(&owner,NULL,SIZE_MAX,0x2000,4));
    assert(!tsfp_native_reserve(&owner,(void *)(UINTPTR_MAX-2),4,0x2000,4));
    assert(!tsfp_native_reserve(&owner,NULL,0,0x2000,4));
    assert(!tsfp_native_reserve(&owner,NULL,page,0x5000,4));
    assert(!tsfp_native_reserve(&owner,NULL,page,0x2000,99));
    assert(reserve_calls==count);
    before=info;assert(!tsfp_native_reserve_query(&owner,(void *)UINTPTR_MAX,&info));
    assert(!memcmp(&before,&info,sizeof(info)));
    assert(tsfp_native_reserve_free(&owner,p,0,0x8000));
    assert(!owner.live && !tsfp_native_reserve_free(&owner,p,0,0x8000));
}
static void partial_failures(void) {
    unsigned char *p=tsfp_native_reserve(&owner,NULL,4*page,0x2000,4);
    assert(p && tsfp_native_reserve(&owner,p+page,page,0x1000,4)==p+page);
    p[page]=73;
    /* First run commits; the second host callback fails without changes.
     * Earlier successful subspans remain honestly recorded and queryable. */
    commit_failure=2;fail_commit_at=commit_calls+2;
    assert(!tsfp_native_reserve(&owner,p,4*page,0x1000,4));
    (void)inspect(p,0x1000,2*page);(void)inspect(p+2*page,0x2000,2*page);
    assert(p[page]==73);
    commit_failure=0;fail_commit_at=0;decommit_failure=2;
    assert(!tsfp_native_reserve_free(&owner,p,2*page,0x4000));
    (void)inspect(p,0x1000,2*page);decommit_failure=0;
    assert(tsfp_native_reserve_free(&owner,p,0,0x8000));
}
static void failures_and_ownership(void) {
    unsigned char *p,*q,*r;
    tsfp_native_vm_region info;
    unsigned count;
    fail_metadata=1;count=reserve_calls;
    assert(!tsfp_native_reserve(&owner,NULL,page,0x2000,4) && reserve_calls==count);
    fail_metadata=0;
    p=tsfp_native_reserve(&owner,NULL,2*page,0x3000,4);assert(p);p[0]=42;
    assert(!tsfp_native_reserve(&owner,p,2*page,0x3000,4) && p[0]==42 && owner.live==1);
    release_failure=1;
    assert(!tsfp_native_reserve_free(&owner,p,0,0x8000) && owner.live==1);
    release_failure=0;
    decommit_failure=-1;
    assert(!tsfp_native_reserve_free(&owner,p,page,0x4000));
    assert(!tsfp_native_reserve_query(&owner,p,&info));
    assert(!tsfp_native_reserve(&owner,p,page,0x1000,4));
    assert(tsfp_native_reserve_free(&owner,p,0,0x8000));decommit_failure=0;
    commit_failure=-1;release_failure=1;
    assert(!tsfp_native_reserve(&owner,NULL,page,0x3000,4) && owner.live==1);
    assert(!tsfp_native_reservations_stop(&owner) && owner.live==1 && !owner.active);
    release_failure=0;assert(tsfp_native_reservations_stop(&owner) && !owner.live);
    {
        tsfp_reservation_backend b=owner.backend;
        assert(tsfp_native_reservations_init(&owner,&b,page,records,3));
    }
    commit_failure=0;
    p=tsfp_native_reserve(&owner,NULL,page,0x1000,4);assert(p);
    q=tsfp_native_reserve(&owner,NULL,page,0x2000,2);assert(q);
    r=tsfp_native_reserve(&owner,NULL,page,0x2000,1);assert(r);
    count=reserve_calls;
    assert(!tsfp_native_reserve(&owner,NULL,page,0x2000,4) && reserve_calls==count);
    assert(tsfp_native_reserve_free(&owner,p,0,0x8000));
    p=reserve(&owner,NULL,page); /* mapped but foreign to reservation owner */
    assert(tsfp_native_reserve_protect(&owner,p,1,2));
    assert(tsfp_native_reserve_query(&owner,p,&info) && !info.allocation_base && !info.type);
    count=release_calls;
    assert(!tsfp_native_reserve_free(&owner,p,0,0x8000) && release_calls==count);
    assert(release(&owner,p,page));
    assert(tsfp_native_reservations_stop(&owner));
}
#ifdef _WIN32
static DWORD WINAPI worker(void *arg)
#else
static void *worker(void *arg)
#endif
{
    unsigned char value=(unsigned char)(uintptr_t)arg;
    for(unsigned i=0;i<300;++i) {
        unsigned char *p=tsfp_native_reserve(&owner,NULL,page,0x3000,4);
        if(!p)continue;
        p[0]=value;p[page-1]=value;
        (void)inspect(p,0x1000,page);
        assert(p[0]==value && p[page-1]==value);
        assert(tsfp_native_reserve_free(&owner,p,0,0x8000));
    }
    return 0;
}
static void concurrency(void) {
    tsfp_reservation_backend b=owner.backend;
    assert(tsfp_native_reservations_init(&owner,&b,page,records,3));
#ifdef _WIN32
    HANDLE threads[4];
    for(uintptr_t i=0;i<4;++i) { threads[i]=CreateThread(NULL,0,worker,(void *)(i+1),0,NULL);assert(threads[i]); }
    assert(WaitForMultipleObjects(4,threads,TRUE,30000)==WAIT_OBJECT_0);
    for(unsigned i=0;i<4;++i)assert(CloseHandle(threads[i]));
#else
    pthread_t threads[4];
    for(uintptr_t i=0;i<4;++i)assert(!pthread_create(threads+i,NULL,worker,(void *)(i+1)));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
#endif
    assert(!owner.live && tsfp_native_reservations_stop(&owner));
}
int main(void) {
    for(unsigned n=0;n<2;++n) {
        initialize(n?16384:4096);states_and_ranges();partial_failures();failures_and_ownership();concurrency();free(storage);
    }
    puts("native reservation ownership and page-state contracts passed");return 0;
}
