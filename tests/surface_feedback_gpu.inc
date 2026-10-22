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
    const float state_values[][4]={{.5f,.5f,.5f,1},{.7f,.2f,.4f,1},{.3f,.8f,.6f,1}};
    for(unsigned state=0;state<3;state++) {
        td.width=td.height=1;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        RhiSubresourceData data={state_values[state],16,16};RhiImage *input=rhi_image_create(&td,&data);CHECK(input);
        RhiView *view=rhi_view_create(input,RHI_VIEW_SAMPLED,NULL);CHECK(view);
        RhiView *views[2]={view,view};RhiSampler *samplers[2]={sampler,sampler};
        rhi_set_textures(0,2,views);rhi_set_samplers(0,2,samplers);
        rhi_set_shader(RHI_STAGE_VERTEX,surface_vs);rhi_set_shader(RHI_STAGE_PIXEL,surface_ps);
        rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&uniforms);
        float original_tick[64*64*4];
        const float deltas[]={0,.0001f,60.f/144,60.f/120,1};
        for(unsigned corrected=0;corrected<2;corrected++)for(unsigned step=0;step<5;step++) {
            const float delta=deltas[step];
            NV2APSConstants cb={0};for(unsigned i=0;i<4;i++)cb.tex_scale[i][0]=cb.tex_scale[i][1]=1;
            /* Use original static constants and the XDK mapping of the three
             * vectors uploaded by 0x195110. Quantization matches its 0..255
             * constant upload, not an imagined floating-point uniform path. */
            float vectors[3][4]={{0,pc_surface_packed_coefficient(.105f*delta),0,0},
                {0,0,pc_surface_packed_coefficient(.65f*delta),pc_surface_packed_coefficient(.65f*delta)},
                {0,0,pc_surface_packed_coefficient(powf(.98f,delta)),pc_surface_packed_coefficient(powf(.98f,delta))}};
            const unsigned shifts[4]={16,8,0,24};
            for(unsigned bank=0;bank<2;bank++)for(unsigned stage=0;stage<8;stage++) {
                float *dst=bank?cb.c1[stage]:cb.c0[stage];
                unsigned mapped=(definition[57+bank]>>(stage*4))&15;
                for(unsigned channel=0;channel<4;channel++)
                    dst[channel]=mapped<3?vectors[mapped][channel]:
                        ((definition[10+bank*8+stage]>>shifts[channel])&255)/255.f;
            }
            CHECK(cb.c1[6][1]==128/255.f);
            PcCombinerConstants overrides={0};
            if(corrected) {
                const float value[4]={0,pc_surface_bias(delta),0,0};
                CHECK(pc_combiner_constant_set(&overrides,1,6,value));
            }
            /* Same lookup used by production upload; untouched slots retain
             * the original packed constants. */
            for(unsigned i=0;i<8;i++) {
                pc_combiner_constant_get(&overrides,0,i,cb.c0[i]);
                pc_combiner_constant_get(&overrides,1,i,cb.c1[i]);
            }
            rhi_buffer_update(uniforms,&cb);rhi_draw(3,0);
            float result[64*64*4];CHECK(rhi_image_readback(target,0,result,64*4*sizeof(float))==0);
            if(step==4) {
                if(!corrected)memcpy(original_tick,result,sizeof result);
                else CHECK(memcmp(original_tick,result,sizeof result)==0);
            }
            const float *v=state_values[state];
            float velocity=v[1]-.5f+(v[0]-.5f)*cb.c0[3][1];
            float green=velocity+cb.c1[6][1];
            float blue=(v[2]+velocity*cb.c0[5][2])*cb.c0[7][2];
            for(unsigned i=0;i<64*64;i++) {
                CHECK(fabsf(result[i*4+1]-green)<1e-6f && fabsf(result[i*4+2]-blue)<1e-6f);
                if(!step)CHECK(fabsf(result[i*4+1]-v[1]-(corrected?0:128/255.f-.5f))<1e-6f);
            }
            printf("state=%u correction=%u delta=%.7f G=%.9f B=%.9f expectedG=%.9f expectedB=%.9f\n",state,corrected,delta,result[1],result[2],green,blue);
        }
        RhiView *none[2]={0};rhi_set_textures(0,2,none);rhi_view_destroy(view);rhi_image_destroy(input);
    }
    puts("PASS: original translated feedback shader zero-step bias and fractional centering correction");
    rhi_set_render_target(NULL,NULL);rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    rhi_buffer_destroy(uniforms);rhi_shader_destroy(surface_vs);rhi_shader_destroy(surface_ps);
    rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);rhi_device_destroy();
    return 0;
}
