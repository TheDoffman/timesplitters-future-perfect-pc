/* Real stream offsets, normalized attributes and indexed base vertices. */
static void rhi_vertex_streams_test(RhiView *output)
{
    RhiShader *vs=shader(RHI_STAGE_VERTEX,
        "struct I{float2 p:POSITION;float4 color:COLOR0;};struct O{float4 p:SV_POSITION;float4 color:TEXCOORD0;};"
        "O main(I i){O o;o.p=float4(i.p,.5,1);o.color=i.color;return o;}","vs_5_0");
    RhiShader *ps=shader(RHI_STAGE_PIXEL,"float4 main(float4 position:SV_POSITION,float4 color:TEXCOORD0):SV_TARGET{return color;}","ps_5_0");
    float positions[12]={0,0,0,0, -9,-9, -1,-1, -1,3, 3,-1};
    unsigned char colors[16]={0,0,0,0, 200,100,40,255, 200,100,40,255, 200,100,40,255};
    RhiBufferDesc desc={sizeof positions,RHI_USAGE_DYNAMIC,RHI_BIND_VERTEX,RHI_CPU_WRITE};
    RhiBuffer *position=rhi_buffer_create(&desc,NULL);CHECK(position);
    void *mapped=rhi_buffer_map(position,RHI_MAP_WRITE_DISCARD);CHECK(mapped);memcpy(mapped,positions,sizeof positions);rhi_buffer_unmap(position);
    desc=(RhiBufferDesc){sizeof colors,RHI_USAGE_DEFAULT,RHI_BIND_VERTEX,0};
    RhiBuffer *color=rhi_buffer_create(&desc,colors);CHECK(color);
    RhiVertexElement elements[]={{"POSITION",0,RHI_FORMAT_R32G32_FLOAT,1,RHI_APPEND},{"COLOR",0,RHI_FORMAT_R8G8B8A8_UNORM,2,RHI_APPEND}};
    RhiVertexLayout *layout=rhi_vertex_layout_create(elements,2,vs);CHECK(layout);
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);RhiDepthDesc dd={0};RhiDepthState *depth=rhi_depth_state_create(&dd);
    RhiBlendDesc bd={0};bd.write_mask=RHI_WRITE_ALL;RhiBlendState *blend=rhi_blend_state_create(&bd);
    CHECK(raster&&depth&&blend);rhi_set_render_target(output,NULL);
    RhiViewport vp={0,0,64,64,0,1};rhi_set_viewports(1,&vp);rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
    rhi_set_vertex_layout(layout);rhi_set_vertex_buffer(1,position,8,16);rhi_set_vertex_buffer(2,color,4,0);
    rhi_set_raster_state(raster);rhi_set_depth_state(depth,0);rhi_set_blend_state(blend,NULL,~0u);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_draw(3,1);
    {float probe[64*64*4];CHECK(rhi_image_readback(target,0,probe,64*4*sizeof(float))==0);
        for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<4;c++)CHECK(fabsf(probe[i*4+c]-colors[4+c]/255.f)<.0001f);}

    const unsigned short indices16[5]={99,99,0,1,2};const unsigned indices32[5]={99,99,0,1,2};
    for(unsigned bits=16;bits<=32;bits+=16) {
        desc=(RhiBufferDesc){bits==16?sizeof indices16:sizeof indices32,RHI_USAGE_DEFAULT,RHI_BIND_INDEX,0};
        RhiBuffer *indices=rhi_buffer_create(&desc,bits==16?(const void *)indices16:(const void *)indices32);CHECK(indices);
        rhi_set_index_buffer(indices,bits,bits/8);rhi_draw_indexed(3,1,1);
        float pixels[64*64*4];CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<4;c++) {
            if(!(fabsf(pixels[i*4+c]-colors[4+c]/255.f)<.0001f))fprintf(stderr,"Vertex stream %u-bit pixel %u c%u: %.8f expected %.8f\n",bits,i,c,pixels[i*4+c],colors[4+c]/255.f);
            CHECK(fabsf(pixels[i*4+c]-colors[4+c]/255.f)<.0001f);
        }
        rhi_set_index_buffer(NULL,16,0);rhi_buffer_destroy(indices);
    }
    /* The Xbox declaration's D3DCOLOR uses BGRA bytes, not ordinary RGBA.
     * Keep the shader unchanged: the vertex format owns the channel ordering. */
    elements[1].format=RHI_FORMAT_B8G8R8A8_UNORM;
    RhiVertexLayout *bgra=rhi_vertex_layout_create(elements,2,vs);CHECK(bgra);
    rhi_set_vertex_layout(bgra);rhi_draw(3,1);
    {float pixels[64*64*4];const unsigned channels[4]={2,1,0,3};
        CHECK(!rhi_image_readback(target,0,pixels,64*4*sizeof(float)));
        for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<4;c++) {
            if(fabsf(pixels[i*4+c]-colors[4+channels[c]]/255.f)>=.0001f)
                fprintf(stderr,"BGRA vertex pixel%u c%u: %.8f expected %.8f\n",i,c,pixels[i*4+c],colors[4+channels[c]]/255.f);
            CHECK(fabsf(pixels[i*4+c]-colors[4+channels[c]]/255.f)<.0001f);
        }
    }
    rhi_set_vertex_layout(NULL);rhi_vertex_layout_destroy(bgra);
    puts("PASS: packed BGRA vertex colors preserve shader channel ordering");
    rhi_set_vertex_buffer(1,NULL,0,0);rhi_set_vertex_buffer(2,NULL,0,0);
    rhi_vertex_layout_destroy(layout);rhi_buffer_destroy(position);rhi_buffer_destroy(color);
    rhi_raster_state_destroy(raster);rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);
    rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_shader_destroy(vs);rhi_shader_destroy(ps);
    puts("PASS: vertex stream offsets, normalized attributes, dynamic mapping and 16/32-bit indexed base vertices");
}
static void rhi_raw_storage_test(void)
{
    RhiImageDesc d={0};d.width=d.height=2;d.depth=d.mip_levels=d.samples=1;
    d.format=RHI_FORMAT_D24_UNORM_S8_UINT;d.bind=RHI_BIND_DEPTH|RHI_BIND_SAMPLED;
    uint32_t values[4]={0x7f123456,0x80800000,0x01ffffff,0xff000000},got[4]={0};
    RhiSubresourceData data={values,8,16};RhiImage *image=rhi_image_create(&d,&data);CHECK(image);
    int read=rhi_image_readback(image,0,got,8);
    if(strcmp(rhi_backend_name(),"vulkan") && (read || memcmp(got,values,sizeof got)))fprintf(stderr,"Packed depth read %d: %08x %08x %08x %08x\n",read,got[0],got[1],got[2],got[3]);
    /* Vulkan's existing readback door explicitly rejects depth resources;
     * its sampled depth/stencil pixels are verified separately below. */
    if(!strcmp(rhi_backend_name(),"vulkan"))CHECK(read==-1);
    else CHECK(read==0 && !memcmp(got,values,sizeof got));
    rhi_image_destroy(image);
    d.width=d.height=4;d.depth=2;d.format=RHI_FORMAT_BC1_UNORM;d.bind=RHI_BIND_SAMPLED;
    const unsigned char blocks[2][8]={{0,248,0,0,0,0,0,0},{224,7,0,0,0,0,0,0}};
    RhiSubresourceData faces[2]={{blocks[0],8,8},{blocks[1],8,8}};
    image=rhi_image_create(&d,faces);CHECK(image);
    for(unsigned layer=0;layer<2;layer++){unsigned char bytes[8]={0};CHECK(rhi_image_readback(image,layer,bytes,8)==0 && !memcmp(bytes,blocks[layer],8));}
    rhi_image_destroy(image);puts("PASS: compressed array-layer bytes and depth readback capability/packing");
}
static void rhi_facing_test(RhiView *output)
{
    RhiShader *vs=shader(RHI_STAGE_VERTEX,"float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id==2?3:-1,id==1?3:-1,.5,1);}","vs_5_0");
    RhiShader *ps=shader(RHI_STAGE_PIXEL,"float4 main():SV_TARGET{return float4(1,0,0,1);}","ps_5_0");
    RhiDepthDesc dd={0};RhiDepthState *depth=rhi_depth_state_create(&dd);CHECK(depth);
    RhiBlendDesc bd={0};bd.write_mask=15;RhiBlendState *blend=rhi_blend_state_create(&bd);CHECK(blend);
    RhiViewport vp={0,0,64,64,0,1};rhi_set_render_target(output,NULL);rhi_set_viewports(1,&vp);
    rhi_set_vertex_layout(NULL);rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
    rhi_set_depth_state(depth,0);rhi_set_blend_state(blend,NULL,~0u);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    for(unsigned front=0;front<2;front++)for(unsigned back=0;back<2;back++) {
        RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=back?RHI_CULL_BACK:RHI_CULL_FRONT;rd.front_ccw=front;rd.depth_clip=1;
        RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);rhi_set_raster_state(raster);
        const float black[4]={0,0,0,1};rhi_clear_color(output,black);rhi_draw(3,0);
        float pixels[64*64*4];CHECK(!rhi_image_readback(target,0,pixels,64*4*sizeof(float)));
        int visible=(!front && back)||(front && !back);
        if(fabsf(pixels[0]-(float)visible)>.001f)fprintf(stderr,"Facing front_ccw %u cull_back %u: %.3f expected %d\n",front,back,pixels[0],visible);
        CHECK(fabsf(pixels[0]-(float)visible)<.001f);rhi_raster_state_destroy(raster);
    }
    rhi_shader_destroy(vs);rhi_shader_destroy(ps);rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);
    puts("PASS: D3D winding, front/back culling and viewport orientation");
}
static void rhi_sampler_address_test(RhiView *output)
{
    RhiShader *vs=shader(RHI_STAGE_VERTEX,"float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id==2?3:-1,id==1?3:-1,0,1);}","vs_5_0");
    RhiShader *ps=shader(RHI_STAGE_PIXEL,"cbuffer Q:register(b0){float4 q;}Texture2D image:register(t0);SamplerState s:register(s0);float4 main(float4 p:SV_POSITION):SV_TARGET{return image.SampleLevel(s,float2(q.x,.5),0);}","ps_5_0");
    RhiImageDesc d={0};d.width=2;d.height=d.depth=d.mip_levels=d.samples=1;d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.bind=RHI_BIND_SAMPLED;
    unsigned char colors[8]={255,0,0,255,0,255,0,255};RhiSubresourceData data={colors,8,8};
    RhiImage *image=rhi_image_create(&d,&data);CHECK(image);RhiView *view=rhi_view_create(image,RHI_VIEW_SAMPLED,NULL);CHECK(view);
    RhiBufferDesc bd={16,RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};RhiBuffer *query=rhi_buffer_create(&bd,NULL);CHECK(query);
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);RhiDepthDesc dd={0};RhiDepthState *depth=rhi_depth_state_create(&dd);
    RhiBlendDesc bl={0};bl.write_mask=15;RhiBlendState *blend=rhi_blend_state_create(&bl);CHECK(raster&&depth&&blend);
    rhi_set_render_target(output,NULL);RhiViewport vp={0,0,64,64,0,1};rhi_set_viewports(1,&vp);
    rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
    rhi_set_raster_state(raster);rhi_set_depth_state(depth,0);rhi_set_blend_state(blend,NULL,~0u);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&query);rhi_set_textures(0,1,&view);
    const unsigned expected[5][2]={{0,0},{1,1},{0,1},{2,2},{1,1}}; /* red, green, blue */
    for(unsigned mode=1;mode<=5;mode++) {
        RhiSamplerDesc sd={0};sd.address_u=mode;sd.address_v=sd.address_w=RHI_ADDRESS_CLAMP;sd.compare=RHI_CMP_ALWAYS;sd.max_lod=1000;sd.max_anisotropy=1;sd.border[2]=sd.border[3]=1;
        RhiSampler *sampler=rhi_sampler_create(&sd);CHECK(sampler);rhi_set_samplers(0,1,&sampler);
        for(unsigned side=0;side<2;side++) {
            float q[4]={side?1.25f:-.75f,0,0,0};rhi_buffer_update(query,q);rhi_draw(3,0);
            float pixels[64*64*4];CHECK(!rhi_image_readback(target,0,pixels,64*4*sizeof(float)));
            for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<4;c++)CHECK(fabsf(pixels[i*4+c]-(float)(c==3 || c==expected[mode-1][side]))<.0001f);
        }
        RhiSampler *none=NULL;rhi_set_samplers(0,1,&none);rhi_sampler_destroy(sampler);
    }
    RhiBuffer *no_buffer=NULL;RhiView *no_view=NULL;rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&no_buffer);rhi_set_textures(0,1,&no_view);
    rhi_view_destroy(view);rhi_image_destroy(image);rhi_buffer_destroy(query);rhi_shader_destroy(vs);rhi_shader_destroy(ps);
    rhi_raster_state_destroy(raster);rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);
    puts("PASS: wrap, mirror, clamp, border and mirror-once sampler pixels");
}
