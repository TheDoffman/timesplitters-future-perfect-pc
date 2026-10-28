#include "mmio_views.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
typedef struct model { unsigned opened,closed,maps,unmaps,fail_on,fail_unmap;uintptr_t next;size_t last_bytes; } model;
static int open_backing(void *p,size_t bytes,uintptr_t *token) { model *m=p;m->last_bytes=bytes;*token=++m->opened;return 1; }
static void close_backing(void *p,uintptr_t token) { model *m=p;CHECK(token);++m->closed; }
static void *map_view(void *p,uintptr_t token,size_t offset,size_t bytes,void *hint,unsigned access) {
    model *m=p;CHECK(token && !offset && bytes && access==TSFP_VIEW_WRITE);++m->maps;
    if(m->maps==m->fail_on)return NULL;
    m->next+=0x100000;return hint?hint:(void *)m->next;
}
static int unmap_view(void *p,void *base,size_t bytes) { model *m=p;CHECK(base && bytes);++m->unmaps;return !m->fail_unmap; }
static void controlled(size_t page) {
    model m={0};tsfp_mmio_views s={0};tsfp_mmio_view records[2];
    tsfp_shared_backend b={&m,open_backing,close_backing,map_view,unmap_view};
    size_t bytes=page+7;
    unsigned calls;
    void *guest,*alias;
    CHECK(tsfp_mmio_views_init(&s,&b,page,page,records,2));
    CHECK(!tsfp_mmio_views_init(&s,&b,page,page,records,2));
    CHECK(!tsfp_mmio_views_map(&s,NULL,0));CHECK(!m.opened);
    CHECK(!tsfp_mmio_views_map(&s,NULL,SIZE_MAX));CHECK(!m.opened);
    guest=tsfp_mmio_views_map(&s,NULL,bytes);CHECK(guest && m.last_bytes==page*2);
    alias=records[0].alias;CHECK(alias && alias!=guest);
    calls=m.maps;
    CHECK(tsfp_mmio_views_alias(&s,guest,bytes)==alias);
    CHECK(tsfp_mmio_views_alias(&s,(void *)((uintptr_t)guest+bytes-1),1)==(void *)((uintptr_t)alias+bytes-1));
    CHECK(!tsfp_mmio_views_alias(&s,(void *)((uintptr_t)guest+bytes-1),2));
    CHECK(!tsfp_mmio_views_alias(&s,(void *)((uintptr_t)guest+bytes),1));
    CHECK(!tsfp_mmio_views_alias(&s,(void *)((uintptr_t)guest-1),1));
    CHECK(!tsfp_mmio_views_alias(&s,(void *)UINTPTR_MAX,2));
    CHECK(!tsfp_mmio_views_alias(&s,guest,0));CHECK(m.maps==calls);
    CHECK(!tsfp_mmio_views_map(&s,guest,page));CHECK(m.maps==calls);
    m.fail_unmap=1;CHECK(!tsfp_mmio_views_reset(&s));CHECK(!m.closed);
    CHECK(!tsfp_mmio_views_alias(&s,guest,1));
    CHECK(!tsfp_mmio_views_map(&s,NULL,page));CHECK(m.maps==calls);
    CHECK(!tsfp_mmio_views_init(&s,&b,page,page,records,2));
    m.fail_unmap=0;CHECK(tsfp_mmio_views_reset(&s));CHECK(m.closed==1);
    CHECK(tsfp_mmio_views_reset(&s));CHECK(m.closed==1);
    CHECK(tsfp_mmio_views_init(&s,&b,page,page,records,2));
    m.fail_on=m.maps+2;m.fail_unmap=1;
    CHECK(!tsfp_mmio_views_map(&s,NULL,bytes));
    guest=records[0].guest;CHECK(guest && records[0].store.live==1 && records[0].store.backing);
    CHECK(!records[0].visible && !tsfp_mmio_views_alias(&s,(void *)((uintptr_t)guest+4),1));
    m.fail_on=0;
    CHECK(tsfp_mmio_views_map(&s,NULL,page));
    calls=m.maps;CHECK(!tsfp_mmio_views_map(&s,NULL,page));CHECK(m.maps==calls);
    CHECK(!tsfp_mmio_views_reset(&s));CHECK(m.closed==1);
    m.fail_unmap=0;CHECK(tsfp_mmio_views_reset(&s));CHECK(m.closed==m.opened);
}
static void real_host(void) {
    tsfp_shared_backend b;size_t page,gran;
    tsfp_mmio_views s={0};tsfp_mmio_view records[2];
    volatile uint32_t *guest,*alias;
    CHECK(tsfp_shared_host_backend(&b,&page,&gran));
    CHECK(tsfp_mmio_views_init(&s,&b,page,gran,records,2));
    guest=tsfp_mmio_views_map(&s,NULL,page*2);CHECK(guest);
    alias=tsfp_mmio_views_alias(&s,(const void *)guest,4);CHECK(alias && alias!=guest);
    guest[0]=0x8badf00d;CHECK(alias[0]==0x8badf00d);
#if defined(_WIN32)
    DWORD old;
    CHECK(VirtualProtect((void *)guest,page*2,PAGE_READONLY,&old));
#else
    CHECK(!mprotect((void *)guest,page*2,PROT_READ));
#endif
    alias[0]=0x12345678;CHECK(guest[0]==0x12345678);
    alias[page/sizeof(uint32_t)]=0xdeadbeef;CHECK(guest[page/sizeof(uint32_t)]==0xdeadbeef);
    CHECK(tsfp_mmio_views_reset(&s));CHECK(!tsfp_mmio_views_alias(&s,(const void *)guest,4));
    CHECK(tsfp_mmio_views_init(&s,&b,page,gran,records,2));
    guest=tsfp_mmio_views_map(&s,NULL,page);CHECK(guest && !guest[0]);
    CHECK(tsfp_mmio_views_reset(&s));
}
int main(void) { controlled(4096);controlled(16384);real_host();puts("PASS: MMIO immutable lookup, protected aliases and retryable construction/reset");return 0; }
