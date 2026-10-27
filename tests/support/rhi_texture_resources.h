/* One asset-free resource/sampling contract for D3D11 and Vulkan. Caller has
 * bound a fullscreen triangle, 64x64 float target, and disabled depth/blend. */
#ifndef TSFP_RHI_TEXTURE_RESOURCES_H
#define TSFP_RHI_TEXTURE_RESOURCES_H

static void texture_expect(RhiBuffer *query_buffer,const float query[4],const float expected[4]) {
    float pixels[64*64*4];
    rhi_buffer_update(query_buffer,query);rhi_draw(3,0);
    CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
    for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<4;c++) {
        /* Negated success also rejects NaNs from a broken shader/readback. */
        if(!(fabsf(pixels[i*4+c]-expected[c])<=.0001f)) {
            fprintf(stderr,"Texture query %.3f,%.3f,%.3f lod %.1f pixel %u channel %u: %.5f expected %.5f\n",
                query[0],query[1],query[2],query[3],i,c,pixels[i*4+c],expected[c]);
            CHECK(0);
        }
    }
}

static void cube_color(unsigned face,unsigned mip,int updated,float color[4]) {
    color[0]=(face+1)/8.0f;color[1]=(mip+1)/4.0f;
    color[2]=(updated && face==4 && mip==1)?.875f:.125f;color[3]=1;
}

static void volume_color(unsigned x,unsigned y,unsigned z,unsigned mip,int updated,float color[4]) {
    color[0]=(x+1)/8.0f;color[1]=(y+1)/8.0f;
    color[2]=(updated && mip==0 && z==1)?.875f:(z+1+4*mip)/16.0f;color[3]=1;
}

static void rhi_texture_resources_test(RhiView *restore_target) {
    const uint32_t row=5*4*sizeof(float),slice=5*row;
    float cube_data[18][5*5*4],volume_data[3][5*5*5*4];
    RhiSubresourceData cube_initial[18],volume_initial[3];
    for(unsigned i=0;i<sizeof cube_data/sizeof(float);i++)((float *)cube_data)[i]=-1000;
    for(unsigned i=0;i<sizeof volume_data/sizeof(float);i++)((float *)volume_data)[i]=-1000;
    for(unsigned face=0;face<6;face++)for(unsigned mip=0;mip<3;mip++) {
        unsigned sub=face*3+mip,size=4>>mip;float color[4];cube_color(face,mip,0,color);
        for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++)
            memcpy(&cube_data[sub][(y*5+x)*4],color,sizeof color);
        cube_initial[sub]=(RhiSubresourceData){cube_data[sub],row,slice};
    }
    for(unsigned mip=0;mip<3;mip++) {
        unsigned size=4>>mip;
        for(unsigned z=0;z<size;z++)for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++)
            volume_color(x,y,z,mip,0,&volume_data[mip][((z*5+y)*5+x)*4]);
        volume_initial[mip]=(RhiSubresourceData){volume_data[mip],row,slice};
    }
    RhiImageDesc desc={0};desc.width=desc.height=4;desc.depth=6;desc.mip_levels=3;
    desc.samples=1;desc.format=RHI_FORMAT_R32G32B32A32_FLOAT;desc.bind=RHI_BIND_SAMPLED;desc.cube=1;
    RhiImage *cube=rhi_image_create(&desc,cube_initial);CHECK(cube);
    RhiImageDesc actual={0};rhi_image_get_desc(cube,&actual);
    CHECK(actual.type==RHI_IMAGE_2D && actual.cube && actual.depth==6 && actual.mip_levels==3);
    desc.type=RHI_IMAGE_3D;desc.depth=4;desc.cube=0;
    RhiImage *volume=rhi_image_create(&desc,volume_initial);CHECK(volume);
    rhi_image_get_desc(volume,&actual);
    CHECK(actual.type==RHI_IMAGE_3D && !actual.cube && actual.depth==4 && actual.mip_levels==3);
    desc.format=RHI_FORMAT_UNKNOWN;CHECK(!rhi_image_create(&desc,NULL));
    RhiView *cube_view=rhi_view_create(cube,RHI_VIEW_SAMPLED,NULL);
    RhiView *volume_view=rhi_view_create(volume,RHI_VIEW_SAMPLED,NULL);CHECK(cube_view && volume_view);
    CHECK(rhi_image_retain(cube)==cube && rhi_image_retain(volume)==volume);
    rhi_image_destroy(cube);rhi_image_destroy(volume); /* retained references remain usable */
    RhiBufferDesc bd={16,RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *queries=rhi_buffer_create(&bd,NULL);CHECK(queries);
    RhiSamplerDesc sd={0};sd.filter=RHI_FILTER_POINT;sd.max_lod=1000;sd.max_anisotropy=1;
    sd.address_u=sd.address_v=sd.address_w=RHI_ADDRESS_CLAMP;sd.compare=RHI_CMP_ALWAYS;
    RhiSampler *sampler=rhi_sampler_create(&sd);CHECK(sampler);
    RhiShader *cube_ps=shader(RHI_STAGE_PIXEL,
        "TextureCube<float4> tex:register(t0);SamplerState s:register(s0);"
        "cbuffer Q:register(b0){float4 q;}"
        "float4 main():SV_TARGET{return tex.SampleLevel(s,q.xyz,q.w);}","ps_5_0");
    RhiShader *volume_ps=shader(RHI_STAGE_PIXEL,
        "Texture3D<float4> tex:register(t0);SamplerState s:register(s0);"
        "cbuffer Q:register(b0){float4 q;}"
        "float4 main():SV_TARGET{return tex.SampleLevel(s,q.xyz,q.w);}","ps_5_0");
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&queries);rhi_set_samplers(0,1,&sampler);
    const float directions[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(int updated=0;updated<2;updated++) {
        if(updated) {
            float color[4];cube_color(4,1,1,color);
            for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)memcpy(&cube_data[13][(y*5+x)*4],color,sizeof color);
            rhi_image_update(cube,13,NULL,cube_data[13],row,slice);
            /* Replace just z=1 at level 0, preserving every other slice/mip. */
            for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)
                volume_color(x,y,1,0,1,&volume_data[0][(y*5+x)*4]);
            RhiBox box={0,0,1,4,4,2};
            rhi_image_update(volume,0,&box,volume_data[0],row,slice);
        }
        rhi_set_shader(RHI_STAGE_PIXEL,cube_ps);rhi_set_textures(0,1,&cube_view);
        for(unsigned face=0;face<6;face++)for(unsigned mip=0;mip<3;mip++) {
            float expected[4],query[4]={directions[face][0],directions[face][1],directions[face][2],(float)mip};
            cube_color(face,mip,updated,expected);texture_expect(queries,query,expected);
            float pixels[4*4*4];unsigned size=4>>mip;
            CHECK(rhi_image_readback(cube,face*3+mip,pixels,size*4*sizeof(float))==0);
            for(unsigned i=0;i<size*size;i++)for(unsigned c=0;c<4;c++)CHECK(fabsf(pixels[i*4+c]-expected[c])<.0001f);
        }
        rhi_set_shader(RHI_STAGE_PIXEL,volume_ps);rhi_set_textures(0,1,&volume_view);
        for(unsigned mip=0;mip<3;mip++) {
            unsigned size=4>>mip;
            for(unsigned z=0;z<size;z++)for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++) {
                float expected[4],query[4]={(x+.5f)/size,(y+.5f)/size,(z+.5f)/size,(float)mip};
                volume_color(x,y,z,mip,updated,expected);texture_expect(queries,query,expected);
            }
        }
    }
    RhiViewDesc view_desc={0};view_desc.base_mip=1;view_desc.mip_count=1;view_desc.layer_count=6;
    view_desc.dim=RHI_VIEW_DIM_CUBE;
    RhiView *cube_mip=rhi_view_create(cube,RHI_VIEW_SAMPLED,&view_desc);CHECK(cube_mip);
    view_desc.dim=RHI_VIEW_DIM_3D;view_desc.layer_count=1;
    RhiView *volume_mip=rhi_view_create(volume,RHI_VIEW_SAMPLED,&view_desc);CHECK(volume_mip);
    rhi_set_shader(RHI_STAGE_PIXEL,cube_ps);rhi_set_textures(0,1,&cube_mip);
    for(unsigned face=0;face<6;face++) {
        float expected[4],query[4]={directions[face][0],directions[face][1],directions[face][2],0};
        cube_color(face,1,1,expected);texture_expect(queries,query,expected);
    }
    rhi_set_shader(RHI_STAGE_PIXEL,volume_ps);rhi_set_textures(0,1,&volume_mip);
    for(unsigned z=0;z<2;z++)for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++) {
        float expected[4],query[4]={(x+.5f)/2,(y+.5f)/2,(z+.5f)/2,0};
        volume_color(x,y,z,1,1,expected);texture_expect(queries,query,expected);
    }
    /* Xbox alpha-only and opaque-X formats require nonidentity view mapping.
     * Include ordinary red/RGBA controls so a blanket shader remap cannot pass. */
    RhiShader *format_ps=shader(RHI_STAGE_PIXEL,
        "Texture2D<float4> tex:register(t0);SamplerState s:register(s0);"
        "float4 main():SV_TARGET{return tex.SampleLevel(s,float2(.5,.5),0);}","ps_5_0");
    const uint32_t formats[]={RHI_FORMAT_A8_UNORM,RHI_FORMAT_B8G8R8X8_UNORM,
                              RHI_FORMAT_R8_UNORM,RHI_FORMAT_R8G8B8A8_UNORM};
    const unsigned char bytes[4][4]={{64,0,0,0},{32,64,128,0},{64,0,0,0},{32,64,128,16}};
    const float expected[4][4]={{0,0,0,64/255.f},{128/255.f,64/255.f,32/255.f,1},
                               {64/255.f,0,0,1},{32/255.f,64/255.f,128/255.f,16/255.f}};
    rhi_set_shader(RHI_STAGE_PIXEL,format_ps);
    for(unsigned f=0;f<4;f++) {
        RhiImageDesc fd={0};fd.width=fd.height=fd.depth=fd.mip_levels=fd.samples=1;
        fd.format=formats[f];fd.bind=RHI_BIND_SAMPLED;
        RhiSubresourceData initial={bytes[f],(f==0 || f==2)?1:4,4};
        RhiImage *image=rhi_image_create(&fd,&initial);CHECK(image);
        RhiView *view=rhi_view_create(image,RHI_VIEW_SAMPLED,NULL);CHECK(view);
        rhi_set_textures(0,1,&view);
        const float query[4]={0};texture_expect(queries,query,expected[f]);
        RhiView *unbound=NULL;rhi_set_textures(0,1,&unbound);
        rhi_view_destroy(view);rhi_image_destroy(image);
    }
    /* Actual render-target writes, sampling and readback must retain sub-byte
     * feedback in half float. Both values intentionally round to byte 128. */
    RhiShader *precision_ps=shader(RHI_STAGE_PIXEL,
        "cbuffer Q:register(b0){float4 q;}float4 main():SV_TARGET{return q;}","ps_5_0");
    const float precise[4]={.501953125f,.50244140625f,.5f,1};
    const float quantized[4]={128/255.f,128/255.f,128/255.f,1};
    for(unsigned half=0;half<2;half++) {
        RhiImageDesc pd={0};pd.width=pd.height=pd.depth=pd.mip_levels=pd.samples=1;
        pd.format=half?RHI_FORMAT_R16G16B16A16_FLOAT:RHI_FORMAT_R8G8B8A8_UNORM;
        pd.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
        RhiImage *image=rhi_image_create(&pd,NULL);CHECK(image);
        RhiView *sample=rhi_view_create(image,RHI_VIEW_SAMPLED,NULL);
        RhiView *write=rhi_view_create(image,RHI_VIEW_RENDER_TARGET,NULL);CHECK(sample && write);
        RhiView *empty=NULL;rhi_set_textures(0,1,&empty);
        RhiViewport tiny_viewport={0,0,1,1,0,1},full={0,0,64,64,0,1};
        rhi_set_render_target(write,NULL);rhi_set_viewports(1,&tiny_viewport);
        rhi_set_shader(RHI_STAGE_PIXEL,precision_ps);rhi_buffer_update(queries,precise);rhi_draw(3,0);
        if(half) {
            uint16_t bytes[4]={0};
            CHECK(rhi_image_readback(image,0,bytes,sizeof bytes)==0);
            CHECK(bytes[0]==0x3804 && bytes[1]==0x3805 && bytes[2]==0x3800 && bytes[3]==0x3c00);
        }
        rhi_set_render_target(restore_target,NULL);rhi_set_viewports(1,&full);
        rhi_set_shader(RHI_STAGE_PIXEL,format_ps);rhi_set_textures(0,1,&sample);
        const float query[4]={0};texture_expect(queries,query,half?precise:quantized);
        rhi_set_textures(0,1,&empty);
        rhi_view_destroy(write);rhi_view_destroy(sample);rhi_image_destroy(image);
    }
    rhi_shader_destroy(precision_ps);
    puts("PASS: shared RHI half-float target preserves sub-byte writes while byte target quantizes");
    rhi_shader_destroy(format_ps);
    puts("PASS: shared RHI A8 alpha, BGRX opaque alpha and ordinary format controls");
    RhiView *none=NULL;rhi_set_textures(0,1,&none);
    RhiBuffer *no_buffer=NULL;rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&no_buffer);
    RhiSampler *no_sampler=NULL;rhi_set_samplers(0,1,&no_sampler);
    rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    rhi_view_destroy(cube_mip);rhi_view_destroy(volume_mip);
    rhi_view_destroy(cube_view);rhi_view_destroy(volume_view);
    rhi_image_destroy(cube);rhi_image_destroy(volume);
    rhi_buffer_destroy(queries);rhi_sampler_destroy(sampler);
    rhi_shader_destroy(cube_ps);rhi_shader_destroy(volume_ps);
    puts("PASS: shared RHI cube faces, volume voxels, mips, pitched uploads, isolated updates, views and retained lifetime");
}
#endif
