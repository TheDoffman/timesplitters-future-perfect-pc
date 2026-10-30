/* Real GPU regression: every channel mask must preserve unselected channels,
 * ignore inherited clipping, and restore render target/viewport state. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "rhi.h"
#include "d3d8_xbox_map.h"
#include "pc_clear_rhi.inc"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void)
{
    HWND window=CreateWindowExA(0,"STATIC","clear regression",WS_OVERLAPPEDWINDOW,
                                0,0,64,64,NULL,NULL,GetModuleHandleA(NULL),NULL);
    RhiDeviceDesc device={0};RhiImageDesc desc={0};RhiViewport viewport={1,2,3,4,0,1};
    RhiRasterDesc raster={0};RhiRasterState *clipped;RhiRect scissor={0,0,0,0};
    RhiImage *image,*other;RhiView *view,*other_view;RhiOutputState saved;
    const float initial[]={.12f,.23f,.34f,.45f},clear[]={.8f,.7f,.6f,.5f};
    unsigned mask,i,p;unsigned char pixels[8*8*4];
    CHECK(window);device.window=window;device.width=64;device.height=64;device.windowed=1;
    CHECK(!rhi_device_create(&device));
    desc.width=desc.height=8;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_RENDER_TARGET;
    image=rhi_image_create(&desc,NULL);other=rhi_image_create(&desc,NULL);CHECK(image && other);
    view=rhi_view_create(image,RHI_VIEW_RENDER_TARGET,NULL);
    other_view=rhi_view_create(other,RHI_VIEW_RENDER_TARGET,NULL);CHECK(view && other_view);
    raster.fill=RHI_FILL_SOLID;raster.cull=RHI_CULL_NONE;raster.scissor=1;
    clipped=rhi_raster_state_create(&raster);CHECK(clipped);
    CHECK(xbox_clear_flags_to_host(0xF3)==(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL));
    for(mask=1;mask<16;mask++) {
        CHECK(xbox_clear_flags_to_host(mask<<4)==(mask==15 ? D3DCLEAR_TARGET : mask<<4));
        rhi_clear_color(view,initial);
        rhi_set_render_target(other_view,NULL);rhi_set_viewports(1,&viewport);
        rhi_set_scissor(&scissor);rhi_set_raster_state(clipped);
        CHECK(pc_clear_masked(view,8,8,clear,mask));
        rhi_output_save(&saved);
        CHECK(rhi_output_color_is(&saved,other_view));
        CHECK(saved.viewport_count==1 && !memcmp(saved.viewports,&viewport,sizeof viewport));
        rhi_output_restore(&saved);
        CHECK(!rhi_image_readback(image,0,pixels,8*4));
        for(p=0;p<8*8;p++)for(i=0;i<4;i++) {
            float expected=(mask & (1u<<i)) ? clear[i] : initial[i];
            CHECK(fabsf(pixels[p*4+i]-expected*255.f)<=1.f);
        }
    }
    pc_clear_shutdown();rhi_raster_state_destroy(clipped);
    rhi_set_render_target(NULL,NULL);
    rhi_view_destroy(other_view);rhi_view_destroy(view);
    rhi_image_destroy(other);rhi_image_destroy(image);rhi_device_destroy();DestroyWindow(window);
    puts("PASS: all 15 GPU channel-clear masks, alpha reset, RGB preservation, inherited scissor and output restoration");
    return 0;
}
