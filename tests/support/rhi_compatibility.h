/* Real pixels for pre-maintenance4 interfaces and multi-pool descriptor lifetime. */
static void rhi_compatibility_test(RhiView *rtv) {
    RhiShader *vs=shader(RHI_STAGE_VERTEX,
        "struct O{float4 p:SV_POSITION;float4 v:TEXCOORD0;};"
        "O main(uint id:SV_VertexID){O o;o.p=float4(id==2?3.0:-1.0,id==1?3.0:-1.0,0,1);"
        "o.v=float4(.2,.4,.6,.8);return o;}","vs_5_0");
    const char *sources[]={
        "float4 main(float4 p:SV_POSITION,float v:TEXCOORD0):SV_TARGET{return float4(v,0,0,1);}",
        "float4 main(float4 p:SV_POSITION,float2 v:TEXCOORD0):SV_TARGET{return float4(v,0,1);}",
        "float4 main(float4 p:SV_POSITION,float3 v:TEXCOORD0):SV_TARGET{return float4(v,1);}"};
    float pixels[64*64*4];
    rhi_set_shader(RHI_STAGE_VERTEX,vs);
    for(unsigned width=1;width<=3;width++) {
        RhiShader *ps=shader(RHI_STAGE_PIXEL,sources[width-1],"ps_5_0");
        rhi_set_shader(RHI_STAGE_PIXEL,ps);rhi_draw(3,0);
        CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        for(unsigned c=0;c<3;c++)CHECK(fabsf(pixels[c]-(c<width?.2f*(c+1):0))<.001f);
        CHECK(fabsf(pixels[3]-1)<.001f);rhi_shader_destroy(ps);
    }
    RhiShader *ps=shader(RHI_STAGE_PIXEL,
        "cbuffer Q:register(b0){float4 q;}float4 main():SV_TARGET{return q;}","ps_5_0");
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;rd.scissor=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);
    rhi_set_shader(RHI_STAGE_PIXEL,ps);rhi_set_raster_state(raster);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&parameters);
    /* D3D11 default buffers support update(), while discard mapping requires
     * a dynamic CPU-write buffer. Exercise both legal contracts on every RHI. */
    RhiBufferDesc mapped_desc={16,RHI_USAGE_DYNAMIC,RHI_BIND_UNIFORM,RHI_CPU_WRITE};
    RhiBuffer *mapped_parameters=rhi_buffer_create(&mapped_desc,NULL);CHECK(mapped_parameters);
    /* No readback/flush between these draws. Four 128-set pools must coexist;
     * each one-pixel draw must retain its own uniform snapshot. Repeat across
     * frame reuse so resets cannot race the previous GPU submission. */
    for(unsigned pass=0;pass<6;pass++) {
        const float black[4]={0,0,0,1};rhi_clear_color(rtv,black);
        for(unsigned i=0;i<512;i++) {
            float q[4]={(i+1)/512.f,pass/8.f,.25f,1};
            RhiRect rect={(int)(i%64),(int)(i/64),(int)(i%64+1),(int)(i/64+1)};
            rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&parameters);
            rhi_set_scissor(&rect);rhi_buffer_update(parameters,q);rhi_buffer_update(parameters,q);rhi_draw(3,0);
            rhi_buffer_update(parameters,q);rhi_draw(3,0);
            /* Actual game constants use map/unmap, not only update(). */
            rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&mapped_parameters);
            for(unsigned repeat=0;repeat<2;repeat++) {
                void *mapped=rhi_buffer_map(mapped_parameters,RHI_MAP_WRITE_DISCARD);CHECK(mapped);
                memcpy(mapped,q,sizeof q);rhi_buffer_unmap(mapped_parameters);rhi_draw(3,0);
            }
        }
        CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        for(unsigned i=0;i<512;i++) {
            CHECK(fabsf(pixels[i*4]-(i+1)/512.f)<.001f);
            CHECK(fabsf(pixels[i*4+1]-pass/8.f)<.001f);
            CHECK(fabsf(pixels[i*4+2]-.25f)<.001f);
        }
    }
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&parameters);
    rhi_buffer_destroy(mapped_parameters);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    puts("PASS: shared RHI varying widths 1/2/3, 512 immutable descriptor draws and six arena reuses");
}
