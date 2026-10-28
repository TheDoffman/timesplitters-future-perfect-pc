/* Exercise the actual non-Windows host adapter with controlled OS calls. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Keep controlled APIs distinct from real libc/pthread symbols on native Mac. */
#define pthread_once_t tsfp_model_once_t
#define pthread_once tsfp_model_once
#define mmap tsfp_model_mmap
#define munmap tsfp_model_munmap
#define mprotect tsfp_model_mprotect
#define madvise tsfp_model_madvise
#define tsfp_host_page_size tsfp_model_page_size
#define tsfp_host_pages_query tsfp_model_page_query
#include "native_vm_host.h"
#undef _WIN32
#include "native_vm_host.c"
static unsigned char *allocation,*arena;
static const size_t model_page=16384;
static struct { int live;size_t bytes;uint32_t protection[4]; } mapping[8];
static unsigned protects,unmaps,advices,once_calls;
static unsigned fail_protect_at;
static int fail_protect_sticky,fail_unmap,fail_map;
static int index_of(const void *address) {
    uintptr_t p=(uintptr_t)address,first=(uintptr_t)arena;
    return p>=first && p-first<32*model_page?(int)((p-first)/(4*model_page)):-1;
}
static void *base_of(int i) { return arena+(size_t)i*4*model_page; }
size_t tsfp_host_page_size(void) { return model_page; }
int pthread_once(pthread_once_t *flag,void (*fn)(void)) {
    if(!*flag) { ++once_calls;fn();*flag=1; }return 0;
}
void *mmap(void *hint,size_t bytes,int protection,int flags,int fd,long long offset) {
    int i=index_of(hint);
    assert(protection==PROT_NONE && flags==(MAP_PRIVATE|MAP_ANON) && fd==-1 && !offset);
    assert(bytes && bytes<=4*model_page && !(bytes%model_page));
    if(fail_map)return MAP_FAILED;
    if(i<0 || hint!=base_of(i) || mapping[i].live) { i=0;while(i<8 && mapping[i].live)++i; }
    if(i==8)return MAP_FAILED;
    mapping[i].live=1;mapping[i].bytes=bytes;
    memset(mapping[i].protection,0,sizeof(mapping[i].protection));
    memset(base_of(i),0,bytes);return base_of(i);
}
int munmap(void *base,size_t bytes) {
    int i=index_of(base);
    assert(i>=0 && base==base_of(i) && mapping[i].live && bytes==mapping[i].bytes);
    ++unmaps;if(fail_unmap)return -1;
    mapping[i].live=0;return 0;
}
int mprotect(void *base,size_t bytes,int protection) {
    int i=index_of(base);
    size_t offset=(unsigned char *)base-(unsigned char *)base_of(i);
    assert(i>=0 && mapping[i].live && !(offset%model_page) && !(bytes%model_page));
    assert(offset+bytes<=mapping[i].bytes);++protects;
    if(fail_protect_at && (protects==fail_protect_at || (fail_protect_sticky && protects>=fail_protect_at)))return -1;
    for(size_t j=offset/model_page;j<(offset+bytes)/model_page;++j)
        mapping[i].protection[j]=(uint32_t)protection;
    return 0;
}
int madvise(void *base,size_t bytes,int advice) {
    assert(index_of(base)>=0 && bytes && advice==MADV_FREE);++advices;
    /* Advice can fail or retain every byte. Recommit must still clear it. */
    return -1;
}
int tsfp_host_pages_query(const void *address,tsfp_host_page_region *out) {
    int i=index_of(address);
    size_t page,first,last;
    uint32_t p;
    if(i<0 || !mapping[i].live)return 0;
    page=((const unsigned char *)address-(unsigned char *)base_of(i))/model_page;
    if(page>=mapping[i].bytes/model_page)return 0;
    first=page;last=page+1;p=mapping[i].protection[page];
    while(first && mapping[i].protection[first-1]==p)--first;
    while(last<mapping[i].bytes/model_page && mapping[i].protection[last]==p)++last;
    out->base=(uintptr_t)base_of(i)+first*model_page;out->bytes=(last-first)*model_page;
    out->protection=(p&PROT_EXEC)?((p&PROT_WRITE)?64:((p&PROT_READ)?32:16)):
        ((p&PROT_WRITE)?4:((p&PROT_READ)?2:1));return 1;
}
int main(void) {
    unsigned char *p;
    tsfp_native_vm_region info;
    unsigned before;
    allocation=(unsigned char *)malloc(33*model_page);assert(allocation);
    arena=(unsigned char *)(((uintptr_t)allocation+model_page-1)&~(model_page-1));
    fail_map=1;assert(!tsfp_native_vm_allocate(NULL,model_page,0x3000,4));fail_map=0;
    p=tsfp_native_vm_allocate(NULL,3*model_page,0x3000,4);assert(p && once_calls==1);
    memset(p,0x5a,3*model_page);
    assert(tsfp_native_vm_allocate(p+model_page,model_page,0x1000,2)==p+model_page);
    assert(p[model_page]==0x5a); /* normal commit preserves existing bytes */
    before=unmaps;
    assert(!tsfp_native_vm_allocate(p,model_page,0x2000,4) && unmaps==before+1 && p[0]==0x5a);
    assert(tsfp_native_vm_release(p+model_page+3,1,0x4000) && advices==1);
    assert(p[model_page]==0x5a); /* retained physical bytes in controlled OS */
    assert(tsfp_native_vm_query(p+model_page,&info) && info.state==0x2000 && !info.protection);
    before=protects;
    assert(!tsfp_native_vm_protect(p+model_page,1,4) && protects==before);
    fail_protect_at=protects+2; /* zeroing succeeds, requested final READONLY fails */
    assert(!tsfp_native_vm_allocate(p+model_page,model_page,0x1000,2));
    assert(tsfp_native_vm_query(p+model_page,&info) && info.state==0x2000);
    assert(mapping[index_of(p)].protection[1]==PROT_NONE); /* rollback succeeded */
    fail_protect_at=0;
    assert(tsfp_native_vm_allocate(p+model_page,model_page,0x1000,4)==p+model_page);
    for(size_t j=model_page;j<2*model_page;++j)assert(!p[j]);
    assert(p[0]==0x5a && p[2*model_page]==0x5a);
    assert(tsfp_native_vm_release(p+model_page,model_page,0x4000));
    fail_protect_at=protects+2;fail_protect_sticky=1;
    assert(!tsfp_native_vm_allocate(p+model_page,model_page,0x1000,2));
    assert(!tsfp_native_vm_query(p,&info)); /* uncertain access quarantines owner */
    assert(!tsfp_native_vm_allocate(p,model_page,0x1000,4));
    fail_protect_at=0;fail_protect_sticky=0;fail_unmap=1;
    assert(!tsfp_native_vm_release(p,0,0x8000));fail_unmap=0;
    assert(tsfp_native_vm_release(p,0,0x8000));
    assert(!tsfp_native_vm_query(p,&info) && once_calls==1);
    assert(tsfp_native_reservations_stop(&reservations));free(allocation);
    puts("controlled POSIX reservation host contracts passed");return 0;
}
