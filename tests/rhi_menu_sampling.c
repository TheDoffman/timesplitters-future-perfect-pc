/* The same production HLSL and pixel contract through the common RHI.
 * No game assets, saves or generated game code are consumed. */
#ifdef _WIN32
#define COBJMACROS
#include <d3d11.h>
#endif
#include "rhi.h"
#include "pc_menu_sampling.h"
#include "graphics/point_size_shader.h"
#include "menu_sampling_cases.h"
#include <stdlib.h>
#ifdef RHI_VULKAN_SDL
#include <SDL3/SDL.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static RhiImage *target;
static RhiBuffer *parameters;
static void render(float enabled,float scale,float *pixels) {
    float data[4]={enabled,scale,0,0};
    rhi_buffer_update(parameters,data);
    rhi_draw(3,0);
    CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
}
static RhiShader *shader(uint32_t stage,const char *text,const char *profile) {
    RhiShaderSource source={text,strlen(text),"shared-menu-contract",NULL,"main",profile,1};
    char errors[4096]={0};
    RhiShader *s=rhi_shader_create(stage,&source,errors,sizeof errors);
    if(!s)fprintf(stderr,"Shader failure: %s\n",errors);
    CHECK(s);return s;
}
#include "rhi_depth_replay.h"
#include "rhi_texture_resources.h"
#include "rhi_xbox_textures.h"
#include "rhi_screen_copy.h"
#ifdef RHI_VULKAN_SDL
#include "vulkan_present_outcomes.h"
#endif
int main(void) {
    void *window=NULL;
#ifdef RHI_VULKAN_SDL
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    window=SDL_CreateWindow("Shared RHI regression",64,64,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);
    CHECK(window);
    CHECK(rhi_select_backend("vulkan")==0);
    RhiDeviceDesc device={window,64,64,2,1,1};
    CHECK(rhi_device_create(&device)==0 && rhi_device_ready());
#elif defined(_WIN32)
    ID3D11Device *warp=NULL;ID3D11DeviceContext *context=NULL;D3D_FEATURE_LEVEL feature;
    CHECK(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,
        D3D11_SDK_VERSION,&warp,&feature,&context)));
    CHECK(rhi_d3d11_adopt(warp,context)==0 && rhi_device_ready());
    CHECK(rhi_present_kind(0,RHI_PRESENT_GUEST)<0); /* WARP fixture has no swapchain */
#else
#error A native host adapter is required
#endif
    RhiShader *vs=shader(RHI_STAGE_VERTEX,
        "float4 main(uint id:SV_VertexID):SV_POSITION {"
        "return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,0,1);}","vs_5_0");
    RhiShader *ps=shader(RHI_STAGE_PIXEL,TSFP_MENU_HLSL
        "Texture2D tex:register(t0); SamplerState samp:register(s0);"
        "cbuffer Params:register(b0) {float enabled;float scale;float2 pad;}"
        "float4 main(float4 p:SV_POSITION):SV_TARGET {"
        "return menu_sample(tex,samp,p.xy/64.0*scale,enabled);}","ps_5_0");
    RhiImageDesc td={0};td.width=td.height=64;td.depth=td.mip_levels=td.samples=1;
    td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_RENDER_TARGET;
    target=rhi_image_create(&td,NULL);CHECK(target);
    RhiView *rtv=rhi_view_create(target,RHI_VIEW_RENDER_TARGET,NULL);CHECK(rtv);
    float texels[16*16*4];menu_sampling_texels(texels);
    td.width=td.height=16;td.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData initial={texels,16*4*sizeof(float),sizeof texels};
    RhiImage *texture=rhi_image_create(&td,&initial);CHECK(texture);
    RhiView *srv=rhi_view_create(texture,RHI_VIEW_SAMPLED,NULL);CHECK(srv);
    RhiBufferDesc bd={16,RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    parameters=rhi_buffer_create(&bd,NULL);CHECK(parameters);
    RhiSamplerDesc sd={0};sd.filter=RHI_FILTER_LINEAR;
    sd.address_u=sd.address_v=sd.address_w=RHI_ADDRESS_CLAMP;sd.max_lod=1000;sd.max_anisotropy=1;sd.compare=RHI_CMP_ALWAYS;
    RhiSampler *sampler=rhi_sampler_create(&sd);CHECK(sampler);
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);
    RhiDepthDesc dd={0};dd.depth_func=RHI_CMP_ALWAYS;
    dd.front.fail=dd.front.depth_fail=dd.front.pass=RHI_STENCIL_KEEP;dd.front.func=RHI_CMP_ALWAYS;
    dd.back=dd.front;
    RhiDepthState *depth=rhi_depth_state_create(&dd);CHECK(depth);
    RhiBlendDesc bl={0};bl.write_mask=RHI_WRITE_ALL;
    bl.src=bl.src_alpha=RHI_BLEND_ONE;bl.dst=bl.dst_alpha=RHI_BLEND_ZERO;
    bl.op=bl.op_alpha=RHI_BLEND_OP_ADD;
    RhiBlendState *blend=rhi_blend_state_create(&bl);CHECK(blend);
    RhiViewport viewport={0,0,64,64,0,1};
    rhi_set_render_target(rtv,NULL);rhi_set_viewports(1,&viewport);
    rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
    rhi_set_textures(0,1,&srv);rhi_set_samplers(0,1,&sampler);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&parameters);
    rhi_set_raster_state(raster);rhi_set_depth_state(depth,0);
    rhi_set_blend_state(blend,NULL,~0u);
    CHECK(menu_sampling_cases(render));
    /* Common NV2A texture signed-mapping helpers use vector conditionals.
     * This must retain FXC semantics even when DXC defaults to HLSL 2021+. */
    RhiShader *legacy=shader(RHI_STAGE_PIXEL,
        "float4 main(float4 p:SV_POSITION):SV_TARGET {"
        "float3 x=float3(p.x,128,256);"
        "return float4(x>=128 ? float3(.2,.4,.6) : float3(.8,.1,.3),1);}","ps_5_0");
    rhi_set_shader(RHI_STAGE_PIXEL,legacy);rhi_draw(3,0);
    float conditional_pixels[64*64*4];
    CHECK(rhi_image_readback(target,0,conditional_pixels,64*4*sizeof(float))==0);
    CHECK(fabsf(conditional_pixels[0]-.8f)<.001f && fabsf(conditional_pixels[1]-.4f)<.001f &&
          fabsf(conditional_pixels[2]-.6f)<.001f);
    rhi_set_shader(RHI_STAGE_PIXEL,ps);rhi_shader_destroy(legacy);
    RhiShader *point_vs=shader(RHI_STAGE_VERTEX,
        "struct Output {float4 pos:SV_POSITION;\n" TSFP_POINT_SIZE_FIELD "};\n"
        "Output main(uint id:SV_VertexID) {Output o;\n" TSFP_POINT_SIZE_INIT
        "o.pos=float4(.015625,.015625,0,1);return o;}","vs_5_0");
    RhiShader *point_ps=shader(RHI_STAGE_PIXEL,
        "float4 main():SV_TARGET{return float4(1,0,0,1);}","ps_5_0");
    const float black[4]={0,0,0,1};rhi_clear_color(rtv,black);
    rhi_set_shader(RHI_STAGE_VERTEX,point_vs);rhi_set_shader(RHI_STAGE_PIXEL,point_ps);
    rhi_set_topology(RHI_TOPOLOGY_POINTS);rhi_draw(1,0);
    CHECK(rhi_image_readback(target,0,conditional_pixels,64*4*sizeof(float))==0);
    unsigned lit=0;
    for(unsigned i=0;i<64*64;i++)lit+=conditional_pixels[i*4]>.99f;
    CHECK(lit==1);
    rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
    rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_shader_destroy(point_vs);rhi_shader_destroy(point_ps);
    rhi_texture_resources_test(rtv);
    rhi_xbox_textures_test();
    rhi_set_shader(RHI_STAGE_PIXEL,ps);rhi_set_textures(0,1,&srv);
    rhi_set_samplers(0,1,&sampler);rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&parameters);
    rhi_depth_replay_test(rtv);
    rhi_screen_copy_test(rtv);

#ifdef RHI_VULKAN_SDL
    /* Also exercise presentation and the swapchain readback channel order. */
    RhiView *swap=rhi_swapchain_view();CHECK(swap);
    const float color[4]={.25f,.5f,.75f,1};
    rhi_set_render_target(swap,NULL);rhi_clear_color(swap,color);
    unsigned char pixels[64*64*4];uint32_t w=64,h=64;
    CHECK(rhi_swapchain_readback(pixels,64*4,&w,&h)==0 && w==64 && h==64);
    CHECK(abs(pixels[0]-64)<=1 && abs(pixels[1]-128)<=1 && abs(pixels[2]-191)<=1);
    CHECK(rhi_present(0)==0);
    vulkan_present_outcomes();
#endif
    rhi_set_render_target(NULL,NULL);
    rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);
    rhi_device_destroy();
#ifdef RHI_VULKAN_SDL
    SDL_DestroyWindow(window);SDL_Quit();
#endif
    #ifdef RHI_VULKAN_SDL
    puts("PASS: shared RHI HLSL, menu pixel contract, swapchain readback and present");
#else
    puts("PASS: shared RHI HLSL and menu pixel contract on D3D11 WARP (no swapchain)");
#endif
    return 0;
}
