/* Shared RHI SMAA 1x, included by d3d8_display.c. All objects belong to the
 * render thread/device. No readback, history, guest state or timing changes. */
#include "graphics/smaa_shader.h"
#include "graphics/pc_smaa_settings.h"
#include "../../vendor/smaa/AreaTex.h"
#include "../../vendor/smaa/SearchTex.h"

typedef struct PcSmaaTarget { RhiImage *image; RhiView *rtv, *srv; } PcSmaaTarget;
static struct {
    PcSmaaTarget color, edges, weights, area, search;
    RhiShader *vs, *edge_ps, *weight_ps, *blend_ps;
    RhiBuffer *cb;
    RhiSampler *linear, *point;
    RhiBlendState *blend;
    RhiDepthState *depth;
    RhiRasterState *raster;
    unsigned width, height;
    int tried, failed;
} smaa;

static void pc_smaa_target_release(PcSmaaTarget *t)
{
    rhi_view_destroy(t->rtv); rhi_view_destroy(t->srv);
    rhi_image_destroy(t->image); memset(t, 0, sizeof *t);
}
static void pc_smaa_shutdown(void)
{
    pc_smaa_target_release(&smaa.color); pc_smaa_target_release(&smaa.edges);
    pc_smaa_target_release(&smaa.weights); pc_smaa_target_release(&smaa.area);
    pc_smaa_target_release(&smaa.search);
    rhi_shader_destroy(smaa.vs); rhi_shader_destroy(smaa.edge_ps);
    rhi_shader_destroy(smaa.weight_ps); rhi_shader_destroy(smaa.blend_ps);
    rhi_buffer_destroy(smaa.cb); rhi_sampler_destroy(smaa.linear);
    rhi_sampler_destroy(smaa.point); rhi_blend_state_destroy(smaa.blend);
    rhi_depth_state_destroy(smaa.depth); rhi_raster_state_destroy(smaa.raster);
    memset(&smaa, 0, sizeof smaa);
}
static int pc_smaa_fail(void)
{
    pc_smaa_shutdown(); smaa.failed = 1;
    fprintf(stderr, "[SMAA] unavailable; using the original display resolve\n");
    return 0;
}
static int pc_smaa_target(PcSmaaTarget *t, unsigned w, unsigned h,
                          RhiFormat format, const void *bytes, unsigned pitch)
{
    RhiImageDesc d = {0}; RhiViewDesc v = {0}; RhiSubresourceData initial = {0};
    d.width=w; d.height=h; d.depth=1; d.mip_levels=1;
    d.format=format; d.samples=1; d.bind=RHI_BIND_SAMPLED;
    d.usage=bytes ? RHI_USAGE_IMMUTABLE : RHI_USAGE_DEFAULT;
    if(!bytes) d.bind |= RHI_BIND_RENDER_TARGET;
    initial.data=bytes; initial.row_pitch=pitch;
    t->image=rhi_image_create(&d, bytes ? &initial : NULL);
    if(!t->image) return 0;
    v.mip_count=v.layer_count=1;
    t->srv=rhi_view_create(t->image,RHI_VIEW_SAMPLED,&v);
    if(!bytes) t->rtv=rhi_view_create(t->image,RHI_VIEW_RENDER_TARGET,&v);
    return t->srv && (bytes || t->rtv);
}
static RhiShader *pc_smaa_compile(unsigned stage, const char *entry)
{
    RhiShaderSource source={0}; char error[2048]={0};
    source.hlsl=tsfp_smaa_shader; source.len=sizeof tsfp_smaa_shader-1;
    source.name="smaa_1x"; source.entry=entry; source.optimize=1;
    source.target=stage==RHI_STAGE_VERTEX ? "vs_4_0" : "ps_4_0";
    RhiShader *shader=rhi_shader_create(stage,&source,error,sizeof error);
    if(!shader) fprintf(stderr,"[SMAA] %s: %s\n",entry,error);
    return shader;
}
static int pc_smaa_prepare(unsigned w, unsigned h)
{
    if(!pc_smaa_enabled() || smaa.failed || !w || !h) return 0;
    if(!smaa.tried) {
        RhiBufferDesc b={0}; RhiSamplerDesc p={0};
        RhiBlendDesc blend={0}; RhiDepthDesc depth={0}; RhiRasterDesc raster={0};
        smaa.tried=1;
        smaa.vs=pc_smaa_compile(RHI_STAGE_VERTEX,"smaa_vs");
        smaa.edge_ps=pc_smaa_compile(RHI_STAGE_PIXEL,"smaa_edges");
        smaa.weight_ps=pc_smaa_compile(RHI_STAGE_PIXEL,"smaa_weights");
        smaa.blend_ps=pc_smaa_compile(RHI_STAGE_PIXEL,"smaa_blend");
        if(!smaa.vs || !smaa.edge_ps || !smaa.weight_ps || !smaa.blend_ps) return pc_smaa_fail();
        b.size=16; b.usage=RHI_USAGE_DYNAMIC; b.bind=RHI_BIND_UNIFORM; b.cpu_access=RHI_CPU_WRITE;
        smaa.cb=rhi_buffer_create(&b,NULL);
        p.filter=RHI_FILTER_LINEAR;
        p.address_u=p.address_v=p.address_w=RHI_ADDRESS_CLAMP;
        p.compare=RHI_CMP_NEVER; p.max_lod=3.402823466e+38f;
        smaa.linear=rhi_sampler_create(&p); p.filter=RHI_FILTER_POINT;
        smaa.point=rhi_sampler_create(&p);
        blend.write_mask=RHI_WRITE_ALL; smaa.blend=rhi_blend_state_create(&blend);
        smaa.depth=rhi_depth_state_create(&depth);
        raster.fill=RHI_FILL_SOLID; raster.cull=RHI_CULL_NONE; raster.depth_clip=1;
        smaa.raster=rhi_raster_state_create(&raster);
        if(!smaa.cb || !smaa.linear || !smaa.point || !smaa.blend || !smaa.depth || !smaa.raster ||
           !pc_smaa_target(&smaa.area,AREATEX_WIDTH,AREATEX_HEIGHT,RHI_FORMAT_R8G8_UNORM,areaTexBytes,AREATEX_PITCH) ||
           !pc_smaa_target(&smaa.search,SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,RHI_FORMAT_R8_UNORM,searchTexBytes,SEARCHTEX_PITCH))
            return pc_smaa_fail();
        fprintf(stderr,"[SMAA] enabled: 1x High (%s)\n",rhi_backend_name());
    }
    if(smaa.width!=w || smaa.height!=h) {
        pc_smaa_target_release(&smaa.color); pc_smaa_target_release(&smaa.edges);
        pc_smaa_target_release(&smaa.weights);
        if(!pc_smaa_target(&smaa.color,w,h,RHI_FORMAT_R8G8B8A8_UNORM,NULL,0) ||
           !pc_smaa_target(&smaa.edges,w,h,RHI_FORMAT_R8G8_UNORM,NULL,0) ||
           !pc_smaa_target(&smaa.weights,w,h,RHI_FORMAT_R8G8B8A8_UNORM,NULL,0)) return pc_smaa_fail();
        smaa.width=w; smaa.height=h;
    }
    void *mapped=rhi_buffer_map(smaa.cb,RHI_MAP_WRITE_DISCARD);
    if(!mapped) return pc_smaa_fail();
    float metrics[4]={1.f/w,1.f/h,(float)w,(float)h};
    memcpy(mapped,metrics,sizeof metrics); rhi_buffer_unmap(smaa.cb);
    return 1;
}
static void pc_smaa_pass(RhiShader *ps, RhiView *target, RhiView *a, RhiView *b,
                          RhiView *c, RhiViewport vp)
{
    RhiView *textures[3]={a,b,c}, *none[3]={NULL,NULL,NULL};
    RhiSampler *samplers[2]={smaa.linear,smaa.point};
    float zero[4]={0,0,0,0}, factor[4]={1,1,1,1};
    if(ps==smaa.blend_ps) zero[3]=1; /* Opaque black letterbox bars. */
    rhi_set_render_target(target,NULL); rhi_clear_color(target,zero);
    rhi_set_viewports(1,&vp); rhi_set_vertex_layout(NULL);
    rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,smaa.vs); rhi_set_shader(RHI_STAGE_PIXEL,ps);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&smaa.cb);
    rhi_set_textures(0,3,textures); rhi_set_samplers(0,2,samplers);
    rhi_set_blend_state(smaa.blend,factor,0xFFFFFFFF);
    rhi_set_depth_state(smaa.depth,0); rhi_set_raster_state(smaa.raster);
    rhi_draw(3,0); rhi_set_textures(0,3,none);
}
static void pc_smaa_apply(RhiView *out, D3D8DisplayFit fit)
{
    RhiOutputState saved;
    RhiViewport vp={0,0,(float)smaa.width,(float)smaa.height,0,1};
    rhi_output_save(&saved);
    pc_smaa_pass(smaa.edge_ps,smaa.edges.rtv,smaa.color.srv,NULL,NULL,vp);
    pc_smaa_pass(smaa.weight_ps,smaa.weights.rtv,smaa.edges.srv,smaa.area.srv,smaa.search.srv,vp);
    vp.x=(float)fit.x; vp.y=(float)fit.y;
    pc_smaa_pass(smaa.blend_ps,out,smaa.color.srv,smaa.weights.srv,NULL,vp);
    rhi_output_restore(&saved);
}
