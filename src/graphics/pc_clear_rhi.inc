/* A self-contained RHI pass for Xbox per-channel colour clears. The ordinary
 * draw path rebinds pipeline state; only output targets/viewports are saved,
 * following rhi.h's overlay/screen-copy contract. No guest state is changed. */
static struct {
    RhiShader *vs, *ps;
    RhiBuffer *color;
    RhiBlendState *blend[16];
    RhiDepthState *depth;
    RhiRasterState *raster;
    int failed;
} pc_clear;

static void pc_clear_shutdown(void)
{
    unsigned i;
    rhi_shader_destroy(pc_clear.vs);rhi_shader_destroy(pc_clear.ps);
    rhi_buffer_destroy(pc_clear.color);
    rhi_depth_state_destroy(pc_clear.depth);rhi_raster_state_destroy(pc_clear.raster);
    for(i=0;i<16;i++)rhi_blend_state_destroy(pc_clear.blend[i]);
    memset(&pc_clear,0,sizeof pc_clear);
}

static int pc_clear_create(unsigned mask)
{
    static const char hlsl[]=
        "cbuffer ClearColor : register(b7) { float4 color; };\n"
        "float4 vs_main(uint id : SV_VertexID) : SV_Position {\n"
        "float2 p=float2((id << 1) & 2,id & 2);\n"
        "return float4(p*float2(2,-2)+float2(-1,1),0,1); }\n"
        "float4 ps_main() : SV_Target { return color; }\n";
    RhiShaderSource source={0};
    RhiBufferDesc buffer={0};
    RhiBlendDesc blend={0};
    RhiDepthDesc depth={0};
    RhiRasterDesc raster={0};
    char error[512]={0};
    if(pc_clear.failed)return 0;
    if(!pc_clear.vs) {
        source.hlsl=hlsl;source.len=sizeof hlsl-1;source.name="channel-clear";
        source.entry="vs_main";source.target="vs_4_0";
        pc_clear.vs=rhi_shader_create(RHI_STAGE_VERTEX,&source,error,sizeof error);
        if(!pc_clear.vs)goto failed;
    }
    if(!pc_clear.ps) {
        source.hlsl=hlsl;source.len=sizeof hlsl-1;source.name="channel-clear";
        source.entry="ps_main";source.target="ps_4_0";
        pc_clear.ps=rhi_shader_create(RHI_STAGE_PIXEL,&source,error,sizeof error);
        if(!pc_clear.ps)goto failed;
    }
    if(!pc_clear.color) {
        buffer.size=sizeof(float)*4;buffer.usage=RHI_USAGE_DYNAMIC;
        buffer.bind=RHI_BIND_UNIFORM;buffer.cpu_access=RHI_CPU_WRITE;
        if(!(pc_clear.color=rhi_buffer_create(&buffer,NULL)))goto failed;
    }
    if(!pc_clear.depth && !(pc_clear.depth=rhi_depth_state_create(&depth)))goto failed;
    if(!pc_clear.raster) {
        raster.fill=RHI_FILL_SOLID;raster.cull=RHI_CULL_NONE;raster.depth_clip=1;
        if(!(pc_clear.raster=rhi_raster_state_create(&raster)))goto failed;
    }
    if(!pc_clear.blend[mask]) {
        blend.write_mask=mask;
        if(!(pc_clear.blend[mask]=rhi_blend_state_create(&blend)))goto failed;
    }
    return 1;
failed:
    fprintf(stderr,"D3D8: per-channel clear setup failed: %s\n",error);
    pc_clear_shutdown();pc_clear.failed=1;return 0;
}

static int pc_clear_masked(RhiView *target,unsigned width,unsigned height,
                           const float rgba[4],unsigned mask)
{
    RhiOutputState saved;
    RhiViewport viewport={0};
    void *mapped;
    mask &= 15;
    if(!target || !width || !height || !mask)return 1;
    if(!pc_clear_create(mask))return 0;
    mapped=rhi_buffer_map(pc_clear.color,RHI_MAP_WRITE_DISCARD);
    if(!mapped)return 0;
    memcpy(mapped,rgba,sizeof(float)*4);rhi_buffer_unmap(pc_clear.color);
    viewport.width=(float)width;viewport.height=(float)height;viewport.max_depth=1;
    rhi_output_save(&saved);
    rhi_set_render_target(target,NULL);rhi_set_viewports(1,&viewport);
    rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,pc_clear.vs);rhi_set_shader(RHI_STAGE_PIXEL,pc_clear.ps);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&pc_clear.color);
    rhi_set_blend_state(pc_clear.blend[mask],NULL,0xFFFFFFFFu);
    rhi_set_depth_state(pc_clear.depth,0);rhi_set_raster_state(pc_clear.raster);
    rhi_draw(3,0);
    rhi_output_restore(&saved);
    return 1;
}
