#include <assert.h>
static int msaa_fail_images,msaa_fail_depth;
static RhiImage *msaa_create_image(const RhiImageDesc *d,const RhiSubresourceData *initial)
{
    if(msaa_fail_images || (msaa_fail_depth && d->samples>1 && (d->bind&RHI_BIND_DEPTH)))return NULL;
    return rhi_image_create(d,initial);
}
#define rhi_image_create msaa_create_image
#include "graphics/msaa_targets.h"
#undef rhi_image_create
static void test_msaa_targets(void)
{
    TsfpMsaaTargets p={0};
    for(unsigned requested=1;requested<=8;requested*=2){
        assert(tsfp_msaa_targets_create(&p,17,19,RHI_FORMAT_R8G8B8A8_UNORM,RHI_FORMAT_D24_UNORM_S8_UINT,requested));
        assert(p.samples<=requested && p.samples>=1 && p.width==17 && p.height==19);
        RhiImageDesc color,depth,sample;
        rhi_image_get_desc(p.color,&color);rhi_image_get_desc(p.depth,&depth);
        rhi_image_get_desc(rhi_view_image(p.srv),&sample);
        assert(color.samples==p.samples && depth.samples==p.samples && sample.samples==1);
        assert((p.resolved!=NULL)==(p.samples>1));
        const float green[4]={0,1,0,1};unsigned char pixels[17*19*4];
        rhi_set_render_target(p.rtv,p.dsv);rhi_clear_color(p.rtv,green);
        assert(tsfp_msaa_targets_sample(&p)==p.srv);
        RhiOutputState saved;rhi_output_save(&saved);
        assert(rhi_output_color_is(&saved,p.rtv));rhi_output_restore(&saved);
        assert(rhi_image_readback(rhi_view_image(p.srv),0,pixels,17*4)==0);
        for(unsigned i=0;i<17*19;i++)assert(pixels[i*4]==0 && pixels[i*4+1]==255);
        rhi_set_render_target(NULL,NULL);
    }
    /* Failure after color allocation must unwind before trying lower counts. */
    msaa_fail_depth=1;
    assert(tsfp_msaa_targets_create(&p,21,23,RHI_FORMAT_R8G8B8A8_UNORM,RHI_FORMAT_D24_UNORM_S8_UINT,8));
    assert(p.samples==1 && !p.resolved);msaa_fail_depth=0;
    TsfpMsaaTargets previous=p;
    msaa_fail_images=1;
    assert(!tsfp_msaa_targets_create(&p,31,29,RHI_FORMAT_R8G8B8A8_UNORM,RHI_FORMAT_D24_UNORM_S8_UINT,8));
    assert(!memcmp(&p,&previous,sizeof p));msaa_fail_images=0;
    assert(!tsfp_msaa_targets_create(&p,0,29,RHI_FORMAT_R8G8B8A8_UNORM,RHI_FORMAT_D24_UNORM_S8_UINT,8));
    assert(!memcmp(&p,&previous,sizeof p));
    assert(tsfp_msaa_targets_create(&p,9,11,RHI_FORMAT_R8G8B8A8_UNORM,RHI_FORMAT_D24_UNORM_S8_UINT,4));
    tsfp_msaa_targets_destroy(&p);tsfp_msaa_targets_destroy(&p);
    assert(!tsfp_msaa_targets_sample(&p));
    puts("PASS: paired MSAA targets, sampled resolve, resize and allocation fallback");
}
