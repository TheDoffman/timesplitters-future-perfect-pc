#define COBJMACROS
#include <d3d11.h>
#include <assert.h>
#include "d3d8_display.h"
#include "graphics/graphics_settings.h"
#include "msaa_resolve.h"
#include "graphics_transition.h"

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
    /* Existing display resolve samples at the next texel boundary (linear
     * filtering). Check that exact legacy convention, not an idealized copy. */
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        int nx=x<31?x+1:x,ny=y<31?y+1:y;
        int expected=(input[(y*32+x)*4]+input[(y*32+nx)*4]+
            input[(ny*32+x)*4]+input[(ny*32+nx)*4])/4;
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
