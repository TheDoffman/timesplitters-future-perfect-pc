#include "guest_map_layout.h"
#include <string.h>
static void *address(const tsfp_guest_map_layout *s,uint64_t guest,size_t bytes) {
    uintptr_t base=(uintptr_t)s->ram;
    if(!base || guest>UINT32_MAX || !bytes || bytes>UINT64_C(0x100000000)-guest ||
       guest>UINTPTR_MAX-base || bytes>UINTPTR_MAX-(base+(uintptr_t)guest))return NULL;
    return (void *)(base+(uintptr_t)guest);
}
static void *private_at(tsfp_guest_map_layout *s,uint32_t guest,size_t bytes,unsigned role) {
    void *at=address(s,guest,bytes),*p;
    if(!at)return NULL;
    p=s->host.private_map(s->host.context,at,bytes,role);
    if(p) { s->private_views[role]=p;s->private_bytes[role]=bytes; }
    /* Host contract: exact placement or NULL, retaining failed cleanup internally. */
    return p;
}
int tsfp_guest_map_stop(tsfp_guest_map_layout *s) {
    int ok=1;
    if(!s)return 0;
    s->ready=0;
    if(s->published)return 2;
    if(!s->started)return 1;
    for(unsigned i=0;i<5;++i) {
        if(!s->private_views[i])continue;
        if(s->host.private_unmap(s->host.context,s->private_views[i],s->private_bytes[i],i)) {
            s->private_views[i]=NULL;s->private_bytes[i]=0;
        } else ok=0;
    }
    if(!tsfp_shared_views_stop(&s->physical_store))ok=0;
    if(!tsfp_shared_views_stop(&s->ram_store))ok=0;
    /* Finish may withdraw all MMIO aliases and release the arena only after
     * the other owners relinquished their views. Never discard a failed owner. */
    if(ok && !s->host.finish(s->host.context))ok=0;
    if(ok)memset(s,0,sizeof(*s));
    return ok;
}
int tsfp_guest_map_publish(tsfp_guest_map_layout *s) {
    if(!s || !s->ready || !s->host.healthy(s->host.context))return 0;
    s->published=1;return 1;
}
int tsfp_guest_map_init(tsfp_guest_map_layout *s,const tsfp_layout_host *host,const tsfp_guest_map_config *config) {
    tsfp_layout_host h;
    tsfp_guest_map_config cfg;
    tsfp_shared_backend backing;
    void *fixed=NULL,*at;
    size_t page=0,gran=0,tiled_exclusion;
    static const uintptr_t hints[]={0x10000,0x800000,0x1000000,0x2000000,0x10000000,0};
    if(!s || !host || !config || !host->begin || !host->private_map || !host->private_unmap ||
       !host->healthy || !host->finish || s->started || s->published ||
       !config->ram_bytes || config->ram_bytes>UINT32_MAX ||
       config->physical_bytes<0x11000 || config->physical_bytes>0x10000000)return 0;
    h=*host;cfg=*config;
    memset(s,0,sizeof(*s));s->host=h;s->config=cfg;s->started=1;s->ram_bytes=cfg.ram_bytes;
    if(!h.begin(h.context,&backing,&fixed,&page,&gran) ||
       !tsfp_shared_views_init(&s->ram_store,&backing,cfg.ram_bytes,page,gran,s->ram_records,29))goto fail;
    if(fixed)s->ram=tsfp_shared_views_map(&s->ram_store,fixed,0,cfg.ram_bytes,TSFP_VIEW_WRITE);
    else for(size_t i=0;i<sizeof(hints)/sizeof(hints[0]);++i) {
        s->ram=tsfp_shared_views_map(&s->ram_store,(void *)hints[i],0,cfg.ram_bytes,TSFP_VIEW_WRITE);
        if(s->ram)break;
    }
    if(!s->ram)goto fail;
    at=address(s,0x80000000,cfg.physical_bytes);
    if(at && tsfp_shared_views_init(&s->physical_store,&backing,cfg.physical_bytes,page,gran,s->physical_records,3)) {
        s->physical=tsfp_shared_views_map(&s->physical_store,at,0,cfg.physical_bytes,TSFP_VIEW_WRITE);
        s->physical_shared=s->physical!=NULL;
    }
    if(!s->physical) {
        (void)tsfp_shared_views_stop(&s->physical_store);
        s->physical=private_at(s,0x80000000,cfg.physical_bytes,TSFP_MAP_PHYSICAL);
    }
    s->nv2a=private_at(s,0xfd000000,0x1000000,TSFP_MAP_NV2A);
    s->mcpx=private_at(s,0xfe800000,0x800000,TSFP_MAP_MCPX);
    s->flash=private_at(s,0xff000000,0x100000,TSFP_MAP_FLASH);
    s->kernel=s->physical?address(s,0x80010000,4096):private_at(s,0x80010000,4096,TSFP_MAP_KERNEL);
    tiled_exclusion=cfg.ram_bytes<0x0d000000?cfg.ram_bytes:0x0d000000;
    s->tiled_bytes=tiled_exclusion<cfg.physical_bytes?tiled_exclusion:cfg.physical_bytes;
    if(s->physical_shared) {
        at=address(s,0xf0000000,s->tiled_bytes);
        if(at)s->tiled=tsfp_shared_views_map(&s->physical_store,at,0,s->tiled_bytes,TSFP_VIEW_WRITE);
        if(cfg.high_alias && s->tiled) {
            at=address(s,0xf8000000,s->tiled_bytes);
            if(at)s->high=tsfp_shared_views_map(&s->physical_store,at,0,s->tiled_bytes,TSFP_VIEW_WRITE);
        }
    }
    if(!s->tiled)s->tiled_bytes=0;
    for(unsigned i=0;i<28;++i) {
        uint64_t lo=(uint64_t)(i+1)*cfg.ram_bytes,hi=lo+cfg.ram_bytes;
        if(lo<UINT64_C(0xf0000000)+tiled_exclusion && UINT64_C(0xf0000000)<hi)continue;
        at=address(s,lo,cfg.ram_bytes);
        if(at)s->mirrors[i]=tsfp_shared_views_map(&s->ram_store,at,0,cfg.ram_bytes,TSFP_VIEW_WRITE);
    }
    if(!h.healthy(h.context))goto fail;
    s->ready=1;return 1;
fail:
    (void)tsfp_guest_map_stop(s);return 0;
}
