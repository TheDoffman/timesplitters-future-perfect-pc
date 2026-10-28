#include "guest_map_layout.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
typedef struct region { uintptr_t base,token;size_t bytes;unsigned role,private_map; } region;
typedef struct model {
    region maps[96];unsigned opened,closed,begins,finishes,map_calls,unmap_calls;
    unsigned fail_open,fail_map,private_fail_mask,private_calls[5],private_releases[5];
    uintptr_t fail_release;
    size_t page;unsigned fixed,unhealthy,fail_finish,fail_begin;
} model;
static region *find(model *m,const void *p) {
    for(unsigned i=0;i<96;++i)if(m->maps[i].base==(uintptr_t)p && p)return m->maps+i;
    return NULL;
}
static unsigned live(model *m) { unsigned n=0;for(unsigned i=0;i<96;++i)n+=m->maps[i].base!=0;return n; }
static void *place(model *m,void *hint,size_t bytes,uintptr_t token,unsigned role,unsigned priv) {
    uintptr_t a=(uintptr_t)hint;region *slot=NULL;
    ++m->map_calls;
    if(m->map_calls==m->fail_map)return NULL;
    if(!a)a=(uintptr_t)UINT64_C(0x200000000);
    for(unsigned i=0;i<96;++i) {
        region *v=m->maps+i;
        if(!v->base) { if(!slot)slot=v;continue; }
        if(a<v->base+v->bytes && v->base<a+bytes)return NULL;
    }
    CHECK(slot);*slot=(region){a,token,bytes,role,priv};return (void *)a;
}
static int open_backing(void *ctx,size_t bytes,uintptr_t *out) {
    model *m=ctx;CHECK(bytes);++m->opened;
    if(m->opened==m->fail_open)return 0;
    *out=m->opened;return 1;
}
static void close_backing(void *ctx,uintptr_t token) {
    model *m=ctx;CHECK(token);
    for(unsigned i=0;i<96;++i)CHECK(!m->maps[i].base || m->maps[i].token!=token || m->maps[i].private_map);
    ++m->closed;
}
static void *map_backing(void *ctx,uintptr_t token,size_t offset,size_t bytes,void *hint,unsigned access) {
    CHECK(!offset && access==TSFP_VIEW_WRITE);return place(ctx,hint,bytes,token,0,0);
}
static int unmap_backing(void *ctx,void *p,size_t bytes) {
    model *m=ctx;region *v=find(m,p);CHECK(v && !v->private_map && v->bytes==bytes);++m->unmap_calls;
    if(m->fail_release==(uintptr_t)p)return 0;
    memset(v,0,sizeof(*v));return 1;
}
static int begin(void *ctx,tsfp_shared_backend *b,void **fixed,size_t *page,size_t *gran) {
    model *m=ctx;++m->begins;
    if(m->fail_begin)return 0;
    *b=(tsfp_shared_backend){m,open_backing,close_backing,map_backing,unmap_backing};
    *fixed=m->fixed?(void *)(uintptr_t)UINT64_C(0x200000000):NULL;
    *page=m->page;*gran=m->fixed?m->page:65536;return 1;
}
static void *map_private(void *ctx,void *p,size_t bytes,unsigned role) {
    model *m=ctx;CHECK(role<5);++m->private_calls[role];
    if(m->private_fail_mask&(1u<<role))return NULL;
    return place(m,p,bytes,0,role,1);
}
static int unmap_private(void *ctx,void *p,size_t bytes,unsigned role) {
    model *m=ctx;region *v=find(m,p);CHECK(v && v->private_map && v->role==role && v->bytes==bytes);
    ++m->private_releases[role];
    if(m->fail_release==(uintptr_t)p)return 0;
    memset(v,0,sizeof(*v));return 1;
}
static int healthy(void *ctx) { return !((model *)ctx)->unhealthy; }
static int finish(void *ctx) { model *m=ctx;CHECK(!live(m));++m->finishes;return !m->fail_finish; }
static tsfp_layout_host host(model *m) { return (tsfp_layout_host){m,begin,map_private,unmap_private,healthy,finish}; }
static void run(size_t page,unsigned fixed) {
    model m={0};tsfp_guest_map_layout s={0};tsfp_layout_host h=host(&m);
    tsfp_guest_map_config cfg={64u*1024*1024,64u*1024*1024,1};
    uintptr_t base;unsigned calls;
    m.page=page;m.fixed=fixed;
    CHECK(tsfp_guest_map_init(&s,&h,&cfg));CHECK(s.ready && s.started && !s.published);
    CHECK(s.ram && s.physical && s.nv2a && s.mcpx && s.flash && s.kernel && s.tiled && s.high);
    CHECK(s.tiled_bytes==cfg.physical_bytes && s.physical_shared);
    base=(uintptr_t)s.ram;
    CHECK((uintptr_t)s.physical==base+0x80000000 && (uintptr_t)s.kernel==base+0x80010000);
    CHECK(!m.private_calls[TSFP_MAP_KERNEL] && !m.private_calls[TSFP_MAP_PHYSICAL]);
    CHECK(find(&m,s.ram)->token!=find(&m,s.physical)->token);
    CHECK(find(&m,s.tiled)->token==find(&m,s.physical)->token);
    CHECK(find(&m,s.high)->token==find(&m,s.physical)->token);
    for(unsigned i=0;i<28;++i) {
        CHECK((uintptr_t)s.mirrors[i]==base+(uint64_t)(i+1)*cfg.ram_bytes);
        CHECK(find(&m,s.mirrors[i])->token==find(&m,s.ram)->token);
    }
    CHECK(!tsfp_guest_map_init(&s,&h,&cfg));
    m.fail_release=(uintptr_t)s.flash;
    CHECK(!tsfp_guest_map_stop(&s));CHECK(s.started && !s.ready && live(&m)==1 && !m.finishes);
    CHECK(m.private_releases[TSFP_MAP_KERNEL]==0);
    CHECK(!tsfp_guest_map_init(&s,&h,&cfg));
    m.fail_release=0;m.fail_finish=1;
    CHECK(!tsfp_guest_map_stop(&s));CHECK(!live(&m) && s.started);
    m.fail_finish=0;CHECK(tsfp_guest_map_stop(&s));CHECK(!s.started && !s.ram && m.closed==m.opened);
    calls=m.finishes;CHECK(tsfp_guest_map_stop(&s));CHECK(m.finishes==calls);

    m=(model){0};m.page=page;m.fixed=fixed;h=host(&m);
    m.fail_open=2; /* physical backing unavailable: independent private fallback */
    CHECK(tsfp_guest_map_init(&s,&h,&cfg));CHECK(s.physical && !s.physical_shared && !s.tiled && !s.high && !s.tiled_bytes);
    CHECK(m.private_calls[TSFP_MAP_PHYSICAL]==1 && !m.private_calls[TSFP_MAP_KERNEL]);
    CHECK(tsfp_guest_map_stop(&s));CHECK(m.closed==m.opened-1 && !live(&m));

    m=(model){0};m.page=page;m.fixed=fixed;h=host(&m);
    m.fail_open=2;m.private_fail_mask=1u<<TSFP_MAP_PHYSICAL;
    CHECK(tsfp_guest_map_init(&s,&h,&cfg));CHECK(!s.physical && s.kernel && !s.tiled);
    CHECK(m.private_calls[TSFP_MAP_KERNEL]==1);CHECK(tsfp_guest_map_stop(&s));CHECK(m.private_releases[TSFP_MAP_KERNEL]==1);

    m=(model){0};m.page=page;m.fixed=fixed;h=host(&m);m.unhealthy=1;
    CHECK(!tsfp_guest_map_init(&s,&h,&cfg));CHECK(!s.started && !live(&m) && m.closed==m.opened);
    m.unhealthy=0;m.fail_begin=1;
    CHECK(!tsfp_guest_map_init(&s,&h,&cfg));CHECK(!s.started && !live(&m));m.fail_begin=0;

    cfg.ram_bytes=256u*1024*1024;
    CHECK(tsfp_guest_map_init(&s,&h,&cfg));CHECK(s.tiled_bytes==64u*1024*1024);
    CHECK(!s.mirrors[14]); /* guest0xf0000000 overlaps the architectural alias */
    for(unsigned i=15;i<28;++i)CHECK(!s.mirrors[i]); /* cannot extend beyond32-bit guest space */
    CHECK(tsfp_guest_map_stop(&s));

    cfg.ram_bytes=64u*1024*1024;
    CHECK(tsfp_guest_map_init(&s,&h,&cfg));CHECK(tsfp_guest_map_publish(&s));
    calls=m.unmap_calls+m.closed+m.finishes;
    CHECK(tsfp_guest_map_stop(&s)==2);CHECK(s.started && s.published && s.ram && !s.ready);
    CHECK(calls==m.unmap_calls+m.closed+m.finishes);
    CHECK(!tsfp_guest_map_init(&s,&h,&cfg));CHECK(tsfp_guest_map_stop(&s)==2);
    CHECK(calls==m.unmap_calls+m.closed+m.finishes);
}
int main(void) {
    run(4096,0);run(16384,1);
    puts("PASS: guest layout roles, aliases, fallback, borrowed kernel, rollback and published lifetime");return 0;
}
