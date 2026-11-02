#define COBJMACROS
#include <d3d11.h>
#include <assert.h>
#include "d3d8_display.h"
#include "graphics/graphics_settings.h"
#include "msaa_resolve.h"
#include "graphics_transition.h"

static void test_supersampling(void)
{
    /* Compare every output pixel to the exact 2x2 footprint. Irregular colors
     * expose half-pixel shifts, missing taps and source/destination confusion. */
    unsigned char input[32*24*4],output[16*12*4];
    RhiImageDesc d={0};RhiSubresourceData data={0};
    TsfpGraphicsOptions o=tsfp_graphics_defaults();
    assert(pc_graphics_set(&o));
    for(unsigned y=0;y<24;y++)for(unsigned x=0;x<32;x++) {
        unsigned i=(y*32+x)*4;
        input[i]=(unsigned char)((x*37+y*13)%256);
        input[i+1]=(unsigned char)((x*19+y*53)%256);
        input[i+2]=(unsigned char)((x*61+y*7)%256);
        input[i+3]=255;
    }
    d.type=RHI_IMAGE_2D;d.width=32;d.height=24;d.depth=d.mip_levels=d.samples=1;
    d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.bind=RHI_BIND_SAMPLED;
    data.data=input;data.row_pitch=32*4;data.slice_pitch=sizeof input;
    RhiImage *scene=rhi_image_create(&d,&data);assert(scene);
    RhiView *srv=rhi_view_create(scene,RHI_VIEW_SAMPLED,NULL);assert(srv);
    d.width=16;d.height=12;d.bind=RHI_BIND_RENDER_TARGET;
    RhiImage *target=rhi_image_create(&d,NULL);assert(target);
    RhiView *rtv=rhi_view_create(target,RHI_VIEW_RENDER_TARGET,NULL);assert(rtv);
    D3D8DisplayFit fit={0,0,16,12};
    assert(SUCCEEDED(d3d8_display_resolve(srv,32,24,rtv,fit)));
    assert(!rhi_image_readback(target,0,output,16*4));
    for(unsigned y=0;y<12;y++)for(unsigned x=0;x<16;x++)for(unsigned c=0;c<4;c++) {
        unsigned sum=0;
        for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)
            sum+=input[((y*2+dy)*32+x*2+dx)*4+c];
        assert(abs((int)output[(y*16+x)*4+c]-(int)((sum+2)/4))<=1);
    }
    rhi_set_render_target(NULL,NULL);rhi_view_destroy(srv);rhi_view_destroy(rtv);
    rhi_image_destroy(scene);rhi_image_destroy(target);

    /* Saved settings are distinct from the active restart-latched scale. */
    unsigned w,h;o.supersampling=3;assert(pc_graphics_set(&o));
    d3d8_display_supersample_size(1920,1080,&w,&h);assert(w==3840 && h==2160);
    o.supersampling=0;assert(pc_graphics_set(&o));
    d3d8_display_supersample_size(1920,1080,&w,&h);assert(w==3840 && h==2160);
    d3d8_display_supersample_size(7680,4320,&w,&h);assert(w==7680 && h==4320);
    assert(pc_graphics_get().supersampling==0);
    puts("PASS: supersampling exact 2x2 GPU resolve, size limits and restart latch");
}

int main(void)
{
    ID3D11Device *device=NULL;ID3D11DeviceContext *context=NULL;D3D_FEATURE_LEVEL level;
    unsigned char input[32*32*4],off[sizeof input],on[sizeof input],again[sizeof input];
    RhiImageDesc desc={0};RhiSubresourceData data={0};
    RhiImage *source,*target;RhiView *srv,*rtv;
    TsfpGraphicsOptions options=tsfp_graphics_defaults();D3D8DisplayFit fit={0,0,32,32};
    assert(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&device,&level,&context)));
    assert(rhi_d3d11_adopt(device,context)==0);
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        int p=(y*32+x)*4;unsigned char c=(unsigned char)(x*2>y+6?240:16);
        input[p]=input[p+1]=input[p+2]=c;input[p+3]=255;
    }
    desc.type=RHI_IMAGE_2D;desc.width=desc.height=32;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_SAMPLED;
    data.data=input;data.row_pitch=32*4;data.slice_pitch=sizeof input;
    source=rhi_image_create(&desc,&data);assert(source);
    srv=rhi_view_create(source,RHI_VIEW_SAMPLED,NULL);assert(srv);
    desc.bind=RHI_BIND_RENDER_TARGET;target=rhi_image_create(&desc,NULL);assert(target);
    rtv=rhi_view_create(target,RHI_VIEW_RENDER_TARGET,NULL);assert(rtv);
    assert(pc_graphics_set(&options));
    assert(SUCCEEDED(d3d8_display_resolve(srv,32,32,rtv,fit)));
    assert(rhi_image_readback(target,0,off,32*4)==0);
    /* Pixel centers must be preserved at 100%; a half-texel shift blurs the
     * baseline and makes ordered-grid supersampling sample outside its cell. */
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        int expected=input[(y*32+x)*4];
        assert(abs((int)off[(y*32+x)*4]-expected)<=1);
    }
    options.edge_smoothing=1;assert(pc_graphics_set(&options));
    assert(SUCCEEDED(d3d8_display_resolve(srv,32,32,rtv,fit)));
    assert(rhi_image_readback(target,0,on,32*4)==0);
    int changed=0;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        int p=(y*32+x)*4;assert(on[p+3]==255);
        if(on[p]!=off[p])changed++;
        if(x>26 || x==0)assert(on[p]==off[p]);
    }
    assert(changed>15 && changed<300);
    options.edge_smoothing=0;assert(pc_graphics_set(&options));
    assert(SUCCEEDED(d3d8_display_resolve(srv,32,32,rtv,fit)));
    assert(rhi_image_readback(target,0,again,32*4)==0);
    assert(memcmp(off,again,sizeof off)==0);
    options.anisotropy=16;assert(pc_graphics_set(&options));
    assert(d3d8_display_policy()->anisotropy==16);
    options.anisotropy=1;assert(pc_graphics_set(&options));
    assert(d3d8_display_policy()->anisotropy==1);
    test_graphics_transition();
    test_supersampling();
    options.edge_smoothing=0;assert(pc_graphics_set(&options));
    /* A save failure cannot publish an unpersisted rendering preference. */
    assert(_putenv_s("TSFP_DISPLAY_CONFIG","missing/settings/display.ini")==0);
    options.edge_smoothing=1;
    assert(!pc_graphics_set(&options));
    assert(pc_graphics_get().edge_smoothing==0);
    d3d8_display_shutdown();rhi_view_destroy(srv);rhi_view_destroy(rtv);
    rhi_image_destroy(source);rhi_image_destroy(target);
    test_msaa_resolve();rhi_device_destroy();
    puts("PASS: production resolve, live FXAA, unchanged flat regions and exact OFF restoration");
    return 0;
}
