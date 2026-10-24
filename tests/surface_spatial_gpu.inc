/* Controlled feedback field at diagnostic and authored sizes. Original pixel
 * and vertex programs are translated unchanged. The wrapped fixture mirrors
 * captured geometry/viewport inputs; it does not replay guest submission code
 * or disturbances. A separate Original60 control includes captured cardinal
 * smoothing. Feedback-only candidate offsets follow callback 0x2BF10. */
#ifndef SURFACE_SIDE
#define SURFACE_SIDE 64
#endif
#ifndef SURFACE_WRAP
#define SURFACE_WRAP 0
#endif
#ifndef SURFACE_SMOOTHING
#define SURFACE_SMOOTHING 0
#endif
#ifndef SURFACE_BASIS_WRAP
#define SURFACE_BASIS_WRAP SURFACE_WRAP
#endif
#include "graphics/surface_basis.h"
static double field_weight(unsigned k,unsigned x) {
    return pc_surface_basis_weight(SURFACE_SIDE,k,x,(PcSurfaceBoundary)SURFACE_BASIS_WRAP);
}
static float field_initial(unsigned x,unsigned y) {
    float dx=x-((SURFACE_SIDE-1)*.5f),dy=y-((SURFACE_SIDE-1)*.5f);
    float value=.5f+.05f*expf(-(dx*dx+dy*dy)/32);
    /* A small edge pulse crosses the periodic seam immediately; a centered
     * Gaussian alone cannot distinguish clamp from wrap over a short test. */
    if(SURFACE_WRAP) {dx=(float)x;dy=(float)y-3;value+=.01f*expf(-(dx*dx+dy*dy)/8);}
    return value;
}
static float field_a[SURFACE_SIDE*SURFACE_SIDE*4],field_b[SURFACE_SIDE*SURFACE_SIDE*4],reference_fields[3][SURFACE_SIDE*SURFACE_SIDE*4];
/* Orthonormal DCT-II diagonalizes the clamped diagonal-neighbor stencil;
 * the separable real Hartley basis handles the observed periodic stencil.
 * CPU transforms are an independent reference for the GPU transform probes;
 * candidate evolution and every reconstructed frame use the shared GPU body. */
static double field_basis[SURFACE_SIDE][SURFACE_SIDE],field_work[SURFACE_SIDE*SURFACE_SIDE*2];
static void field_transform(const float *input,float *output,int inverse) {
    for(unsigned k=0;k<SURFACE_SIDE;k++)for(unsigned x=0;x<SURFACE_SIDE;x++)
        field_basis[k][x]=field_weight(k,x);
    for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++)for(unsigned c=0;c<2;c++) {
        double sum=0;
        for(unsigned i=0;i<SURFACE_SIDE;i++)sum+=(inverse?field_basis[i][x]:field_basis[x][i])*input[(y*SURFACE_SIDE+i)*4+c];
        field_work[(y*SURFACE_SIDE+x)*2+c]=sum;
    }
    for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++) {
        for(unsigned c=0;c<2;c++) {
            double sum=0;
            for(unsigned i=0;i<SURFACE_SIDE;i++)sum+=(inverse?field_basis[i][y]:field_basis[y][i])*field_work[(i*SURFACE_SIDE+x)*2+c];
            output[(y*SURFACE_SIDE+x)*4+c]=(float)sum;
        }
        output[(y*SURFACE_SIDE+x)*4+2]=0;output[(y*SURFACE_SIDE+x)*4+3]=1;
    }
}
static unsigned field_index(int x,int y,unsigned c) {
    if(SURFACE_WRAP) {x=(x+SURFACE_SIDE)%SURFACE_SIDE;y=(y+SURFACE_SIDE)%SURFACE_SIDE;}
    else {x=x<0?0:x>(SURFACE_SIDE-1)?(SURFACE_SIDE-1):x;y=y<0?0:y>(SURFACE_SIDE-1)?(SURFACE_SIDE-1):y;}
    return (y*SURFACE_SIDE+x)*4+c;
}
static void field_read(RhiImage *image,unsigned storage,float *out) {
    union {float f[SURFACE_SIDE*SURFACE_SIDE*4];uint16_t h[SURFACE_SIDE*SURFACE_SIDE*4];uint8_t b[SURFACE_SIDE*SURFACE_SIDE*4];} raw;
    unsigned stride=storage==0?16:storage==1?8:4;
    CHECK(rhi_image_readback(image,0,&raw,SURFACE_SIDE*stride)==0);
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE*4;i++) {
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
/* Reusable test-owned resources; every transform stays on the GPU. */
static RhiShader *transform_ps[2][2];
static RhiView *transform_scratch_read,*transform_scratch_write,*transform_basis;
static void field_gpu_transform(RhiView *source,RhiView *destination,unsigned inverse) {
    for(unsigned axis=0;axis<2;axis++) {
        RhiView *none[3]={0};rhi_set_textures(0,3,none);
        rhi_set_render_target(axis?destination:transform_scratch_write,NULL);
        RhiView *inputs[2]={axis?transform_scratch_read:source,transform_basis};
        rhi_set_textures(0,2,inputs);
        rhi_set_shader(RHI_STAGE_PIXEL,transform_ps[inverse][axis]);rhi_draw(3,0);
    }
    RhiView *none[3]={0};rhi_set_textures(0,3,none);rhi_set_render_target(NULL,NULL);
}
/* SPATIAL_MAIN */
    viewport.width=viewport.height=SURFACE_SIDE;rhi_set_viewports(1,&viewport);
    if(SURFACE_WRAP) {
        RhiSampler *none[4]={0};rhi_set_samplers(0,4,none);rhi_sampler_destroy(sampler);
        sd.address_u=sd.address_v=sd.address_w=RHI_ADDRESS_WRAP;
        sampler=rhi_sampler_create(&sd);CHECK(sampler);
    }
    /* Execute the original six-instruction vertex program with captured inputs
     * for wrap, or a synthetic NDC triangle for the clamp diagnostic. Actual
     * guest submission and scene output still require runtime validation. */
    char vertex_text[65536];
    if(SURFACE_WRAP) {
        /* Mirror the observed Original60 host inputs: four 24-byte vertices,
         * pixel-space quad, normalized UVs, and enabled screen-space undo. */
        snprintf(vertex_text,sizeof vertex_text,"#define main original_vertex\n%s\n#undef main\n"
            "VS_OUT main(uint id:SV_VertexID) {VS_IN v=(VS_IN)0;"
            "float2 uv=float2(id>=2?1:0,(id&1)?1:0);"
            "v.v1=float4(uv*%u,.001,1);v.v2=float4(127,127,127,127)/255;"
            "v.v3=float4(uv,0,1);return original_vertex(v);}",surface_vertex_hlsl,SURFACE_SIDE);
        rhi_set_topology(RHI_TOPOLOGY_TRIANGLE_STRIP);
    } else {
        snprintf(vertex_text,sizeof vertex_text,"#define main original_vertex\n%s\n#undef main\n"
            "VS_OUT main(uint id:SV_VertexID) {VS_IN v=(VS_IN)0;"
            "v.v1=float4(id==2?3:-1,id==1?3:-1,0,1);v.v2=float4(1,1,1,1);"
            "v.v3=float4(float2(v.v1.x+1,1-v.v1.y)*.5,0,1);return original_vertex(v);}",surface_vertex_hlsl);
    }
    RhiShader *field_vs=shader(RHI_STAGE_VERTEX,vertex_text,"vs_5_0");
    float vertex_constants[2][192][4]={0},vertex_screenspace[12]={0},vertex_inputs[20][4]={0};
    if(SURFACE_WRAP) {
        vertex_screenspace[0]=SURFACE_SIDE*.5f;vertex_screenspace[1]=-SURFACE_SIDE*.5f;
        vertex_screenspace[2]=16777215.f;vertex_screenspace[3]=1;
        vertex_screenspace[4]=vertex_screenspace[5]=SURFACE_SIDE*.5f;
        vertex_screenspace[8]=1;
    }
    vertex_inputs[16][1]=vertex_inputs[16][2]=vertex_inputs[16][3]=1;
    vertex_constants[0][18][0]=vertex_constants[0][18][1]=-1.f/SURFACE_SIDE;
    vertex_constants[0][19][0]=1.f/SURFACE_SIDE;vertex_constants[0][19][1]=-1.f/SURFACE_SIDE;
    vertex_constants[0][20][0]=vertex_constants[0][20][1]=1.f/SURFACE_SIDE;
    vertex_constants[1][18][0]=-1.f/SURFACE_SIDE;vertex_constants[1][18][1]=1.f/SURFACE_SIDE;
    RhiBufferDesc vertex_desc={sizeof vertex_constants[0],RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *vertex_buffers[3];
    vertex_buffers[0]=rhi_buffer_create(&vertex_desc,vertex_constants[0]);
    vertex_desc.size=sizeof vertex_screenspace;vertex_buffers[1]=rhi_buffer_create(&vertex_desc,vertex_screenspace);
    vertex_desc.size=sizeof vertex_inputs;vertex_buffers[2]=rhi_buffer_create(&vertex_desc,vertex_inputs);
    CHECK(vertex_buffers[0] && vertex_buffers[1] && vertex_buffers[2]);
    rhi_set_uniform_buffers(RHI_STAGE_VERTEX,1,3,vertex_buffers);
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
    FieldFinal final;field_final_init(&final,field_vs,vs,vertex_buffers[0],uniforms);
    RhiShader *filter_ps=NULL;
    NV2APSConstants filter_constants={0};
    float filter_offsets[192][4]={0};
    if(SURFACE_SMOOTHING) {
        DWORD definition[60];FILE *f=fopen("filter.bin","rb");CHECK(f);
        CHECK(fread(definition,4,60,f)==60);fclose(f);
        CHECK(definition[13]==0x80808080u);
        for(unsigned i=0;i<4;i++)filter_constants.tex_scale[i][0]=filter_constants.tex_scale[i][1]=1;
        for(unsigned c=0;c<4;c++)filter_constants.c0[3][c]=128/255.f;
        filter_offsets[17][1]=-1.f/SURFACE_SIDE;
        filter_offsets[18][0]=1.f/SURFACE_SIDE;
        filter_offsets[19][1]=1.f/SURFACE_SIDE;
        filter_offsets[20][0]=-1.f/SURFACE_SIDE;
        filter_ps=shader(RHI_STAGE_PIXEL,filter_hlsl,"ps_5_0");
    }
    const uint32_t formats[]={RHI_FORMAT_R32G32B32A32_FLOAT,RHI_FORMAT_R16G16B16A16_FLOAT,RHI_FORMAT_R8G8B8A8_UNORM};
    const unsigned rates[]={60,120,144};
    const float authored[3]={.105f,.65f,.98f};
    for(unsigned i=0;i<3;i++)CHECK(pc_surface_packed_coefficient(authored[i])==packed_coefficients[0][i]/255.f);
    /* Controls 3/4 quantize only one pass, isolating intermediate-gradient
     * rounding from the persistent velocity/displacement rounding. */
    for(unsigned storage=0;storage<6;storage++)for(unsigned rate=0;rate<(storage<3?3u:1u);rate++) {
        if(SURFACE_SMOOTHING && rate)continue; /* Original60 reference only. */
        unsigned first_storage=storage<3?storage:storage==4?0:2;
        unsigned second_storage=storage<3?storage:storage==4?2:0;
        unsigned fps=rates[rate];float delta=60.f/fps,precise[3];pc_surface_coefficients(delta,authored,precise);
        float k=pc_surface_packed_coefficient(.17f),bias=pc_surface_bias(delta);
        float first_vectors[3][4]={{k,k,k,k}};
        float second_vectors[3][4]={{0,precise[0],0,0},{0,0,precise[1],precise[1]},
            {0,0,precise[2],precise[2]}};
        NV2APSConstants constants[2];field_constants(&constants[0],definitions[0],first_vectors);
        field_constants(&constants[1],definitions[1],second_vectors);constants[1].c1[6][1]=bias;
        for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++) {
            unsigned i=(y*SURFACE_SIDE+x)*4;
            field_a[i]=field_a[i+1]=.5f;field_a[i+2]=field_initial(x,y);field_a[i+3]=1;
        }
        td.width=td.height=SURFACE_SIDE;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        RhiSubresourceData initial_state={field_a,SURFACE_SIDE*4*sizeof(float),sizeof field_a};
        RhiImage *inputs[2]={rhi_image_create(&td,&initial_state),rhi_image_create(&td,&initial_state)};
        CHECK(inputs[0] && inputs[1]);
        RhiView *input_views[2]={rhi_view_create(inputs[0],RHI_VIEW_SAMPLED,NULL),rhi_view_create(inputs[1],RHI_VIEW_SAMPLED,NULL)};
        CHECK(input_views[0] && input_views[1]);
        RhiImage *outputs[2];RhiView *output_views[2];
        for(unsigned pass=0;pass<2;pass++) {
            td.format=formats[pass?second_storage:first_storage];td.bind=RHI_BIND_RENDER_TARGET;
            outputs[pass]=rhi_image_create(&td,NULL);CHECK(outputs[pass]);
            output_views[pass]=rhi_view_create(outputs[pass],RHI_VIEW_RENDER_TARGET,NULL);CHECK(output_views[pass]);
        }
        RhiSampler *samplers[4]={sampler,sampler,sampler,sampler};rhi_set_samplers(0,4,samplers);
        float max_error=0;unsigned changed=0;
        const float tolerances[]={.000004f,.0005f,.501f/255.f};
        double quantization_error[2][3]={{0}};
        double smoothing_error=0;
        for(unsigned step=0;step<fps;step++) {
            if(SURFACE_SMOOTHING) {
                rhi_image_update(inputs[0],0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
                RhiView *sampled[4]={input_views[0],input_views[0],input_views[0],input_views[0]};
                rhi_set_textures(0,4,sampled);rhi_set_shader(RHI_STAGE_VERTEX,field_vs);
                rhi_buffer_update(vertex_buffers[0],filter_offsets);
                rhi_set_shader(RHI_STAGE_PIXEL,filter_ps);rhi_set_render_target(output_views[0],NULL);
                rhi_buffer_update(uniforms,&filter_constants);rhi_draw(4,0);
                field_read(outputs[0],first_storage,field_b);
                for(int y=0;y<SURFACE_SIDE;y++)for(int x=0;x<SURFACE_SIDE;x++)for(unsigned c=0;c<3;c++) {
                    float average=(field_a[field_index(x,y-1,c)]+field_a[field_index(x+1,y,c)]+
                        field_a[field_index(x,y+1,c)]+field_a[field_index(x-1,y,c)])*.25f;
                    float expected=fminf(1,average-.5f+128/255.f)*(254/255.f);
                    double error=fabs(field_b[field_index(x,y,c)]-expected);
                    smoothing_error=fmax(smoothing_error,error);CHECK(error<tolerances[first_storage]);
                }
                memcpy(field_a,field_b,sizeof field_a);
            }
            rhi_image_update(inputs[0],0,NULL,field_a,SURFACE_SIDE*4*sizeof(float),sizeof field_a);
            RhiView *views[4]={input_views[0],input_views[0],input_views[0],input_views[0]};
            rhi_set_textures(0,4,views);rhi_set_shader(RHI_STAGE_VERTEX,field_vs);rhi_buffer_update(vertex_buffers[0],vertex_constants[0]);rhi_set_shader(RHI_STAGE_PIXEL,field_ps[0]);
            rhi_set_render_target(output_views[0],NULL);
            rhi_buffer_update(uniforms,&constants[0]);rhi_draw(SURFACE_WRAP?4:3,0);field_read(outputs[0],first_storage,field_b);
            for(int y=0;y<SURFACE_SIDE;y++)for(int x=0;x<SURFACE_SIDE;x++) {
                unsigned i=field_index(x,y,0);float b=field_a[i+2];
                float gradient=(128/255.f-b)*k+field_a[field_index(x-1,y-1,2)]+field_a[field_index(x+1,y-1,2)]+field_a[field_index(x+1,y+1,2)]-3*b;
                float expected[3]={gradient+128/255.f,field_a[i+1],b};
                for(unsigned c=0;c<3;c++) {CHECK(expected[c]>0 && expected[c]<1);
                    float e=fabsf(field_b[i+c]-expected[c]);if(e>max_error)max_error=e;CHECK(e<tolerances[first_storage]);
                    quantization_error[0][c]+=field_b[i+c]-expected[c];}
            }
            /* Control 5 keeps only the first pass's gradient quantization;
             * its copied G/B channels retain full precision. This prevents
             * attributing repeated state rounding to the gradient alone. */
            if(storage==5)for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++) {
                field_b[i*4+1]=field_a[i*4+1];field_b[i*4+2]=field_a[i*4+2];
            }
            /* t0 is the first pass result; t1 is the old persistent image. */
            rhi_image_update(inputs[1],0,NULL,field_b,SURFACE_SIDE*4*sizeof(float),sizeof field_b);
            views[0]=input_views[1];views[1]=input_views[0];views[2]=views[3]=NULL;
            rhi_set_textures(0,4,views);rhi_set_shader(RHI_STAGE_VERTEX,field_vs);rhi_buffer_update(vertex_buffers[0],vertex_constants[1]);rhi_set_shader(RHI_STAGE_PIXEL,field_ps[1]);
            rhi_set_render_target(output_views[1],NULL);
            rhi_buffer_update(uniforms,&constants[1]);rhi_draw(SURFACE_WRAP?4:3,0);
            static float next[SURFACE_SIDE*SURFACE_SIDE*4];field_read(outputs[1],second_storage,next);unsigned fresh=0;
            for(int y=0;y<SURFACE_SIDE;y++)for(int x=0;x<SURFACE_SIDE;x++) {
                unsigned i=field_index(x,y,0);
                float v=field_b[i+1]-.5f+precise[0]*(field_b[i]-.5f+field_a[field_index(x-1,y+1,2)]-field_b[i+2]);
                float expected[2]={v+bias,precise[2]*(field_b[i+2]+precise[1]*v)};
                for(unsigned c=0;c<2;c++) {CHECK(expected[c]>0 && expected[c]<1);
                    float e=fabsf(next[i+c+1]-expected[c]);if(e>max_error)max_error=e;CHECK(e<tolerances[second_storage]);
                    quantization_error[1][c]+=next[i+c+1]-expected[c];
                    fresh|=next[i+c+1]!=field_a[i+c+1];}
            }
            changed+=fresh;memcpy(field_a,next,sizeof field_a);
        }
        if(!rate && storage<3)memcpy(reference_fields[storage],field_a,sizeof field_a);
        double sum=0,sum2=0,difference=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++) {double b=field_a[i*4+2],d=b-reference_fields[storage<3?storage:2][i*4+2];sum+=b;sum2+=b*b;difference+=d*d;}
        double mean=sum/(SURFACE_SIDE*SURFACE_SIDE),std=sqrt(fmax(0,sum2/(SURFACE_SIDE*SURFACE_SIDE)-mean*mean)),rms=sqrt(difference/(SURFACE_SIDE*SURFACE_SIDE));
        printf("spatial storage=%u fps=%u meanB=%.9f stdB=%.9f rms_to_60=%.9f changed=%u/%u max_error=%.9f\n",storage,fps,mean,std,rms,changed,fps,max_error);
        CHECK(changed>0);if(rate && !storage)CHECK(rms>.0001);
        if(storage>=3)CHECK(rms>.005);
        if(SURFACE_SMOOTHING)printf("smoothed-original storage=%u max_filter_error=%.9g\n",storage,smoothing_error);
        if(!rate)printf("spatial-rounding first=%u second=%u preserve_gb=%u mean_error_first_RGB=%.9g,%.9g,%.9g mean_error_second_GB=%.9g,%.9g\n",
            first_storage,second_storage,storage==5,quantization_error[0][0]/(fps*SURFACE_SIDE*SURFACE_SIDE),
            quantization_error[0][1]/(fps*SURFACE_SIDE*SURFACE_SIDE),quantization_error[0][2]/(fps*SURFACE_SIDE*SURFACE_SIDE),
            quantization_error[1][0]/(fps*SURFACE_SIDE*SURFACE_SIDE),quantization_error[1][1]/(fps*SURFACE_SIDE*SURFACE_SIDE));
        RhiView *none[4]={0};rhi_set_textures(0,4,none);rhi_set_render_target(NULL,NULL);
        for(unsigned i=0;i<2;i++){rhi_view_destroy(input_views[i]);rhi_image_destroy(inputs[i]);}
        for(unsigned pass=0;pass<2;pass++) {rhi_view_destroy(output_views[pass]);rhi_image_destroy(outputs[pass]);}
    }
    if(SURFACE_SMOOTHING) {
        double sum=0;for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=1;c<3;c++) {
            double d=reference_fields[0][i*4+c]-reference_fields[2][i*4+c];sum+=d*d;
        }
        printf("smoothed-original float_byte_rms_GB=%.9g\n",sqrt(sum/(SURFACE_SIDE*SURFACE_SIDE*2)));
        puts("PASS: original translated feedback shader with captured cardinal smoothing");
    }
    static float final_reference[3][SURFACE_SIDE*SURFACE_SIDE*4];
    static float final_bytes[3][SURFACE_SIDE*SURFACE_SIDE*4];
    static float final_output[SURFACE_SIDE*SURFACE_SIDE*4],final_previous[SURFACE_SIDE*SURFACE_SIDE*4];
    for(unsigned storage=0;storage<3;storage++) {
        rhi_image_update(final.packed,0,NULL,reference_fields[storage],SURFACE_SIDE*16,sizeof field_a);
        field_final_run(&final,final.packed_read,0,0,final_reference[storage]);
        field_final_check(reference_fields[storage],3,final_reference[storage],.00001f);
        field_final_run(&final,final.packed_read,0,1,final_bytes[storage]);
        field_final_check(reference_fields[storage],3,final_bytes[storage],.501f/255);
        double variation=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
            double v=final_reference[storage][i*4+c]-128/255.;variation+=v*v;
        }
        printf("surface-final smoothing=%u storage=%u rms_RG_from_flat=%.9g\n",
            SURFACE_SMOOTHING,storage,sqrt(variation/(SURFACE_SIDE*SURFACE_SIDE*2)));
    }
    double final_gap=0,final_byte_gap=0;
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
        double v=final_reference[0][i*4+c]-final_reference[2][i*4+c];final_gap+=v*v;
        v=final_bytes[0][i*4+c]-final_bytes[2][i*4+c];final_byte_gap+=v*v;
    }
    printf("surface-final smoothing=%u float_byte_rms_RG=%.9g\n",SURFACE_SMOOTHING,
        sqrt(final_gap/(SURFACE_SIDE*SURFACE_SIDE*2)));
    printf("surface-final smoothing=%u byte_target_float_byte_rms_RG=%.9g\n",SURFACE_SMOOTHING,
        sqrt(final_byte_gap/(SURFACE_SIDE*SURFACE_SIDE*2)));
    if(!SURFACE_SMOOTHING) {
    rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    /* Exercise every spectral mode through the actual common-RHI GPU adapter.
     * Compare reconstructed G/B with the unmodified original shader pair's
     * 60-step float32 result above. Byte/half reference differences remain open. */
#define TSFP_SURFACE_VEC2 "float2"
#define TSFP_SURFACE_VEC3 "float3"
#include "graphics/surface_coupled_shader.h"
    const char spectral_hlsl[]=
        "Texture2D source:register(t0);Texture2D rows0:register(t1);Texture2D rows1:register(t2);"
        TSFP_SURFACE_COUPLED_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {int3 at=int3(p.xy,0);"
        "float2 state=surface_coupled(source.Load(at).xy,rows0.Load(at).xyz,rows1.Load(at).xyz);"
        "return float4(state,0,1);}";
    RhiShader *spectral_ps=shader(RHI_STAGE_PIXEL,spectral_hlsl,"ps_5_0");
    rhi_set_shader(RHI_STAGE_PIXEL,spectral_ps);
    static float row_data[2][SURFACE_SIDE*SURFACE_SIDE*4],spectrum[SURFACE_SIDE*SURFACE_SIDE*4],previous_spectrum[SURFACE_SIDE*SURFACE_SIDE*4];
    const double original_a=packed_coefficients[0][0]/255.,original_b=packed_coefficients[0][1]/255.,
        original_d=packed_coefficients[0][2]/255.,original_k=43./255,center=128./255;
#define TSFP_SURFACE_IVEC2 "int2"
#include "graphics/surface_transform_shader.h"
    const char transform_prefix[]=
        "Texture2D source:register(t0);Texture2D basis:register(t1);"
        "float2 surface_load(int2 at) {return source.Load(int3(at,0)).xy;}"
        "float surface_basis(int frequency,int position) {return basis.Load(int3(position,frequency,0)).x;}"
        TSFP_SURFACE_TRANSFORM_SHADER;
    char transform_text[4096];
    for(unsigned inverse=0;inverse<2;inverse++)for(unsigned axis=0;axis<2;axis++) {
        snprintf(transform_text,sizeof transform_text,
            "%sfloat4 main(float4 p:SV_POSITION):SV_TARGET {return float4(surface_transform(int2(p.xy),%u,%u,%u),0,1);}",
            transform_prefix,axis,inverse,SURFACE_SIDE);
        transform_ps[inverse][axis]=shader(RHI_STAGE_PIXEL,transform_text,"ps_5_0");
    }
    for(unsigned k=0;k<SURFACE_SIDE;k++)for(unsigned x=0;x<SURFACE_SIDE;x++) {
        unsigned i=(k*SURFACE_SIDE+x)*4;
        spectrum[i]=(float)field_weight(k,x);
        spectrum[i+1]=spectrum[i+2]=spectrum[i+3]=0;
    }
    td.width=td.height=SURFACE_SIDE;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData basis_data={spectrum,SURFACE_SIDE*16,sizeof spectrum};
    RhiImage *basis_image=rhi_image_create(&td,&basis_data);CHECK(basis_image);
    transform_basis=rhi_view_create(basis_image,RHI_VIEW_SAMPLED,NULL);CHECK(transform_basis);
    RhiImage *world_input=rhi_image_create(&td,NULL);CHECK(world_input);
    RhiView *world_read=rhi_view_create(world_input,RHI_VIEW_SAMPLED,NULL);CHECK(world_read);
    td.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
    RhiImage *transform_scratch=rhi_image_create(&td,NULL);CHECK(transform_scratch);
    transform_scratch_read=rhi_view_create(transform_scratch,RHI_VIEW_SAMPLED,NULL);
    transform_scratch_write=rhi_view_create(transform_scratch,RHI_VIEW_RENDER_TARGET,NULL);
    CHECK(transform_scratch_read && transform_scratch_write);
    /* Asymmetric corners, checkerboard and deterministic noise expose axis,
     * transpose and high-frequency errors hidden by a centered Gaussian. */
    RhiImage *probe[2];RhiView *probe_read[2],*probe_write[2];
    for(unsigned i=0;i<2;i++) {
        probe[i]=rhi_image_create(&td,NULL);CHECK(probe[i]);
        probe_read[i]=rhi_view_create(probe[i],RHI_VIEW_SAMPLED,NULL);
        probe_write[i]=rhi_view_create(probe[i],RHI_VIEW_RENDER_TARGET,NULL);
        CHECK(probe_read[i] && probe_write[i]);
    }
    for(unsigned pattern=0;pattern<3;pattern++) {
        unsigned random=0x12345678;
        for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++)for(unsigned c=0;c<4;c++) {
            random=random*1664525u+1013904223u;
            float v=pattern==0?((x==(c?(SURFACE_SIDE-1):0) && y==(c?0:(SURFACE_SIDE-1)))?.75f:0):
                pattern==1?(((x+y+c)&1)?.25f:-.25f):((random>>8)/16777216.f-.5f);
            field_a[(y*SURFACE_SIDE+x)*4+c]=c<2?v:0;
        }
        field_transform(field_a,previous_spectrum,0);
        rhi_image_update(world_input,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
        field_gpu_transform(world_read,probe_write[0],0);field_read(probe[0],0,spectrum);
        double transform_error=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
            CHECK(isfinite(spectrum[i*4+c]));
            transform_error=fmax(transform_error,fabs(spectrum[i*4+c]-previous_spectrum[i*4+c]));
        }
        CHECK(transform_error<.00001);
        field_gpu_transform(probe_read[0],probe_write[1],1);field_read(probe[1],0,spectrum);
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
            CHECK(isfinite(spectrum[i*4+c]));
            transform_error=fmax(transform_error,fabs(spectrum[i*4+c]-field_a[i*4+c]));
        }
        printf("spatial-transform pattern=%u max_error=%.9g\n",pattern,transform_error);
        CHECK(transform_error<.00001);
    }
    for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];unsigned final_fresh=0;
        memset(final_previous,0,sizeof final_previous);
        for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++) {
            unsigned i=(y*SURFACE_SIDE+x)*4;
            field_a[i]=0;field_a[i+1]=field_initial(x,y);
            field_a[i+2]=0;field_a[i+3]=1;
            double shift=pc_surface_stencil_shift(SURFACE_SIDE,SURFACE_SIDE,x,y,(PcSurfaceBoundary)SURFACE_BASIS_WRAP);
            double dc=(!x && !y)?(double)SURFACE_SIDE:0.;PcSurfaceCoupledStep matrix;
            CHECK(pc_surface_coupled_step(original_a,original_b,original_d,original_k+shift,
                dc*(center-.5+original_k*center),dc*(center-.5),60./fps,&matrix));
            for(unsigned row=0;row<2;row++)memcpy(&row_data[row][i],matrix.row[row],16);
        }
        rhi_image_update(world_input,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
        td.width=td.height=SURFACE_SIDE;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        RhiImage *row_images[2];RhiView *row_views[2];
        for(unsigned i=0;i<2;i++) {
            RhiSubresourceData data={row_data[i],SURFACE_SIDE*16,sizeof row_data[i]};
            row_images[i]=rhi_image_create(&td,&data);CHECK(row_images[i]);
            row_views[i]=rhi_view_create(row_images[i],RHI_VIEW_SAMPLED,NULL);CHECK(row_views[i]);
        }
        td.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
        RhiImage *states[2]={rhi_image_create(&td,NULL),rhi_image_create(&td,NULL)};
        CHECK(states[0] && states[1]);RhiView *reads[2],*writes[2];
        for(unsigned i=0;i<2;i++) {
            reads[i]=rhi_view_create(states[i],RHI_VIEW_SAMPLED,NULL);
            writes[i]=rhi_view_create(states[i],RHI_VIEW_RENDER_TARGET,NULL);CHECK(reads[i] && writes[i]);
        }
        field_gpu_transform(world_read,writes[0],0);
        field_read(states[0],0,previous_spectrum);
        rhi_set_shader(RHI_STAGE_PIXEL,spectral_ps);
        memcpy(field_b,field_a,sizeof field_a);
        unsigned current=0,fresh=0,world_fresh=0;
        for(unsigned frame=0;frame<fps;frame++) {
            unsigned next=current^1;RhiView *none[3]={0};rhi_set_textures(0,3,none);
            rhi_set_render_target(writes[next],NULL);
            RhiView *sources[3]={reads[current],row_views[0],row_views[1]};rhi_set_textures(0,3,sources);
            rhi_set_shader(RHI_STAGE_PIXEL,spectral_ps);
            rhi_draw(3,0);field_read(states[next],0,spectrum);
            int changed=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
                CHECK(isfinite(spectrum[i*4+c]));changed|=spectrum[i*4+c]!=previous_spectrum[i*4+c];
            }
            fresh+=changed;memcpy(previous_spectrum,spectrum,sizeof spectrum);current=next;
            field_gpu_transform(reads[current],probe_write[1],1);
            field_read(probe[1],0,field_a);changed=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
                CHECK(isfinite(field_a[i*4+c]));changed|=field_a[i*4+c]!=field_b[i*4+c];
            }
            world_fresh+=changed;memcpy(field_b,field_a,sizeof field_a);
            field_final_run(&final,probe_read[1],1,0,final_output);
            field_final_check(field_a,1,final_output,.00001f);changed=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++)
                changed|=final_output[i*4+c]!=final_previous[i*4+c];
            final_fresh+=changed;memcpy(final_previous,final_output,sizeof final_output);
        }
        CHECK(fresh==fps && world_fresh==fps);
        double max_error=0,rms=0,byte_rms=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<2;c++) {
            double value=field_a[i*4+c]+(c?0:.5);
            double error=value-reference_fields[0][i*4+c+1];
            max_error=fmax(max_error,fabs(error));rms+=error*error;
            double byte_error=value-reference_fields[2][i*4+c+1];byte_rms+=byte_error*byte_error;
        }
        rms=sqrt(rms/(SURFACE_SIDE*SURFACE_SIDE*2));byte_rms=sqrt(byte_rms/(SURFACE_SIDE*SURFACE_SIDE*2));
        printf("spatial-coupled size=%u wrap=%u fps=%u rms_to_float60=%.9g max_error=%.9g rms_to_byte60=%.9g fresh=%u/%u world_fresh=%u/%u\n",
            SURFACE_SIDE,SURFACE_WRAP,fps,rms,max_error,byte_rms,fresh,fps,world_fresh,fps);
        CHECK(max_error<.00003);CHECK(byte_rms>.001);
        double final_error=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<3;c++)
            final_error=fmax(final_error,fabs(final_output[i*4+c]-final_reference[0][i*4+c]));
        printf("surface-final coupled fps=%u max_float_reference_error=%.9g fresh=%u/%u\n",
            fps,final_error,final_fresh,fps);
        CHECK(final_error<.00003);CHECK(final_fresh==fps);
        field_final_run(&final,probe_read[1],1,1,final_output);
        final_error=0;
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<3;c++)
            final_error=fmax(final_error,fabs(final_output[i*4+c]-final_bytes[0][i*4+c]));
        printf("surface-final coupled fps=%u byte_target_max_error=%.9g\n",fps,final_error);
        CHECK(final_error<1.001/255);
        RhiView *none[3]={0};rhi_set_textures(0,3,none);rhi_set_render_target(NULL,NULL);
        for(unsigned i=0;i<2;i++) {
            rhi_view_destroy(reads[i]);rhi_view_destroy(writes[i]);rhi_image_destroy(states[i]);
            rhi_view_destroy(row_views[i]);rhi_image_destroy(row_images[i]);
        }
    }
    for(unsigned i=0;i<2;i++) {rhi_view_destroy(probe_read[i]);rhi_view_destroy(probe_write[i]);rhi_image_destroy(probe[i]);}
    rhi_view_destroy(world_read);rhi_image_destroy(world_input);
    rhi_view_destroy(transform_basis);rhi_image_destroy(basis_image);
    rhi_view_destroy(transform_scratch_read);rhi_view_destroy(transform_scratch_write);rhi_image_destroy(transform_scratch);
    for(unsigned inverse=0;inverse<2;inverse++)for(unsigned axis=0;axis<2;axis++)rhi_shader_destroy(transform_ps[inverse][axis]);
    rhi_shader_destroy(spectral_ps);
    puts("PASS: original translated feedback shader pair matched by coupled spectral GPU steps (float32 only)");
    puts("PASS: original translated feedback shader pair verifies spatial coupling and storage gaps");
    }
    rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    field_final_destroy(&final);
    rhi_shader_destroy(field_vs);
    if(filter_ps)rhi_shader_destroy(filter_ps);
    for(unsigned i=0;i<2;i++)rhi_shader_destroy(field_ps[i]);
    for(unsigned i=0;i<3;i++)rhi_buffer_destroy(vertex_buffers[i]);
    rhi_buffer_destroy(uniforms);rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);rhi_device_destroy();return 0;
}
