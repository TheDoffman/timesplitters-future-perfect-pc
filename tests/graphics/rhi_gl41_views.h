/* Hardware tests of the4.1 single-sample compatibility resource path.
 * These do not claim MSAA coverage or native Mac/Linux driver acceptance. */
static void gl41_pixel(RhiView *target,const float expected[4])
{
    rhi_set_render_target(target,NULL);rhi_draw(3,0);
    float pixels[64*64*4];CHECK(!rhi_image_readback(rhi_view_image(target),0,pixels,64*16));
    for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<4;c++)if(!(fabsf(pixels[i*4+c]-expected[c])<.001f)) {
        fprintf(stderr,"pixel %u channel %u actual=%g expected=%g\n",i,c,pixels[i*4+c],expected[c]);CHECK(0);
    }
}
static void gl41_shader(RhiShader **current,const char *source)
{
    rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_shader_destroy(*current);
    *current=shader(RHI_STAGE_PIXEL,source,"ps_5_0");rhi_set_shader(RHI_STAGE_PIXEL,*current);
}
static void gl41_views_test(RhiView *target)
{
    CHECK(rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,1));
    CHECK(!rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,4));
    RhiShader *vs=shader(RHI_STAGE_VERTEX,"float4 main(uint i:SV_VertexID):SV_POSITION{return float4(i==2?3:-1,i==1?3:-1,0,1);}","vs_5_0"),*ps=NULL;
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);CHECK(raster);rhi_set_raster_state(raster);
    rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    RhiViewport vp={0,0,64,64,0,1};rhi_set_viewports(1,&vp);
    RhiImageDesc d={0};d.width=d.height=4;d.depth=2;d.mip_levels=2;d.samples=1;
    d.format=RHI_FORMAT_R32G32B32A32_FLOAT;d.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
    float data[4][4*4*4];RhiSubresourceData initial[4];
    const float first[4]={.2f,.4f,.6f,1},updated[4]={.8f,.1f,.25f,1},cleared[4]={.3f,.7f,.2f,1};
    for(unsigned sub=0;sub<4;sub++){
        unsigned size=4>>(sub%2);for(unsigned i=0;i<16;i++)memcpy(data[sub]+i*4,first,16);
        initial[sub]=(RhiSubresourceData){data[sub],size*16,size*size*16};
    }
    RhiImage *array=rhi_image_create(&d,initial);CHECK(array);
    RhiViewDesc v={0};v.format=d.format;v.base_mip=1;v.mip_count=1;v.base_layer=1;v.layer_count=1;v.dim=RHI_VIEW_DIM_2D_ARRAY;
    RhiView *view=rhi_view_create(array,RHI_VIEW_SAMPLED,&v);CHECK(view);rhi_set_textures(0,1,&view);
    gl41_shader(&ps,"Texture2DArray<float4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{uint w,h,a,m;t.GetDimensions(0,w,h,a,m);return w==2&&h==2&&a==1&&m==1?t.Load(int4(0,0,0,0)):float4(1,0,1,1);}");
    gl41_pixel(target,first);
    for(unsigned i=0;i<16;i++)memcpy(data[3]+i*4,updated,16);
    rhi_image_update(array,3,NULL,data[3],32,64);gl41_pixel(target,updated);
    v.dim=RHI_VIEW_DIM_2D;RhiView *rtv=rhi_view_create(array,RHI_VIEW_RENDER_TARGET,&v);CHECK(rtv);
    rhi_clear_color(rtv,cleared);gl41_pixel(target,cleared);rhi_view_destroy(rtv);
    /* Refreshing a later sampler must not overwrite an earlier array binding. */
    RhiImageDesc full=d;full.mip_levels=1;
    RhiSubresourceData two_layers[2]={initial[0],initial[2]};
    RhiImage *other=rhi_image_create(&full,two_layers);CHECK(other);
    RhiView *other_view=rhi_view_create(other,RHI_VIEW_SAMPLED,NULL);CHECK(other_view);
    RhiView *pair[2]={other_view,view};rhi_set_textures(0,2,pair);
    rhi_image_update(array,3,NULL,data[3],32,64);
    gl41_shader(&ps,"Texture2DArray<float4> a:register(t0);Texture2DArray<float4> b:register(t1);float4 main(float4 p:SV_POSITION):SV_TARGET{return .5*(a.Load(int4(0,0,0,0))+b.Load(int4(0,0,0,0)));}");
    float mixed[4];for(unsigned i=0;i<4;i++)mixed[i]=.5f*(first[i]+updated[i]);gl41_pixel(target,mixed);
    rhi_set_textures(0,2,NULL);rhi_view_destroy(other_view);rhi_image_destroy(other);
    rhi_view_destroy(view);rhi_image_destroy(array);

    /* A single cube face/mip becomes a2D view with the same logical rows. */
    d.cube=1;d.depth=6;RhiImage *cube=rhi_image_create(&d,NULL);CHECK(cube);
    rhi_image_update(cube,9,NULL,data[3],32,64);v.base_layer=4;
    view=rhi_view_create(cube,RHI_VIEW_SAMPLED,&v);CHECK(view);rhi_set_textures(0,1,&view);
    gl41_shader(&ps,"Texture2D<float4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return t.Load(int3(0,0,0));}");
    gl41_pixel(target,updated);rhi_set_textures(0,1,NULL);rhi_view_destroy(view);rhi_image_destroy(cube);

    d.type=RHI_IMAGE_3D;d.depth=4;d.cube=0;d.mip_levels=3;
    RhiImage *volume=rhi_image_create(&d,NULL);CHECK(volume);float volume_data[8*4];
    for(unsigned i=0;i<8;i++)memcpy(volume_data+i*4,cleared,16);
    rhi_image_update(volume,1,NULL,volume_data,32,64);
    v.base_layer=0;v.dim=RHI_VIEW_DIM_3D;
    view=rhi_view_create(volume,RHI_VIEW_SAMPLED,&v);CHECK(view);rhi_set_textures(0,1,&view);
    gl41_shader(&ps,"Texture3D<float4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return t.Load(int4(0,0,1,0));}");
    gl41_pixel(target,cleared);rhi_set_textures(0,1,NULL);rhi_view_destroy(view);rhi_image_destroy(volume);

    d.type=RHI_IMAGE_2D;d.depth=1;d.mip_levels=2;d.format=RHI_FORMAT_D24_UNORM_S8_UINT;d.bind=RHI_BIND_DEPTH|RHI_BIND_SAMPLED;
    RhiImage *depth=rhi_image_create(&d,NULL);CHECK(depth);
    RhiView *dsv=rhi_view_create(depth,RHI_VIEW_DEPTH,NULL);CHECK(dsv);
    v=(RhiViewDesc){0};v.format=RHI_FORMAT_X24_TYPELESS_G8_UINT;v.mip_count=v.layer_count=1;v.dim=RHI_VIEW_DIM_2D;
    view=rhi_view_create(depth,RHI_VIEW_SAMPLED,&v);CHECK(view);rhi_set_textures(0,1,&view);
    gl41_shader(&ps,"Texture2D<uint> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.Load(int3(0,0,0))/255.0,0,0,1);}");
    rhi_clear_depth(dsv,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,.5f,37);
    float stencil[4]={37/255.f,0,0,1};gl41_pixel(target,stencil);
    rhi_clear_depth(dsv,RHI_CLEAR_STENCIL,1,203);stencil[0]=203/255.f;gl41_pixel(target,stencil);
    rhi_set_textures(0,1,NULL);rhi_view_destroy(view);rhi_view_destroy(dsv);
    v.format=d.format;v.base_mip=1;
    view=rhi_view_create(depth,RHI_VIEW_SAMPLED,&v);CHECK(view);
    dsv=rhi_view_create(depth,RHI_VIEW_DEPTH,&v);CHECK(dsv);
    rhi_clear_depth(dsv,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,.375f,5);rhi_set_textures(0,1,&view);
    gl41_shader(&ps,"Texture2D<float> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return float4(t.Load(int3(0,0,0)),0,0,1);}");
    const float sampled_depth[4]={.375f,0,0,1};gl41_pixel(target,sampled_depth);
    rhi_set_textures(0,1,NULL);rhi_view_destroy(view);rhi_view_destroy(dsv);rhi_image_destroy(depth);

    /* Compressed array slice copies retain blocks and invalidate after upload. */
    d.format=RHI_FORMAT_BC1_UNORM;d.depth=2;d.mip_levels=1;d.bind=RHI_BIND_SAMPLED;
    unsigned char blocks[2][8]={{0,0xf8,0,0,0,0,0,0},{0xe0,7,0,0,0,0,0,0}};
    RhiSubresourceData bc[2]={{blocks[0],8,8},{blocks[1],8,8}};
    RhiImage *compressed=rhi_image_create(&d,bc);CHECK(compressed);
    v.format=d.format;v.base_mip=0;v.base_layer=1;view=rhi_view_create(compressed,RHI_VIEW_SAMPLED,&v);CHECK(view);rhi_set_textures(0,1,&view);
    gl41_shader(&ps,"Texture2D<float4> t:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return t.Load(int3(0,0,0));}");
    const float green[4]={0,1,0,1},blue[4]={0,0,1,1};gl41_pixel(target,green);
    blocks[1][0]=31;blocks[1][1]=0;rhi_image_update(compressed,1,NULL,blocks[1],8,8);gl41_pixel(target,blue);
    rhi_set_textures(0,1,NULL);rhi_view_destroy(view);rhi_image_destroy(compressed);
    rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_set_shader(RHI_STAGE_VERTEX,NULL);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    puts("PASS: OpenGL4.1 subset/array/cube/volume/stencil/compressed views and update invalidation; single-sample only");
}
