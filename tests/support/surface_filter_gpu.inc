/* Original shader math diagnostic, appended to the shared RHI fixture setup.
 * Reads a locally extracted definition; no game shader bytes are tracked. */
    RhiShader *surface_vs=shader(RHI_STAGE_VERTEX,
        "struct O {float4 pos:SV_POSITION;float4 c0:COLOR0;float4 c1:COLOR1;"
        "float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};"
        "O main(uint id:SV_VertexID) {O o=(O)0;o.pos=float4(id==2?3:-1,id==1?3:-1,0,1);"
        "o.c0=127.0/255.0;o.t0=o.t1=o.t2=o.t3=float4(.5,.5,0,1);return o;}","vs_5_0");
    RhiShader *surface_ps=shader(RHI_STAGE_PIXEL,surface_hlsl,"ps_5_0");
    RhiBufferDesc constants_desc={sizeof(NV2APSConstants),RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *uniforms=rhi_buffer_create(&constants_desc,NULL);CHECK(uniforms);
    NV2APSConstants cb={0};for(unsigned i=0;i<4;i++)cb.tex_scale[i][0]=cb.tex_scale[i][1]=1;
    DWORD definition[60];FILE *input_definition=fopen("feedback.bin","rb");CHECK(input_definition);
    CHECK(fread(definition,sizeof(DWORD),60,input_definition)==60);fclose(input_definition);
    CHECK(definition[13]==0x80808080u);
    const unsigned shifts[4]={16,8,0,24};
    for(unsigned c=0;c<4;c++)cb.c0[3][c]=((definition[13]>>shifts[c])&255)/255.f;
    rhi_set_shader(RHI_STAGE_VERTEX,surface_vs);rhi_set_shader(RHI_STAGE_PIXEL,surface_ps);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&uniforms);rhi_buffer_update(uniforms,&cb);
    const float values[]={0,.2f,.5f,.8f,1};
    for(unsigned value=0;value<5;value++) {
        td.width=td.height=1;td.format=RHI_FORMAT_R32G32B32A32_FLOAT;td.bind=RHI_BIND_SAMPLED;
        float v=values[value],texel[4]={v,v,v,v};RhiSubresourceData data={texel,16,16};
        RhiImage *input=rhi_image_create(&td,&data);CHECK(input);
        RhiView *view=rhi_view_create(input,RHI_VIEW_SAMPLED,NULL);CHECK(view);
        RhiView *views[4]={view,view,view,view};RhiSampler *samplers[4]={sampler,sampler,sampler,sampler};
        rhi_set_textures(0,4,views);rhi_set_samplers(0,4,samplers);rhi_draw(3,0);
        float result[64*64*4];CHECK(rhi_image_readback(target,0,result,64*4*sizeof(float))==0);
        float expected=fminf(1,v-.5f+128/255.f)*(254/255.f);
        for(unsigned i=0;i<64*64*4;i++)CHECK(fabsf(result[i]-expected)<1e-6f);
        printf("zero_radius input=%.9f output=%.9f expected=%.9f drift=%.9f\n",v,result[0],expected,result[0]-v);
        RhiView *none[4]={0};rhi_set_textures(0,4,none);rhi_view_destroy(view);rhi_image_destroy(input);
    }
    puts("PASS: original translated feedback shader zero-radius filter is not an identity copy");
    rhi_set_render_target(NULL,NULL);rhi_set_shader(RHI_STAGE_VERTEX,NULL);rhi_set_shader(RHI_STAGE_PIXEL,NULL);
    rhi_buffer_destroy(uniforms);rhi_shader_destroy(surface_vs);rhi_shader_destroy(surface_ps);
    rhi_buffer_destroy(parameters);rhi_sampler_destroy(sampler);
    rhi_view_destroy(srv);rhi_image_destroy(texture);rhi_view_destroy(rtv);rhi_image_destroy(target);
    rhi_shader_destroy(ps);rhi_shader_destroy(vs);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);rhi_device_destroy();
    return 0;
}
