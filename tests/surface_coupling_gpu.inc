/* Original shader math diagnostic, appended to the shared RHI fixture setup.
 * Reads a locally extracted definition; no game shader bytes are tracked. */
    RhiShader *surface_vs=shader(RHI_STAGE_VERTEX,
        "struct O {float4 pos:SV_POSITION;float4 c0:COLOR0;float4 c1:COLOR1;"
        "float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};"
        "O main(uint id:SV_VertexID) {O o=(O)0;o.pos=float4(id==2?3:-1,id==1?3:-1,0,1);"
        "o.t0=o.t1=o.t2=o.t3=float4(.5,.5,0,1);return o;}","vs_5_0");
    RhiShader *surface_ps=shader(RHI_STAGE_PIXEL,surface_hlsl,"ps_5_0");
    RhiBufferDesc constants_desc={sizeof(NV2APSConstants),RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *uniforms=rhi_buffer_create(&constants_desc,NULL);CHECK(uniforms);
    DWORD definition[60];
    FILE *input_definition=fopen("feedback.bin","rb");CHECK(input_definition);
    CHECK(fread(definition,sizeof(DWORD),60,input_definition)==60);fclose(input_definition);
    CHECK(definition[58]==0xf5ffffffu); /* Only stage-6 C1 is a static centering value. */
    /* Controlled fixed-neighbor oscillator, not a complete wave simulation.
     * t0 supplies persistent G/B; t1 supplies a fixed neutral neighbor. Same initial
     * state at every rate/format; no RNG, smoothing or scene scheduling. */
    const unsigned rates[]={60,120,144};
    const uint32_t formats[]={RHI_FORMAT_R32G32B32A32_FLOAT,
        RHI_FORMAT_R16G16B16A16_FLOAT,RHI_FORMAT_R8G8B8A8_UNORM};
    float finals[3][3][2]={0};
    for(unsigned storage=0;storage<3;storage++)for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];float delta=60.f/fps;
        const float authored[3]={.105f,.65f,.98f};float precise[3];
        pc_surface_coefficients(delta,authored,precise);
        for(unsigned i=0;i<3;i++)CHECK(pc_surface_packed_coefficient(authored[i])==packed_coefficients[0][i]/255.f);
        NV2APSConstants cb={0};for(unsigned i=0;i<4;i++)cb.tex_scale[i][0]=cb.tex_scale[i][1]=1;
        float vectors[3][4]={{0,precise[0],0,0},{0,0,precise[1],precise[1]},
            {0,0,precise[2],precise[2]}};
        const unsigned shifts[4]={16,8,0,24};
        for(unsigned bank=0;bank<2;bank++)for(unsigned stage=0;stage<8;stage++) {
            float *dst=bank?cb.c1[stage]:cb.c0[stage];
            unsigned mapped=(definition[57+bank]>>(stage*4))&15;
            for(unsigned channel=0;channel<4;channel++)dst[channel]=mapped<3?vectors[mapped][channel]:
                ((definition[10+bank*8+stage]>>shifts[channel])&255)/255.f;
        }
        cb.c1[6][1]=pc_surface_bias(delta);
        rhi_buffer_update(uniforms,&cb);
        td.width=td.height=64;td.format=formats[storage];td.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *output=rhi_image_create(&td,NULL);CHECK(output);
        RhiView *output_view=rhi_view_create(output,RHI_VIEW_RENDER_TARGET,NULL);CHECK(output_view);
        rhi_set_render_target(output_view,NULL);
        td.width=td.height=1;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        float state[4]={.5f,.5f,.65f,1},neighbor[4]={.5f,.5f,.5f,1};
        RhiSubresourceData data={state,sizeof state,sizeof state};
        RhiImage *input=rhi_image_create(&td,&data);CHECK(input);
        data.data=neighbor;
        RhiImage *neighbor_input=rhi_image_create(&td,&data);CHECK(neighbor_input);
        RhiView *view=rhi_view_create(input,RHI_VIEW_SAMPLED,NULL);CHECK(view);
        RhiView *neighbor_view=rhi_view_create(neighbor_input,RHI_VIEW_SAMPLED,NULL);CHECK(neighbor_view);
        RhiView *views[2]={view,neighbor_view};RhiSampler *samplers[2]={sampler,sampler};
        rhi_set_textures(0,2,views);rhi_set_samplers(0,2,samplers);
        rhi_set_shader(RHI_STAGE_VERTEX,surface_vs);rhi_set_shader(RHI_STAGE_PIXEL,surface_ps);
        rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&uniforms);
        union {float f[64*64*4];uint16_t h[64*64*4];uint8_t b[64*64*4];} result;
        float error_max=0;unsigned fresh=0;
        unsigned stride=storage==0?16:storage==1?8:4;
        for(unsigned step=0;step<fps;step++) {
            float v=state[1]-.5f+precise[0]*(state[0]-.5f+neighbor[2]-state[2]);
            float expected[2]={v+cb.c1[6][1],precise[2]*(state[2]+precise[1]*v)};
            CHECK(expected[0]>0 && expected[0]<1 && expected[1]>0 && expected[1]<1);
            rhi_image_update(input,0,NULL,state,sizeof state,sizeof state);
            rhi_draw(3,0);CHECK(rhi_image_readback(output,0,&result,64*stride)==0);
            float next[2]={0};
            for(unsigned i=0;i<64*64;i++)for(unsigned c=0;c<2;c++) {
                unsigned k=i*4+c+1;float observed;
                if(storage==0)observed=result.f[k];
                else if(storage==2)observed=result.b[k]/255.f;
                else {unsigned h=result.h[k],e=(h>>10)&31,m=h&1023;CHECK(e!=31);
                    observed=ldexpf((float)(e?1024+m:m),e?(int)e-25:-24);if(h&32768)observed=-observed;}
                float error=fabsf(observed-expected[c]);if(error>error_max)error_max=error;
                CHECK(error<(storage==0?.000003f:storage==1?.0005f:.501f/255.f));
                if(!i)next[c]=observed;else CHECK(observed==next[c]);
            }
            if(next[0]!=state[1] || next[1]!=state[2])fresh++;
            state[1]=next[0];state[2]=next[1];
        }
        finals[storage][rate][0]=state[1];finals[storage][rate][1]=state[2];
        printf("coupled storage=%u fps=%u G=%.9f B=%.9f fresh=%u/%u max_storage_error=%.9f\n",
            storage,fps,state[1],state[2],fresh,fps,error_max);
        RhiView *none[2]={0};rhi_set_textures(0,2,none);rhi_set_render_target(NULL,NULL);
        rhi_view_destroy(view);rhi_image_destroy(input);rhi_view_destroy(neighbor_view);rhi_image_destroy(neighbor_input);
        rhi_view_destroy(output_view);rhi_image_destroy(output);
    }
    /* These are characterization controls, not acceptance thresholds. Neither
     * eliminating coefficient packing nor using float buffers proves coupled
     * one-tick equivalence. Fail if this fixture stops exercising those gaps. */
    CHECK(fabsf(finals[0][0][1]-finals[0][1][1])>.0001f);
    CHECK(fabsf(finals[0][0][1]-finals[0][2][1])>.0001f);
    CHECK(fabsf(finals[0][0][1]-finals[2][0][1])>.0001f);
    /* The corrected coupled operator is evaluated on the GPU, then compared
     * with the original translated shader's full 60-step float32 trajectory.
     * This is the same fixed-neighbor case, not a replacement spatial solver. */
#define TSFP_SURFACE_VEC2 "float2"
#define TSFP_SURFACE_VEC3 "float3"
#include "graphics/surface_coupled_shader.h"
    const char coupled_hlsl[]=
        "Texture2D source:register(t0);SamplerState smp:register(s0);"
        "cbuffer Step:register(b0){float4 row0;float4 row1;}"
        TSFP_SURFACE_COUPLED_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {"
        "float4 value=source.Sample(smp,float2(.5,.5));"
        "float2 state=surface_coupled(float2(value.g-.5,value.b),row0.xyz,row1.xyz);"
        "return float4(.5,state.x+.5,state.y,1);}";
    RhiShader *coupled_ps=shader(RHI_STAGE_PIXEL,coupled_hlsl,"ps_5_0");
    RhiBufferDesc coupled_desc={sizeof(PcSurfaceCoupledStep),RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *coupled_uniforms=rhi_buffer_create(&coupled_desc,NULL);CHECK(coupled_uniforms);
    float coupled_max_error=0;
    for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];PcSurfaceCoupledStep matrix;
        CHECK(pc_surface_coupled_step(packed_coefficients[0][0]/255.,
            packed_coefficients[0][1]/255.,packed_coefficients[0][2]/255.,
            1,.5,128./255-.5,60./fps,&matrix));
        rhi_buffer_update(coupled_uniforms,&matrix);
        rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&coupled_uniforms);
        rhi_set_shader(RHI_STAGE_PIXEL,coupled_ps);
        td.width=td.height=64;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *output=rhi_image_create(&td,NULL);CHECK(output);
        RhiView *output_view=rhi_view_create(output,RHI_VIEW_RENDER_TARGET,NULL);CHECK(output_view);
        rhi_set_render_target(output_view,NULL);
        td.width=td.height=1;td.bind=RHI_BIND_SAMPLED;
        float state[4]={.5f,.5f,.65f,1};RhiSubresourceData data={state,sizeof state,sizeof state};
        RhiImage *input=rhi_image_create(&td,&data);CHECK(input);
        RhiView *view=rhi_view_create(input,RHI_VIEW_SAMPLED,NULL);CHECK(view);
        rhi_set_textures(0,1,&view);
        float pixels[64*64*4];unsigned fresh=0;
        for(unsigned frame=0;frame<fps;frame++) {
            rhi_image_update(input,0,NULL,state,sizeof state,sizeof state);
            rhi_draw(3,0);CHECK(rhi_image_readback(output,0,pixels,64*16)==0);
            if(pixels[1]!=state[1] || pixels[2]!=state[2])fresh++;
            for(unsigned i=0;i<64*64;i++) {
                CHECK(isfinite(pixels[4*i+1]) && isfinite(pixels[4*i+2]));
                CHECK(pixels[4*i+1]==pixels[1] && pixels[4*i+2]==pixels[2]);
            }
            state[1]=pixels[1];state[2]=pixels[2];
        }
        CHECK(fresh==fps);
        for(unsigned i=0;i<2;i++) {
            float error=fabsf(state[i+1]-finals[0][0][i]);
            if(error>coupled_max_error)coupled_max_error=error;
            CHECK(error<.00001f);
        }
        printf("coupled-corrected fps=%u G=%.9f B=%.9f fresh=%u/%u\n",fps,state[1],state[2],fresh,fps);
        RhiView *none=NULL;rhi_set_textures(0,1,&none);rhi_set_render_target(NULL,NULL);
        rhi_view_destroy(view);rhi_image_destroy(input);rhi_view_destroy(output_view);rhi_image_destroy(output);
    }
    printf("PASS: original translated feedback shader reference matched by coupled GPU step; max error %.9g\n",coupled_max_error);
    rhi_buffer_destroy(coupled_uniforms);rhi_shader_destroy(coupled_ps);
    puts("PASS: original translated feedback shader separates coupled timestep and storage effects");
    rhi_set_render_target(NULL,NULL);rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    rhi_buffer_destroy(uniforms);rhi_shader_destroy(surface_vs);rhi_shader_destroy(surface_ps);
    rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);rhi_device_destroy();
    return 0;
}
