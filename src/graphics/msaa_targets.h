#ifndef TSFP_MSAA_TARGETS_H
#define TSFP_MSAA_TARGETS_H
#include "rhi.h"
#include <string.h>

/* Color/depth are one allocation transaction. The sampled image always has
 * one sample, so existing guest Texture2D shaders never receive Texture2DMS.
 * Views own references in RHI, but this bundle also owns each image once. */
typedef struct TsfpMsaaTargets {
    RhiImage *color,*resolved,*depth;
    RhiView *rtv,*srv,*dsv;
    unsigned width,height,samples;
} TsfpMsaaTargets;
static void tsfp_msaa_targets_destroy(TsfpMsaaTargets *p)
{
    if(!p)return;
    rhi_view_destroy(p->rtv);rhi_view_destroy(p->srv);rhi_view_destroy(p->dsv);
    rhi_image_destroy(p->color);rhi_image_destroy(p->resolved);rhi_image_destroy(p->depth);
    memset(p,0,sizeof *p);
}
static int tsfp_msaa_targets_try(TsfpMsaaTargets *p,unsigned width,unsigned height,
    RhiFormat color_format,RhiFormat depth_format,unsigned samples)
{
    RhiImageDesc d={0};
    memset(p,0,sizeof *p);
    d.type=RHI_IMAGE_2D;d.width=width;d.height=height;d.depth=d.mip_levels=1;
    d.samples=samples;d.format=color_format;d.usage=RHI_USAGE_DEFAULT;
    d.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
    p->color=rhi_image_create(&d,NULL);if(!p->color)goto fail;
    p->rtv=rhi_view_create(p->color,RHI_VIEW_RENDER_TARGET,NULL);if(!p->rtv)goto fail;
    if(samples>1){
        d.samples=1;p->resolved=rhi_image_create(&d,NULL);if(!p->resolved)goto fail;
    }
    p->srv=rhi_view_create(p->resolved?p->resolved:p->color,RHI_VIEW_SAMPLED,NULL);
    if(!p->srv)goto fail;
    d.samples=samples;d.format=depth_format;d.bind=RHI_BIND_DEPTH|RHI_BIND_SAMPLED;
    p->depth=rhi_image_create(&d,NULL);if(!p->depth)goto fail;
    p->dsv=rhi_view_create(p->depth,RHI_VIEW_DEPTH,NULL);if(!p->dsv)goto fail;
    p->width=width;p->height=height;p->samples=samples;return 1;
fail:
    tsfp_msaa_targets_destroy(p);return 0;
}
/* Caller unbinds the old targets before replacing them. A failed allocation
 * leaves the old bundle intact. Never upgrade a request or change preferences. */
static int tsfp_msaa_targets_create(TsfpMsaaTargets *p,unsigned width,unsigned height,
    RhiFormat color_format,RhiFormat depth_format,unsigned requested)
{
    TsfpMsaaTargets next;
    if(!p||!width||!height)return 0;
    unsigned samples=requested==2||requested==4||requested==8?requested:1;
    for(;;){
        if(rhi_sample_count_supported(color_format,samples)&&
           rhi_sample_count_supported(depth_format,samples)&&
           tsfp_msaa_targets_try(&next,width,height,color_format,depth_format,samples)){
            tsfp_msaa_targets_destroy(p);*p=next;return 1;
        }
        if(samples==1)return 0;
        samples/=2;
    }
}
static RhiView *tsfp_msaa_targets_sample(TsfpMsaaTargets *p)
{
    if(!p||!p->srv)return NULL;
    if(p->resolved && rhi_image_resolve(p->color,p->resolved)!=0)return NULL;
    return p->srv;
}
#endif
