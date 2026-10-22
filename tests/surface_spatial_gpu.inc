/* Controlled 64x64 feedback field. The original pixel programs are translated
 * unchanged. Explicit diagonal offsets follow callback 0x2BF10; this does not
 * execute the original vertex program, disturbance or smoothing passes. */
static float field_a[64*64*4],field_b[64*64*4],reference_fields[3][64*64*4];
static unsigned field_index(int x,int y,unsigned c) {
    x=x<0?0:x>63?63:x;y=y<0?0:y>63?63:y;
    return (y*64+x)*4+c;
}
static void field_read(RhiImage *image,unsigned storage,float *out) {
    union {float f[64*64*4];uint16_t h[64*64*4];uint8_t b[64*64*4];} raw;
    unsigned stride=storage==0?16:storage==1?8:4;
    CHECK(rhi_image_readback(image,0,&raw,64*stride)==0);
    for(unsigned i=0;i<64*64*4;i++) {
        if(!storage)out[i]=raw.f[i];
        else if(storage==2)out[i]=raw.b[i]/255.f;
        else {unsigned h=raw.h[i],e=(h>>10)&31,m=h&1023;CHECK(e!=31);
            out[i]=ldexpf((float)(e?1024+m:m),e?(int)e-25:-24);if(h&32768)out[i]=-out[i];}
    }
}
static void field_constants(NV2APSConstants *cb,const DWORD *definition,float vectors[3][4]) {
    memset(cb,0,sizeof *cb);for(unsigned i=0;i<4;i++)cb->tex_scale[i][0]=cb->tex_scale[i][1]=1;
    const unsigned shifts[4]={16,8,0,24};
    for(unsigned bank=0;bank<2;bank++)for(unsigned stage=0;stage<8;stage++) {
        float *dst=bank?cb->c1[stage]:cb->c0[stage];
        unsigned mapped=(definition[57+bank]>>(stage*4))&15;
        for(unsigned c=0;c<4;c++)dst[c]=mapped<3?vectors[mapped][c]:
            ((definition[10+bank*8+stage]>>shifts[c])&255)/255.f;
    }
}
/* SPATIAL_MAIN */
    RhiShader *field_vs[2];
    const char *vertex_prefix=
        "struct O {float4 pos:SV_POSITION;float4 c0:COLOR0;float4 c1:COLOR1;"
        "float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};"
        "O main(uint id:SV_VertexID) {O o=(O)0;o.pos=float4(id==2?3:-1,id==1?3:-1,0,1);"
        "float2 uv=float2(o.pos.x+1,1-o.pos.y)*.5;o.t0=float4(uv,0,1);";
    char vertex_text[2048];
    snprintf(vertex_text,sizeof vertex_text,"%so.t1=float4(uv+float2(-1,-1)/64,0,1);"
        "o.t2=float4(uv+float2(1,-1)/64,0,1);o.t3=float4(uv+float2(1,1)/64,0,1);return o;}",vertex_prefix);
    field_vs[0]=shader(RHI_STAGE_VERTEX,vertex_text,"vs_5_0");
    snprintf(vertex_text,sizeof vertex_text,"%so.t1=float4(uv+float2(-1,1)/64,0,1);"
        "o.t2=o.t3=o.t0;return o;}",vertex_prefix);
    field_vs[1]=shader(RHI_STAGE_VERTEX,vertex_text,"vs_5_0");
    RhiShader *field_ps[2]={shader(RHI_STAGE_PIXEL,first_hlsl,"ps_5_0"),
        shader(RHI_STAGE_PIXEL,surface_hlsl,"ps_5_0")};
    DWORD definitions[2][60];
    for(unsigned i=0;i<2;i++) {
        FILE *f=fopen(i?"feedback.bin":"first.bin","rb");CHECK(f);
        CHECK(fread(definitions[i],4,60,f)==60);fclose(f);
    }
    RhiBufferDesc constants_desc={sizeof(NV2APSConstants),RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *uniforms=rhi_buffer_create(&constants_desc,NULL);CHECK(uniforms);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&uniforms);
    const uint32_t formats[]={RHI_FORMAT_R32G32B32A32_FLOAT,RHI_FORMAT_R16G16B16A16_FLOAT,RHI_FORMAT_R8G8B8A8_UNORM};
    const unsigned rates[]={60,120,144};
    const float authored[3]={.105f,.65f,.98f};
    for(unsigned i=0;i<3;i++)CHECK(pc_surface_packed_coefficient(authored[i])==packed_coefficients[0][i]/255.f);
    for(unsigned storage=0;storage<3;storage++)for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];float delta=60.f/fps,precise[3];pc_surface_coefficients(delta,authored,precise);
        float k=pc_surface_packed_coefficient(.17f),bias=pc_surface_bias(delta);
        float first_vectors[3][4]={{k,k,k,k}};
        float second_vectors[3][4]={{0,precise[0],0,0},{0,0,precise[1],precise[1]},
            {0,0,precise[2],precise[2]}};
        NV2APSConstants constants[2];field_constants(&constants[0],definitions[0],first_vectors);
        field_constants(&constants[1],definitions[1],second_vectors);constants[1].c1[6][1]=bias;
        for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++) {
            unsigned i=(y*64+x)*4;float dx=(float)x-31.5f,dy=(float)y-31.5f;
            field_a[i]=field_a[i+1]=.5f;field_a[i+2]=.5f+.05f*expf(-(dx*dx+dy*dy)/32);field_a[i+3]=1;
        }
        td.width=td.height=64;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        RhiSubresourceData initial_state={field_a,64*4*sizeof(float),sizeof field_a};
        RhiImage *inputs[2]={rhi_image_create(&td,&initial_state),rhi_image_create(&td,&initial_state)};
        CHECK(inputs[0] && inputs[1]);
        RhiView *input_views[2]={rhi_view_create(inputs[0],RHI_VIEW_SAMPLED,NULL),rhi_view_create(inputs[1],RHI_VIEW_SAMPLED,NULL)};
        CHECK(input_views[0] && input_views[1]);
        td.format=formats[storage];td.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *output=rhi_image_create(&td,NULL);CHECK(output);
        RhiView *output_view=rhi_view_create(output,RHI_VIEW_RENDER_TARGET,NULL);CHECK(output_view);
        rhi_set_render_target(output_view,NULL);
        RhiSampler *samplers[4]={sampler,sampler,sampler,sampler};rhi_set_samplers(0,4,samplers);
        float max_error=0;unsigned changed=0;
        const float tolerance=!storage?.000004f:storage==1?.0005f:.501f/255.f;
        for(unsigned step=0;step<fps;step++) {
            rhi_image_update(inputs[0],0,NULL,field_a,64*4*sizeof(float),sizeof field_a);
            RhiView *views[4]={input_views[0],input_views[0],input_views[0],input_views[0]};
            rhi_set_textures(0,4,views);rhi_set_shader(RHI_STAGE_VERTEX,field_vs[0]);rhi_set_shader(RHI_STAGE_PIXEL,field_ps[0]);
            rhi_buffer_update(uniforms,&constants[0]);rhi_draw(3,0);field_read(output,storage,field_b);
            for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
                unsigned i=field_index(x,y,0);float b=field_a[i+2];
                float gradient=(128/255.f-b)*k+field_a[field_index(x-1,y-1,2)]+field_a[field_index(x+1,y-1,2)]+field_a[field_index(x+1,y+1,2)]-3*b;
                float expected[3]={gradient+128/255.f,field_a[i+1],b};
                for(unsigned c=0;c<3;c++) {CHECK(expected[c]>0 && expected[c]<1);
                    float e=fabsf(field_b[i+c]-expected[c]);if(e>max_error)max_error=e;CHECK(e<tolerance);}
            }
            /* t0 is the first pass result; t1 is the old persistent image. */
            rhi_image_update(inputs[1],0,NULL,field_b,64*4*sizeof(float),sizeof field_b);
            views[0]=input_views[1];views[1]=input_views[0];views[2]=views[3]=NULL;
            rhi_set_textures(0,4,views);rhi_set_shader(RHI_STAGE_VERTEX,field_vs[1]);rhi_set_shader(RHI_STAGE_PIXEL,field_ps[1]);
            rhi_buffer_update(uniforms,&constants[1]);rhi_draw(3,0);
            static float next[64*64*4];field_read(output,storage,next);unsigned fresh=0;
            for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
                unsigned i=field_index(x,y,0);
                float v=field_b[i+1]-.5f+precise[0]*(field_b[i]-.5f+field_a[field_index(x-1,y+1,2)]-field_b[i+2]);
                float expected[2]={v+bias,precise[2]*(field_b[i+2]+precise[1]*v)};
                for(unsigned c=0;c<2;c++) {CHECK(expected[c]>0 && expected[c]<1);
                    float e=fabsf(next[i+c+1]-expected[c]);if(e>max_error)max_error=e;CHECK(e<tolerance);
                    fresh|=next[i+c+1]!=field_a[i+c+1];}
            }
            changed+=fresh;memcpy(field_a,next,sizeof field_a);
        }
        if(!rate)memcpy(reference_fields[storage],field_a,sizeof field_a);
        double sum=0,sum2=0,difference=0;
        for(unsigned i=0;i<64*64;i++) {double b=field_a[i*4+2],d=b-reference_fields[storage][i*4+2];sum+=b;sum2+=b*b;difference+=d*d;}
        double mean=sum/(64*64),std=sqrt(fmax(0,sum2/(64*64)-mean*mean)),rms=sqrt(difference/(64*64));
        printf("spatial storage=%u fps=%u meanB=%.9f stdB=%.9f rms_to_60=%.9f changed=%u/%u max_error=%.9f\n",storage,fps,mean,std,rms,changed,fps,max_error);
        CHECK(changed>0);if(rate && !storage)CHECK(rms>.0001);
        RhiView *none[4]={0};rhi_set_textures(0,4,none);rhi_set_render_target(NULL,NULL);
        for(unsigned i=0;i<2;i++){rhi_view_destroy(input_views[i]);rhi_image_destroy(inputs[i]);}
        rhi_view_destroy(output_view);rhi_image_destroy(output);
    }
    puts("PASS: original translated feedback shader pair verifies spatial coupling and storage gaps");
    rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    for(unsigned i=0;i<2;i++){rhi_shader_destroy(field_vs[i]);rhi_shader_destroy(field_ps[i]);}
    rhi_buffer_destroy(uniforms);rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);rhi_device_destroy();return 0;
}
