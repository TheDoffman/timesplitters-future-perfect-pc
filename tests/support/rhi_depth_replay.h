#ifndef TSFP_RHI_DEPTH_REPLAY_TEST_H
#define TSFP_RHI_DEPTH_REPLAY_TEST_H
#include "graphics/interpolation_depth.h"
/* Uses the fixture's shader(), target/rtv and fixed 64x64 viewport. */
static void rhi_depth_replay_test(RhiView *rtv) {
    TsfpReplayDepth cache={0};
    RhiImageDesc desc={0};desc.type=RHI_IMAGE_2D;desc.width=desc.height=64;
    desc.depth=desc.mip_levels=desc.samples=1;desc.format=RHI_FORMAT_D24_UNORM_S8_UINT;
    desc.bind=RHI_BIND_DEPTH;
    RhiImage *guest=rhi_image_create(&desc,NULL);CHECK(guest);
    RhiView *guest_view=rhi_view_create(guest,RHI_VIEW_DEPTH,NULL);CHECK(guest_view);
    RhiShader *far_shader=shader(RHI_STAGE_VERTEX,
        "float4 main(uint id:SV_VertexID):SV_POSITION {return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,.5,1);}","vs_5_0");
    RhiShader *near_shader=shader(RHI_STAGE_VERTEX,
        "float4 main(uint id:SV_VertexID):SV_POSITION {return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,.1,1);}","vs_5_0");
    RhiShader *green=shader(RHI_STAGE_PIXEL,"float4 main():SV_TARGET{return float4(0,1,0,1);}","ps_5_0");
    RhiDepthDesc state={0};state.depth_enable=state.depth_write=1;state.depth_func=RHI_CMP_LESS;
    state.stencil_enable=1;state.stencil_read_mask=state.stencil_write_mask=255;
    state.front.fail=state.front.depth_fail=state.front.pass=RHI_STENCIL_KEEP;
    state.front.func=RHI_CMP_EQUAL;state.back=state.front;
    RhiDepthState *depth=rhi_depth_state_create(&state);CHECK(depth);
    rhi_set_shader(RHI_STAGE_VERTEX,far_shader);rhi_set_shader(RHI_STAGE_PIXEL,green);
    const float black[4]={0,0,0,1};float pixels[64*64*4];
    rhi_clear_depth(guest_view,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,.25f,7);
    RhiView *previous=NULL;
    for(int repeat=0;repeat<2;repeat++) {
        tsfp_replay_depth_begin(&cache);
        RhiView *scratch=tsfp_replay_depth_view(&cache,guest,0);CHECK(scratch && scratch!=guest_view);
        if(previous)CHECK(scratch==previous);previous=scratch;
        rhi_set_render_target(rtv,scratch);rhi_set_depth_state(depth,0);rhi_clear_color(rtv,black);
        rhi_draw(3,0);CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        CHECK(pixels[1]>.99f); /* fresh depth/stencil, including the second replay */
        rhi_clear_depth(scratch,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,.1f,3);
        CHECK(tsfp_replay_depth_view(&cache,guest,0)==scratch);
        rhi_set_depth_state(depth,3);rhi_clear_color(rtv,black);rhi_draw(3,0);
        CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        CHECK(pixels[1]<.01f); /* lookup must not clear existing replay depth */
        rhi_set_render_target(rtv,guest_view);tsfp_replay_depth_end(&cache);
        rhi_set_depth_state(depth,7);rhi_clear_color(rtv,black);rhi_draw(3,0);
        CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        CHECK(pixels[1]<.01f); /* guest depth .25 survived */
    }
    rhi_set_shader(RHI_STAGE_VERTEX,near_shader);rhi_draw(3,0);
    CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
    CHECK(pixels[1]>.99f); /* guest stencil 7 survived */
    rhi_set_render_target(rtv,NULL);tsfp_replay_depth_destroy(&cache);
    rhi_view_destroy(guest_view);rhi_image_destroy(guest);rhi_depth_state_destroy(depth);
    rhi_shader_destroy(far_shader);rhi_shader_destroy(near_shader);rhi_shader_destroy(green);
    puts("PASS: shared RHI interpolation depth/stencil isolation and reuse");
}
#endif
