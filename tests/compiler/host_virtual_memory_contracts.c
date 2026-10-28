#include "host_virtual_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Deliberately native-width fields, including bytes beyond the guest's record. */
typedef struct region {
    uintptr_t base, allocation;
    uint32_t allocation_protection;
    size_t size;
    uint32_t state, protection, kind;
} region;
static tsfp_host_vm vm;
static void *seen_base;
static size_t seen_size;
static uint32_t seen_flags, seen_protect;
static unsigned calls;
static int failure, short_query;
#define WIDE_ADDRESS ((uintptr_t)UINT64_C(0x123456789abcdef0))
#define WIDE_SIZE ((size_t)UINT64_C(0x100004321))
static void *allocate(void *host, void *hint, size_t size, uint32_t type, uint32_t protect) {
    assert(host==&vm); ++calls;
    seen_base=hint; seen_size=size; seen_flags=type; seen_protect=protect;
    return failure?NULL:(void *)WIDE_ADDRESS;
}
static int release(void *host, void *base, size_t size, uint32_t type) {
    assert(host==&vm); ++calls;
    seen_base=base; seen_size=size; seen_flags=type;
    return !failure;
}
static size_t query(void *host, const void *address, void *out, size_t size) {
    region *r=out;
    assert(host==&vm && (uintptr_t)address==WIDE_ADDRESS && size==sizeof(region));
    ++calls;
    /* Check padding is deterministic before the OS adapter populates fields. */
    for(size_t i=0;i<size;++i) assert(((unsigned char *)out)[i]==0);
    r->base=WIDE_ADDRESS; r->allocation=WIDE_ADDRESS-0x1000;
    r->allocation_protection=0x20; r->size=WIDE_SIZE;
    r->state=0x1000; r->protection=0x40; r->kind=0x20000;
    return failure?0:(short_query?size-1:size);
}
static void setup(void) {
    assert(sizeof(void *)==8 && sizeof(size_t)==8);
    vm.host=&vm; vm.allocate=allocate; vm.release=release; vm.query=query;
    calls=0; failure=0; short_query=0;
}
static void allocation_checks(void) {
    void *base=(void *)(uintptr_t)(WIDE_ADDRESS-0x1000);
    size_t bytes=WIDE_SIZE;
    setup();
    assert(tsfp_host_vm_allocate(&vm,&base,33,&bytes,0x3000,0x120)==0);
    assert((uintptr_t)base==WIDE_ADDRESS && bytes==WIDE_SIZE);
    assert((uintptr_t)seen_base==WIDE_ADDRESS-0x1000 && seen_size==WIDE_SIZE);
    assert(seen_flags==0x3000 && seen_protect==0x20 && calls==1);
    failure=1;
    assert(tsfp_host_vm_allocate(&vm,&base,0,&bytes,0x1000,255)==0xc0000017u);
    assert((uintptr_t)base==WIDE_ADDRESS && bytes==WIDE_SIZE && seen_protect==4);
    assert(tsfp_host_vm_allocate(NULL,&base,0,&bytes,0,4)==0xc000000du);
    assert(tsfp_host_vm_allocate(&vm,NULL,0,&bytes,0,4)==0xc000000du);
    assert(tsfp_host_vm_allocate(&vm,&base,0,NULL,0,4)==0xc000000du && calls==2);
    for(uint32_t i=0;i<65536;++i) {
        uint32_t b=i & 255u;
        uint32_t expected=b==1||b==2||b==4||b==8||b==16||b==32||b==64?b:4;
        assert(tsfp_host_vm_protection(i)==expected);
    }
}
static void release_checks(void) {
    void *base=(void *)WIDE_ADDRESS;
    size_t bytes=WIDE_SIZE;
    setup(); failure=1;
    assert(tsfp_host_vm_free(&vm,&base,&bytes,0x8000)==0xc0000001u);
    assert((uintptr_t)base==WIDE_ADDRESS && bytes==WIDE_SIZE && seen_size==0);
    failure=0;
    assert(tsfp_host_vm_free(&vm,&base,&bytes,0x4000)==0);
    assert((uintptr_t)base==WIDE_ADDRESS && seen_size==WIDE_SIZE && bytes==WIDE_SIZE);
    assert(tsfp_host_vm_free(&vm,&base,NULL,0x4000)==0 && seen_size==0);
    assert(tsfp_host_vm_free(&vm,&base,&bytes,0x8000)==0);
    assert(!base && bytes==WIDE_SIZE && calls==4 && seen_flags==0x8000);
    assert(tsfp_host_vm_free(&vm,&base,&bytes,0x8000)==0xc000000du && calls==4);
}
static void query_checks(void) {
    region expected, scratch;
    unsigned char output[sizeof(region)+16], snapshot[sizeof(output)];
    uint32_t length;
    setup(); memset(&expected,0,sizeof(expected));
    assert(query(&vm,(void *)WIDE_ADDRESS,&expected,sizeof(expected))==sizeof(expected));
    for(uint32_t n=0;n<=sizeof(output);++n) {
        size_t copied=n<sizeof(region)?n:sizeof(region);
        memset(output,0xa5,sizeof(output)); memset(&scratch,0xcc,sizeof(scratch)); length=99;
        assert(tsfp_host_vm_query(&vm,(void *)WIDE_ADDRESS,output,n,&length,&scratch,sizeof(scratch))==0);
        assert(length==copied && !memcmp(output,&expected,copied));
        for(size_t i=copied;i<sizeof(output);++i) assert(output[i]==0xa5);
    }
    failure=1; length=99; memcpy(snapshot,output,sizeof(output));
    assert(tsfp_host_vm_query(&vm,(void *)WIDE_ADDRESS,output,sizeof(output),&length,&scratch,sizeof(scratch))==0xc000000du);
    assert(length==99 && !memcmp(snapshot,output,sizeof(output)));
    failure=0; short_query=1;
    assert(tsfp_host_vm_query(&vm,(void *)WIDE_ADDRESS,output,sizeof(output),&length,&scratch,sizeof(scratch))==0xc000000du);
    assert(length==99 && !memcmp(snapshot,output,sizeof(output)));
    short_query=0;
    assert(tsfp_host_vm_query(&vm,(void *)WIDE_ADDRESS,NULL,0,&length,&scratch,sizeof(scratch))==0 && length==0);
    assert(tsfp_host_vm_query(&vm,(void *)WIDE_ADDRESS,NULL,1,&length,&scratch,sizeof(scratch))==0xc000000du);
    assert(tsfp_host_vm_query(&vm,(void *)WIDE_ADDRESS,&scratch,1,&length,&scratch,sizeof(scratch))==0xc000000du);
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"allocate")) allocation_checks();
    else if(!strcmp(argv[1],"free")) release_checks();
    else if(!strcmp(argv[1],"query")) query_checks();
    else abort();
    puts("native virtual memory contracts passed"); return 0;
}
