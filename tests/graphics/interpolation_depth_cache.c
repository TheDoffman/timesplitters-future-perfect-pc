/* Fault injection and lifetime checks for the production shared depth cache. */
#include "graphics/interpolation_depth.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
struct RhiImage {RhiImageDesc desc;unsigned refs;float depth;unsigned stencil;};
struct RhiView {RhiImage *image;};
static unsigned images,views,clears,fail_image,fail_view;
RhiImage *rhi_image_create(const RhiImageDesc *d,const RhiSubresourceData *initial) {
    (void)initial;if(fail_image)return NULL;
    RhiImage *p=calloc(1,sizeof *p);assert(p);p->desc=*d;p->refs=1;images++;return p;
}
RhiImage *rhi_image_retain(RhiImage *p){assert(p && p->refs);p->refs++;return p;}
void rhi_image_destroy(RhiImage *p){if(p && --p->refs==0){images--;free(p);}}
void rhi_image_get_desc(const RhiImage *p,RhiImageDesc *d){*d=p->desc;}
RhiView *rhi_view_create(RhiImage *p,uint32_t kind,const RhiViewDesc *d) {
    (void)d;assert(kind==RHI_VIEW_DEPTH);if(fail_view)return NULL;
    RhiView *v=calloc(1,sizeof *v);assert(v);v->image=rhi_image_retain(p);views++;return v;
}
void rhi_view_destroy(RhiView *v){if(v){rhi_image_destroy(v->image);views--;free(v);}}
void rhi_clear_depth(RhiView *v,uint32_t flags,float depth,uint8_t stencil) {
    assert(flags==(v->image->desc.format==RHI_FORMAT_D24_UNORM_S8_UINT ?
        RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL : RHI_CLEAR_DEPTH));clears++;
    v->image->depth=depth;v->image->stencil=stencil;
}
int main(void) {
    RhiImageDesc d={0};d.width=64;d.height=32;d.depth=2;d.mip_levels=3;
    d.format=RHI_FORMAT_D24_UNORM_S8_UINT;d.samples=1;d.bind=RHI_BIND_DEPTH;
    RhiImage *source=rhi_image_create(&d,NULL);source->depth=.25f;source->stencil=7;
    TsfpReplayDepth cache={0};tsfp_replay_depth_begin(&cache);
    RhiView *a=tsfp_replay_depth_view(&cache,source,0);assert(a && a->image!=source);
    assert(source->depth==.25f && source->stencil==7 && source->refs==2);
    assert(a->image->depth==1 && !a->image->stencil && clears==1);
    a->image->depth=.1f;a->image->stencil=3;
    assert(tsfp_replay_depth_view(&cache,source,0)==a && clears==1 && a->image->depth==.1f);
    RhiView *b=tsfp_replay_depth_view(&cache,source,4);assert(b && b!=a);
    assert(b->image->desc.width==32 && b->image->desc.height==16 && b->image->desc.depth==1 && b->image->desc.mip_levels==1);
    assert(source->refs==3);tsfp_replay_depth_end(&cache);
    tsfp_replay_depth_begin(&cache);
    assert(tsfp_replay_depth_view(&cache,source,0)==a && a->image->depth==1 && !a->image->stencil);
    tsfp_replay_depth_end(&cache);assert(views==1 && images==2 && source->refs==2);
    cache.epoch=UINT_MAX;tsfp_replay_depth_begin(&cache);
    a->image->depth=.5f;assert(tsfp_replay_depth_view(&cache,source,0)==a && a->image->depth==1);
    tsfp_replay_depth_end(&cache);
    for(unsigned failure=0;failure<2;failure++) {
        tsfp_replay_depth_begin(&cache);fail_image=failure==0;fail_view=failure==1;
        assert(!tsfp_replay_depth_view(&cache,source,2) && cache.failed);
        assert(source->depth==.25f && source->stencil==7);
        fail_image=fail_view=0;tsfp_replay_depth_end(&cache);
        assert(images==1 && views==0 && source->refs==1);
    }
    tsfp_replay_depth_begin(&cache);
    assert(!tsfp_replay_depth_view(&cache,source,6) && cache.failed); /* out-of-range layer */
    tsfp_replay_depth_end(&cache);
    tsfp_replay_depth_begin(&cache);assert(tsfp_replay_depth_view(&cache,source,0));
    d.format=RHI_FORMAT_D16_UNORM;
    RhiImage *depth_only=rhi_image_create(&d,NULL);
    assert(tsfp_replay_depth_view(&cache,depth_only,0));
    rhi_image_destroy(depth_only);
    rhi_image_destroy(source); /* cache retains identity after surface release */
    tsfp_replay_depth_destroy(&cache);assert(!images && !views);
    puts("PASS: depth cache subresources, reuse, eviction, epoch wrap, allocation failures and lifetime");
    return 0;
}
