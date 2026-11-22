/* Pixel regressions for typed null resources; positive controls prevent a
 * permanently-zero shader or missing draw from passing the unbind check. */
static void gl_null_case(RhiView *target,unsigned type,unsigned layers,int cube,unsigned format,unsigned samples,
                         const char *source,const void *data,unsigned row,const float expected[4],const float missing[4])
{
    fprintf(stderr,"NULL case type=%u layers=%u cube=%d format=%u samples=%u\n",type,layers,cube,format,samples);
    RhiImageDesc d={0};d.type=type;d.width=d.height=d.mip_levels=1;d.depth=layers;d.cube=cube;d.samples=samples;
    d.format=format;d.bind=RHI_BIND_SAMPLED|(format==RHI_FORMAT_D32_FLOAT?RHI_BIND_DEPTH:RHI_BIND_RENDER_TARGET);
    RhiSubresourceData initial[6];for(unsigned i=0;i<6;i++)initial[i]=(RhiSubresourceData){data,row,row};
    RhiImage *image=rhi_image_create(&d,samples>1?NULL:initial);CHECK(image);
    if(samples>1){
        RhiViewDesc layer={0};layer.format=format;layer.dim=RHI_VIEW_DIM_2D;layer.mip_count=layer.layer_count=1;
        RhiView *color=rhi_view_create(image,RHI_VIEW_RENDER_TARGET,&layer);CHECK(color);
        rhi_clear_color(color,expected);rhi_view_destroy(color);
    }
    RhiView *view=rhi_view_create(image,RHI_VIEW_SAMPLED,NULL);CHECK(view);
    RhiShader *ps=shader(RHI_STAGE_PIXEL,source,"ps_5_0");rhi_set_shader(RHI_STAGE_PIXEL,ps);
    fprintf(stderr,"NULL positive\n");rhi_set_textures(0,1,&view);gl41_pixel(target,expected);
    fprintf(stderr,"NULL missing\n");rhi_set_textures(0,1,NULL);gl41_pixel(target,missing);
    rhi_set_textures(0,1,&view);gl41_pixel(target,expected);
    rhi_set_textures(0,1,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    rhi_shader_destroy(ps);rhi_view_destroy(view);rhi_image_destroy(image);
}
static void gl_null_resources_test(RhiView *target,int limited)
{
    RhiShader *vs=shader(RHI_STAGE_VERTEX,"float4 main(uint i:SV_VertexID):SV_POSITION{return float4(i==2?3:-1,i==1?3:-1,0,1);}","vs_5_0");
    rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);rhi_set_raster_state(raster);
    RhiViewport vp={0,0,64,64,0,1};rhi_set_viewports(1,&vp);
    RhiDepthDesc dd={0};dd.depth_func=RHI_CMP_ALWAYS;
    RhiDepthState *depth_state=rhi_depth_state_create(&dd);CHECK(depth_state);rhi_set_depth_state(depth_state,0);
    RhiBlendDesc bd={0};bd.write_mask=RHI_WRITE_ALL;bd.src=bd.src_alpha=RHI_BLEND_ONE;bd.dst=bd.dst_alpha=RHI_BLEND_ZERO;bd.op=bd.op_alpha=RHI_BLEND_OP_ADD;
    RhiBlendState *blend=rhi_blend_state_create(&bd);CHECK(blend);rhi_set_blend_state(blend,NULL,~0u);
    RhiSamplerDesc sd={0};sd.filter=RHI_FILTER_LINEAR;sd.address_u=sd.address_v=sd.address_w=RHI_ADDRESS_BORDER;
    sd.border[0]=sd.border[1]=sd.border[2]=sd.border[3]=1;sd.max_lod=1000;sd.max_anisotropy=1;
    RhiSampler *sampler=rhi_sampler_create(&sd);CHECK(sampler);rhi_set_samplers(0,1,&sampler);
    const float color[8]={.2f,.4f,.6f,1,.2f,.4f,.6f,1},zero[4]={0};
#define GL_NULL(t,l,c,f,n,h,data,row,expected,missing) gl_null_case(target,t,l,c,f,n,h,data,row,expected,missing)
#define FLOAT_SOURCE(kind,coord) "Texture" kind "<float4> t:register(t0);SamplerState s:register(s0);float4 main(float4 p:SV_POSITION):SV_TARGET{return t.SampleLevel(s," coord ",0);}"
    GL_NULL(RHI_IMAGE_2D,1,0,RHI_FORMAT_R32G32B32A32_FLOAT,1,FLOAT_SOURCE("2D","float2(.5,.5)"),color,16,color,zero);
    GL_NULL(RHI_IMAGE_2D,2,0,RHI_FORMAT_R32G32B32A32_FLOAT,1,FLOAT_SOURCE("2DArray","float3(.5,.5,1)"),color,16,color,zero);
    GL_NULL(RHI_IMAGE_2D,6,1,RHI_FORMAT_R32G32B32A32_FLOAT,1,FLOAT_SOURCE("Cube","float3(1,0,0)"),color,16,color,zero);
    GL_NULL(RHI_IMAGE_3D,2,0,RHI_FORMAT_R32G32B32A32_FLOAT,1,FLOAT_SOURCE("3D","float3(.5,.5,.5)"),color,16,color,zero);
    const unsigned uints[2]={7,7};const float uint_expected[4]={7,0,0,1};
    GL_NULL(RHI_IMAGE_2D,1,0,RHI_FORMAT_R32_UINT,1,"Texture2D<uint4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.Load(int3(0,0,0)));}",uints,4,uint_expected,zero);
    const int ints[8]={-1,2,-3,4,-1,2,-3,4};const float int_expected[4]={-1,2,-3,4};
    GL_NULL(RHI_IMAGE_3D,2,0,RHI_FORMAT_R32G32B32A32_SINT,1,"Texture3D<int4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.Load(int4(0,0,0,0)));}",ints,16,int_expected,zero);
    /* Border colors/linear filters must not change an unbound resource's zero. */
    RhiShader *ps=shader(RHI_STAGE_PIXEL,FLOAT_SOURCE("2D","float2(-10,-10)"),"ps_5_0");
    rhi_set_shader(RHI_STAGE_PIXEL,ps);gl41_pixel(target,zero);rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_shader_destroy(ps);
    /* Out-of-range loads are masked, not delegated to driver robustness. */
    ps=shader(RHI_STAGE_PIXEL,"Texture3D<int4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.Load(int4(9999,-3,17,30)));}","ps_5_0");
    rhi_set_shader(RHI_STAGE_PIXEL,ps);gl41_pixel(target,zero);rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_shader_destroy(ps);
    /* Missing one input must not zero a separate, valid input's contribution. */
    RhiImageDesc keep_desc={0};keep_desc.width=keep_desc.height=keep_desc.depth=keep_desc.mip_levels=keep_desc.samples=1;
    keep_desc.format=RHI_FORMAT_R32G32B32A32_FLOAT;keep_desc.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData keep_data={color,16,16};RhiImage *keep=rhi_image_create(&keep_desc,&keep_data);CHECK(keep);
    RhiView *keep_view=rhi_view_create(keep,RHI_VIEW_SAMPLED,NULL);CHECK(keep_view);rhi_set_textures(1,1,&keep_view);
    ps=shader(RHI_STAGE_PIXEL,"TextureCube<float4> a:register(t0);Texture2D<float4> b:register(t1);SamplerState s:register(s0);[noinline] float4 inner(float3 coord){return a.SampleLevel(s,coord,0);}[noinline] float4 outer(float2 coord){return inner(float3(1,0,0))+b.SampleLevel(s,coord,0);}float4 main(float4 p:SV_POSITION):SV_TARGET{return outer(float2(.5,.5));}","ps_5_0");
    rhi_set_shader(RHI_STAGE_PIXEL,ps);gl41_pixel(target,color);rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_shader_destroy(ps);
    rhi_set_textures(1,1,NULL);rhi_view_destroy(keep_view);rhi_image_destroy(keep);
    rhi_set_samplers(0,1,NULL);rhi_sampler_destroy(sampler);
    sd.filter=0x80;sd.compare=RHI_CMP_LESS;
    sampler=rhi_sampler_create(&sd);CHECK(sampler);rhi_set_samplers(0,1,&sampler);
    const float depth=.5f,shadow[4]={1,0,0,1},no_shadow[4]={0,0,0,1};
    GL_NULL(RHI_IMAGE_2D,1,0,RHI_FORMAT_D32_FLOAT,1,"Texture2D<float> t:register(t0);SamplerComparisonState s:register(s0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.SampleCmpLevelZero(s,float2(.5,.5),.25),0,0,1);}",&depth,4,shadow,no_shadow);
    rhi_set_samplers(0,1,NULL);rhi_sampler_destroy(sampler);
    if(!limited){
        GL_NULL(RHI_IMAGE_2D,1,0,RHI_FORMAT_R32G32B32A32_FLOAT,2,"Texture2DMS<float4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return t.Load(int2(0,0),0);}",NULL,0,color,zero);
        GL_NULL(RHI_IMAGE_2D,2,0,RHI_FORMAT_R32_UINT,2,"Texture2DMSArray<uint4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.Load(int3(0,0,0),0));}",NULL,0,uint_expected,zero);
    }
#undef FLOAT_SOURCE
#undef GL_NULL
    rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);rhi_depth_state_destroy(depth_state);rhi_blend_state_destroy(blend);
    puts("PASS: OpenGL typed null textures, stale-target isolation, integer/depth and sampler-independent zeros");
}
