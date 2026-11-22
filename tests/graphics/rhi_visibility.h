/* Asset-free exact-sample acceptance, shared by native GPU fixtures. */
static void test_visibility_queries(void) {
    RhiShader *vs=shader(RHI_STAGE_VERTEX,
        "float4 main(uint id:SV_VertexID):SV_POSITION {return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,.5,1);}","vs_5_0");
    RhiShader *solid=shader(RHI_STAGE_PIXEL,"float4 main():SV_TARGET{return float4(0,1,0,1);}","ps_5_0");
    RhiShader *cutout=shader(RHI_STAGE_PIXEL,
        "float4 main(float4 p:SV_POSITION):SV_TARGET{if(p.x<32)discard;return float4(0,1,0,1);}","ps_5_0");
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);
    RhiDepthDesc dd={0};dd.depth_enable=1;dd.depth_func=RHI_CMP_LESS;dd.depth_write=0;
    RhiDepthState *depth=rhi_depth_state_create(&dd);CHECK(depth);
    RhiBlendDesc bd={0};bd.write_mask=0;
    RhiBlendState *blend=rhi_blend_state_create(&bd);CHECK(blend);
    for(uint32_t samples=1;samples<=4;samples*=4) {
        if(!rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,samples) ||
           !rhi_sample_count_supported(RHI_FORMAT_D32_FLOAT,samples))continue;
        RhiImageDesc desc={0};desc.type=RHI_IMAGE_2D;desc.width=desc.height=64;
        desc.depth=desc.mip_levels=1;desc.samples=samples;desc.format=RHI_FORMAT_R8G8B8A8_UNORM;
        desc.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *color=rhi_image_create(&desc,NULL);CHECK(color);
        RhiView *rt=rhi_view_create(color,RHI_VIEW_RENDER_TARGET,NULL);CHECK(rt);
        desc.format=RHI_FORMAT_D32_FLOAT;desc.bind=RHI_BIND_DEPTH;
        RhiImage *z=rhi_image_create(&desc,NULL);CHECK(z);
        RhiView *ds=rhi_view_create(z,RHI_VIEW_DEPTH,NULL);CHECK(ds);
        desc.samples=1;desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *read=rhi_image_create(&desc,NULL);CHECK(read);
        RhiViewport vp={0,0,64,64,0,1};
        rhi_set_render_target(rt,ds);rhi_set_viewports(1,&vp);rhi_set_raster_state(raster);
        rhi_set_depth_state(depth,0);rhi_set_blend_state(blend,NULL,~0u);
        rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);rhi_set_vertex_layout(NULL);
        rhi_set_shader(RHI_STAGE_VERTEX,vs);
        RhiOcclusion *q=rhi_occlusion_create();CHECK(q);
        uint64_t result=123;CHECK(rhi_occlusion_result(q,&result)==-1 && result==123);
        for(int mode=0;mode<4;mode++) {
            rhi_clear_depth(ds,RHI_CLEAR_DEPTH,mode==0?.25f:.75f,0);
            rhi_set_shader(RHI_STAGE_PIXEL,mode==2?cutout:solid);
            CHECK(rhi_occlusion_begin(q));rhi_draw(3,0);
            if(mode==3)rhi_draw(3,0);
            CHECK(rhi_occlusion_end(q));
            uint64_t sentinel=1234;int status=rhi_occlusion_result(q,&sentinel);
            CHECK(status==0 || status==1);if(!status)CHECK(sentinel==1234);
            if(samples>1)CHECK(rhi_image_resolve_subresource(color,0,read,0)==0);
            unsigned char pixels[64*64*4];
            CHECK(rhi_image_readback(samples>1?read:color,0,pixels,64*4)==0);
            CHECK(rhi_occlusion_result(q,&result)==1);
            CHECK(result==(uint64_t)(mode==0?0:mode==2?2048:mode==3?8192:4096)*samples);
        }
        rhi_occlusion_destroy(q);rhi_set_render_target(NULL,NULL);
        rhi_view_destroy(ds);rhi_image_destroy(z);rhi_view_destroy(rt);rhi_image_destroy(color);rhi_image_destroy(read);
    }
    rhi_shader_destroy(vs);rhi_shader_destroy(solid);rhi_shader_destroy(cutout);
    rhi_raster_state_destroy(raster);rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);
    puts("PASS: exact GPU visibility, wall occlusion, cutout, multiple draws, query reuse and MSAA counts");
}
