/* Asset-free sequence contract, not a simulation of the canyon explosion.
 * Keep targets alive across flash/recovery and inspect every output pixel. */
#include <math.h>
#include "graphics/pc_smaa_settings.h"
#include "graphics/msaa_color_surface.h"
#include "graphics/ssao_pass.h"
#include "graphics/ssao_settings.h"

static void test_graphics_transition(void)
{
    const unsigned sizes[][2]={{64,48},{80,45},{107,45}};
    unsigned cases=0;
    for(unsigned shape=0;shape<3;shape++)
    for(unsigned samples=1;samples<=8;samples*=2) {
        if(!rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,samples))continue;
        unsigned w=sizes[shape][0],h=sizes[shape][1],n=w*h;
        unsigned char *input=malloc(n*4),*output=malloc(n*4);
        float *depth=malloc(n*sizeof(float));assert(input&&output&&depth);
        const float a=100.f/99.9f,b=-10.f/99.9f;
        float inverse[16]={1,0,0,0,0,1,0,0,0,0,0,1/b,0,0,1,-a/b};
        RhiImageDesc d={0};d.type=RHI_IMAGE_2D;d.width=w;d.height=h;
        d.depth=d.mip_levels=d.samples=1;d.format=RHI_FORMAT_R8G8B8A8_UNORM;
        d.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
        RhiImage *scene=rhi_image_create(&d,NULL),*display=rhi_image_create(&d,NULL);
        assert(scene&&display);
        RhiView *scene_rtv=rhi_view_create(scene,RHI_VIEW_RENDER_TARGET,NULL);
        RhiView *scene_srv=rhi_view_create(scene,RHI_VIEW_SAMPLED,NULL);
        RhiView *display_rtv=rhi_view_create(display,RHI_VIEW_RENDER_TARGET,NULL);
        d.format=RHI_FORMAT_R32_FLOAT;d.bind=RHI_BIND_SAMPLED;
        RhiImage *z=rhi_image_create(&d,NULL);assert(z);
        RhiView *z_srv=rhi_view_create(z,RHI_VIEW_SAMPLED,NULL);
        assert(scene_rtv&&scene_srv&&display_rtv&&z_srv);
        TsfpMsaaColorSurface ms={0};TsfpSsaoPass ao={0};
        memset(input,0,n*4);rhi_image_update(scene,0,NULL,input,w*4,0);
        if(samples>1)assert(tsfp_msaa_color_create(&ms,scene,0,samples));
        for(unsigned enabled=0;enabled<2;enabled++)
        for(unsigned smoothing=0;smoothing<2;smoothing++)
        for(unsigned smaa_enabled=0;smaa_enabled<2;smaa_enabled++) {
            assert(pc_smaa_set_enabled((int)smaa_enabled));
            TsfpGraphicsOptions options=tsfp_graphics_defaults();
            options.edge_smoothing=smoothing;options.ambient_occlusion=enabled;
            assert(pc_graphics_set(&options));
            /* World, full flash with cleared depth, fading flash, world recovery.
             * Alternate colors so a one-frame-old resolve cannot pass. */
            for(unsigned frame=0;frame<6;frame++) {
                unsigned base=frame==1?255:frame==2?224:frame==3?176:frame==4?96:64;
                for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
                    unsigned i=y*w+x;
                    unsigned c=(frame!=1&&x>w*3/4&&y>h/2)?base+24:base;
                    input[i*4]=input[i*4+1]=input[i*4+2]=(unsigned char)c;
                    input[i*4+3]=255;depth[i]=frame==1?1.f:a+b/10.f;
                }
                rhi_image_update(scene,0,NULL,input,w*4,0);
                rhi_image_update(z,0,NULL,depth,w*sizeof(float),0);
                RhiView *draw=samples>1?ms.rtv:scene_rtv;
                if(samples>1)assert(tsfp_msaa_color_seed(&ms));
                if(enabled) {
                    assert(tsfp_ssao_render(&ao,z_srv,w,h,inverse,1,
                        TSFP_SSAO_RADIUS,TSFP_SSAO_BIAS,TSFP_SSAO_STRENGTH));
                    assert(tsfp_ssao_apply(&ao,z_srv,draw,w,h));
                }
                if(samples>1)assert(tsfp_msaa_color_resolve(&ms));
                const float poison[4]={1,0,1,1};rhi_clear_color(display_rtv,poison);
                D3D8DisplayFit fit={0,0,(int)w,(int)h};
                assert(SUCCEEDED(d3d8_display_resolve(scene_srv,w,h,display_rtv,fit)));
                assert(rhi_image_readback(display,0,output,w*4)==0);
                for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
                    unsigned i=(y*w+x)*4;
                    /* Only the synthetic weapon boundary may be smoothed. */
                    assert(output[i]>=base-1&&output[i]<=base+25);
                    assert(output[i]==output[i+1]&&output[i]==output[i+2]);
                    assert(output[i+3]==255);
                    if(frame==1)assert(output[i]==255);
                    if(x<w/2)assert(abs((int)output[i]-(int)base)<=1);
                    if(frame!=1&&x>w*7/8&&y>h*3/4)
                        assert(abs((int)output[i]-(int)base-24)<=1);
                }
            }
            cases++;
        }
        rhi_set_render_target(NULL,NULL);tsfp_ssao_destroy(&ao);tsfp_msaa_color_destroy(&ms);
        rhi_view_destroy(z_srv);rhi_image_destroy(z);
        rhi_view_destroy(scene_rtv);rhi_view_destroy(scene_srv);rhi_view_destroy(display_rtv);
        rhi_image_destroy(scene);rhi_image_destroy(display);free(input);free(output);free(depth);
    }
    assert(pc_smaa_set_enabled(0));
    printf("PASS: %u graphics transition combinations, six consecutive frames each\n",cases);
}
