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
    /* Isolate displacement damping: acceleration/displacement coupling are
     * zero, inputs are uniform, storage is float32, and only B feeds back.
     * This diagnoses coefficient packing, not a complete water trajectory. */
    const unsigned rates[]={60,120,144};
    const float original=packed_coefficients[0][2]/255.f;
    const float reference=.6f*powf(original,60.f);
    for(unsigned corrected=0;corrected<2;corrected++)for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];float delta=60.f/fps;
        const float authored[3]={.105f,.65f,.98f};float precise[3];
        pc_surface_coefficients(delta,authored,precise);
        for(unsigned i=0;i<3;i++)CHECK(pc_surface_packed_coefficient(authored[i])==packed_coefficients[0][i]/255.f);
        float damping=corrected?precise[2]:packed_coefficients[rate][2]/255.f;
        NV2APSConstants cb={0};for(unsigned i=0;i<4;i++)cb.tex_scale[i][0]=cb.tex_scale[i][1]=1;
        float vectors[3][4]={{0},{0},{0,0,damping,damping}};
        const unsigned shifts[4]={16,8,0,24};
        for(unsigned bank=0;bank<2;bank++)for(unsigned stage=0;stage<8;stage++) {
            float *dst=bank?cb.c1[stage]:cb.c0[stage];
            unsigned mapped=(definition[57+bank]>>(stage*4))&15;
            for(unsigned channel=0;channel<4;channel++)dst[channel]=mapped<3?vectors[mapped][channel]:
                ((definition[10+bank*8+stage]>>shifts[channel])&255)/255.f;
        }
        /* Corrected coefficient is a diagnostic uniform; production still
         * uses packed constants. Delta 1 retains the original packed value. */
        CHECK(cb.c0[7][2]==damping);
        rhi_buffer_update(uniforms,&cb);
        td.width=td.height=1;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        float state[4]={.5f,.5f,.6f,1};
        RhiSubresourceData data={state,sizeof state,sizeof state};
        RhiImage *input=rhi_image_create(&td,&data);CHECK(input);
        RhiView *view=rhi_view_create(input,RHI_VIEW_SAMPLED,NULL);CHECK(view);
        RhiView *views[2]={view,view};RhiSampler *samplers[2]={sampler,sampler};
        rhi_set_textures(0,2,views);rhi_set_samplers(0,2,samplers);
        rhi_set_shader(RHI_STAGE_VERTEX,surface_vs);rhi_set_shader(RHI_STAGE_PIXEL,surface_ps);
        rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&uniforms);
        float result[64*64*4];
        for(unsigned step=0;step<fps;step++) {
            rhi_image_update(input,0,NULL,state,sizeof state,sizeof state);
            rhi_draw(3,0);CHECK(rhi_image_readback(target,0,result,64*4*sizeof(float))==0);
            for(unsigned i=0;i<64*64;i++)CHECK(fabsf(result[i*4+2]-state[2]*damping)<1e-6f);
            state[2]=result[2];
        }
        float error=state[2]/reference-1.f;
        printf("damping corrected=%u fps=%u coefficient=%.9f final=%.9f reference=%.9f relative_error=%.7f\n",
            corrected,fps,damping,state[2],reference,error);
        if(corrected || !rate)CHECK(fabsf(state[2]-reference)<.000005f);
        else CHECK(fabsf(error)>.001f);
        RhiView *none[2]={0};rhi_set_textures(0,2,none);rhi_view_destroy(view);rhi_image_destroy(input);
    }
    puts("PASS: original translated feedback shader exposes packed fractional damping drift");
    rhi_set_render_target(NULL,NULL);rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    rhi_buffer_destroy(uniforms);rhi_shader_destroy(surface_vs);rhi_shader_destroy(surface_ps);
    rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);rhi_device_destroy();
    return 0;
}
