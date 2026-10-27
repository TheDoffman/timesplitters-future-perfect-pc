#ifndef TSFP_RHI_STATE_CACHE_TEST_H
#define TSFP_RHI_STATE_CACHE_TEST_H
#include "d3d8_internal.h"
#include "d3d8_display.h"
/* Supply guest state to the complete production d3d8_states.c module. The
 * renderer, state objects, draws and readbacks use the real host GPU adapter. */
static DWORD cache_rs[256], cache_tss[4][32];
static BOOL cache_scissor_on;
static RhiRect cache_scissor;
static D3D8DisplayPolicy cache_policy;
const DWORD *d3d8_GetRenderStates(void) { return cache_rs; }
const DWORD *d3d8_GetTSS(DWORD stage) { return cache_tss[stage]; }
BOOL d3d8_GetScissor(RhiRect *out) { *out=cache_scissor;return cache_scissor_on; }
const D3D8DisplayPolicy *d3d8_display_policy(void) { return &cache_policy; }
static void cache_pixel(float *pixels,unsigned x,unsigned y,float r,float g,float b) {
    float *p=pixels+4*(64*y+x);
    CHECK(fabsf(p[0]-r)<.01f && fabsf(p[1]-g)<.01f && fabsf(p[2]-b)<.01f);
}
static void cache_draw_read(float *pixels) {
    d3d8_states_apply();rhi_draw(3,0);
    CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
}
static void rhi_state_cache_test(RhiView *rtv) {
    RhiShader *vs=shader(RHI_STAGE_VERTEX,
        "float4 main(uint id:SV_VertexID):SV_POSITION {return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,.5,1);}","vs_5_0");
    RhiShader *red=shader(RHI_STAGE_PIXEL,"float4 main():SV_TARGET{return float4(1,0,0,.5);}","ps_5_0");
    RhiShader *green=shader(RHI_STAGE_PIXEL,"float4 main():SV_TARGET{return float4(0,1,0,1);}","ps_5_0");
    RhiImageDesc dd={0};dd.type=RHI_IMAGE_2D;dd.width=dd.height=64;
    dd.depth=dd.mip_levels=dd.samples=1;dd.format=RHI_FORMAT_D24_UNORM_S8_UINT;dd.bind=RHI_BIND_DEPTH;
    RhiImage *depth=rhi_image_create(&dd,NULL);CHECK(depth);
    RhiView *dsv=rhi_view_create(depth,RHI_VIEW_DEPTH,NULL);CHECK(dsv);
    RhiViewport viewport={0,0,64,64,0,1};float pixels[64*64*4];
    const float blue[4]={0,0,1,1};
    memset(cache_rs,0,sizeof(cache_rs));
    cache_rs[D3DRS_SRCBLEND]=D3DBLEND_SRCALPHA;
    cache_rs[D3DRS_DESTBLEND]=D3DBLEND_INVSRCALPHA;
    cache_rs[D3DRS_BLENDOP]=1;cache_rs[D3DRS_COLORWRITEENABLE]=15;
    cache_rs[D3DRS_ZFUNC]=D3DCMP_LESSEQUAL;
    cache_rs[D3DRS_CULLMODE]=D3DCULL_NONE;cache_rs[D3DRS_FILLMODE]=D3DFILL_SOLID;
    cache_rs[D3DRS_STENCILFUNC]=D3DCMP_EQUAL;
    cache_rs[D3DRS_STENCILMASK]=cache_rs[D3DRS_STENCILWRITEMASK]=255;
    cache_rs[D3DRS_STENCILFAIL]=cache_rs[D3DRS_STENCILZFAIL]=cache_rs[D3DRS_STENCILPASS]=1;
    CHECK(d3d8_states_init()==S_OK);
    rhi_set_render_target(rtv,dsv);rhi_set_viewports(1,&viewport);
    rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);rhi_set_shader(RHI_STAGE_VERTEX,vs);
    rhi_set_shader(RHI_STAGE_PIXEL,red);
    for(unsigned repeat=0;repeat<6;repeat++) {
        rhi_clear_color(rtv,blue);
        cache_rs[D3DRS_ALPHABLENDENABLE]=1;cache_draw_read(pixels);
        cache_pixel(pixels,32,32,.5f,0,.5f);
        cache_rs[D3DRS_ALPHABLENDENABLE]=0;cache_draw_read(pixels);
        cache_pixel(pixels,32,32,1,0,0);
    }
    /* Stencil reference is dynamic even when its immutable descriptor hits. */
    cache_rs[D3DRS_STENCILENABLE]=1;
    rhi_clear_depth(dsv,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,1,7);
    cache_rs[D3DRS_STENCILREF]=7;rhi_set_shader(RHI_STAGE_PIXEL,green);cache_draw_read(pixels);
    cache_pixel(pixels,32,32,0,1,0);
    cache_rs[D3DRS_STENCILREF]=6;rhi_set_shader(RHI_STAGE_PIXEL,red);cache_draw_read(pixels);
    cache_pixel(pixels,32,32,0,1,0);
    cache_rs[D3DRS_STENCILREF]=7;cache_draw_read(pixels);cache_pixel(pixels,32,32,1,0,0);
    cache_rs[D3DRS_STENCILENABLE]=0;
    /* Rectangles change independently of the cached scissor-enabled object. */
    cache_scissor_on=1;cache_scissor=(RhiRect){0,0,32,64};
    rhi_clear_color(rtv,blue);rhi_set_shader(RHI_STAGE_PIXEL,green);cache_draw_read(pixels);
    cache_pixel(pixels,16,32,0,1,0);cache_pixel(pixels,48,32,0,0,1);
    cache_scissor=(RhiRect){32,0,64,64};rhi_set_shader(RHI_STAGE_PIXEL,red);cache_draw_read(pixels);
    cache_pixel(pixels,16,32,0,1,0);cache_pixel(pixels,48,32,1,0,0);
    cache_scissor_on=0;
    /* Exceed blend/depth capacity, draw through replacements, then revisit A. */
    for(unsigned i=0;i<72;i++) {
        cache_rs[D3DRS_STENCILMASK]=i;
        cache_rs[D3DRS_SRCBLEND]=1+i%11;
        cache_rs[D3DRS_DESTBLEND]=1+(i/11)%8;
        cache_draw_read(pixels);cache_pixel(pixels,32,32,1,0,0);
    }
    cache_rs[D3DRS_SRCBLEND]=D3DBLEND_SRCALPHA;
    cache_rs[D3DRS_DESTBLEND]=D3DBLEND_INVSRCALPHA;
    cache_rs[D3DRS_ALPHABLENDENABLE]=1;
    rhi_clear_color(rtv,blue);cache_draw_read(pixels);cache_pixel(pixels,32,32,.5f,0,.5f);
    d3d8_states_shutdown();
    rhi_set_render_target(rtv,NULL);rhi_view_destroy(dsv);rhi_image_destroy(depth);
    rhi_shader_destroy(vs);rhi_shader_destroy(red);rhi_shader_destroy(green);
    puts("PASS: cached blend/stencil/scissor state pixels and replacement on common RHI");
}
#endif
