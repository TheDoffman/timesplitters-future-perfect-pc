/* Switching color targets must not clear or substitute their shared depth. */
static void test_msaa_shared_depth(void)
{
    const char *text="struct O{float4 p:SV_POSITION;};O vs(uint id:SV_VertexID){"
      "float2 q=float2((id<<1)&2,id&2);O o;o.p=float4(q*float2(2,-2)+float2(-1,1),0,1);return o;}"
      "struct P{float4 color:SV_TARGET;float z:SV_Depth;};"
      "P front(O i){P o;o.color=float4(1,0,0,1);o.z=.25;return o;}"
      "P back(O i){P o;o.color=float4(0,0,1,1);o.z=.75;return o;}";
    char error[2048];RhiShaderSource code={text,strlen(text),"shared-ms-depth",NULL,"vs","vs_5_0",1};
    RhiShader *vs=rhi_shader_create(RHI_STAGE_VERTEX,&code,error,sizeof error);CHECK(vs);
    code.entry="front";code.target="ps_5_0";
    RhiShader *front=rhi_shader_create(RHI_STAGE_PIXEL,&code,error,sizeof error);CHECK(front);
    code.entry="back";RhiShader *back=rhi_shader_create(RHI_STAGE_PIXEL,&code,error,sizeof error);CHECK(back);
    RhiDepthDesc dd={0};dd.depth_enable=dd.depth_write=1;dd.depth_func=RHI_CMP_LESS_EQUAL;
    RhiDepthState *depth=rhi_depth_state_create(&dd);CHECK(depth);
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.multisample=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);
    RhiBlendDesc bd={0};bd.write_mask=RHI_WRITE_ALL;RhiBlendState *blend=rhi_blend_state_create(&bd);CHECK(blend);
    unsigned exercised=0;
    for(unsigned requested=2;requested<=8;requested*=2){
        TsfpMsaaTargets scene={0};
        CHECK(tsfp_msaa_targets_create(&scene,16,16,RHI_FORMAT_R8G8B8A8_UNORM,RHI_FORMAT_D24_UNORM_S8_UINT,requested));
        /* Vulkan devices may support 4x but not 2x. Validate the documented
         * requested-or-lower fallback, and exercise shared depth where available. */
        unsigned expected=requested;
        while(expected>1 && (!rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,expected) ||
                             !rhi_sample_count_supported(RHI_FORMAT_D24_UNORM_S8_UINT,expected)))expected/=2;
        CHECK(scene.samples==expected);
        if(scene.samples==1){tsfp_msaa_targets_destroy(&scene);continue;}
        exercised++;
        RhiImageDesc d={0};d.type=RHI_IMAGE_2D;d.width=d.height=16;d.depth=d.mip_levels=d.samples=1;
        d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
        RhiImage *original=rhi_image_create(&d,NULL);CHECK(original);
        TsfpMsaaColorSurface offscreen={0};CHECK(tsfp_msaa_color_create(&offscreen,original,0,scene.samples));
        RhiViewport vp={0,0,16,16,0,1};const float green[4]={0,1,0,1};
        rhi_set_viewports(1,&vp);rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
        rhi_set_raster_state(raster);rhi_set_depth_state(depth,0);rhi_set_blend_state(blend,NULL,~0u);
        rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,front);
        rhi_set_render_target(scene.rtv,scene.dsv);rhi_clear_depth(scene.dsv,RHI_CLEAR_DEPTH,1,0);rhi_draw(3,0);
        rhi_set_shader(RHI_STAGE_PIXEL,back);
        rhi_set_render_target(offscreen.rtv,scene.dsv);rhi_clear_color(offscreen.rtv,green);rhi_draw(3,0);
        rhi_set_render_target(scene.rtv,scene.dsv);rhi_draw(3,0);
        CHECK(tsfp_msaa_targets_sample(&scene));CHECK(tsfp_msaa_color_resolve(&offscreen));
        unsigned char pixels[16*16*4];
        CHECK(rhi_image_readback(rhi_view_image(scene.srv),0,pixels,16*4)==0);
        for(unsigned i=0;i<16*16;i++)CHECK(pixels[i*4]==255&&pixels[i*4+2]==0);
        CHECK(rhi_image_readback(original,0,pixels,16*4)==0);
        for(unsigned i=0;i<16*16;i++)CHECK(pixels[i*4+1]==255&&pixels[i*4+2]==0);
        /* The second color target must see an explicit depth clear, too. */
        rhi_clear_depth(scene.dsv,RHI_CLEAR_DEPTH,1,0);
        rhi_set_render_target(offscreen.rtv,scene.dsv);rhi_draw(3,0);
        CHECK(tsfp_msaa_color_resolve(&offscreen));
        CHECK(rhi_image_readback(original,0,pixels,16*4)==0);
        for(unsigned i=0;i<16*16;i++)CHECK(pixels[i*4+1]==0&&pixels[i*4+2]==255);
        rhi_set_render_target(NULL,NULL);tsfp_msaa_color_destroy(&offscreen);
        rhi_image_destroy(original);tsfp_msaa_targets_destroy(&scene);
    }
    CHECK(exercised>0);
    rhi_shader_destroy(vs);rhi_shader_destroy(front);rhi_shader_destroy(back);
    rhi_depth_state_destroy(depth);rhi_raster_state_destroy(raster);rhi_blend_state_destroy(blend);
    puts("PASS: MSAA shared depth survives scene/offscreen switches and resolves");
}
