#ifndef TSFP_MSAA_COLOR_SURFACE_H
#define TSFP_MSAA_COLOR_SURFACE_H
#include "rhi.h"
#include <string.h>

/* A guest texture remains single-sampled. Its renderable subresource has a
 * companion with coverage samples; resolve before guest texture reads, seed
 * after a CPU/copy write. Lifetime belongs to the guest resource, not a frame. */
typedef struct TsfpMsaaColorSurface {
    RhiImage *source,*image;
    RhiView *source_view,*rtv;
    RhiShader *vs,*ps;
    RhiRasterState *raster;
    RhiDepthState *depth;
    RhiBlendState *blend;
    unsigned subresource,width,height,samples;
} TsfpMsaaColorSurface;
static void tsfp_msaa_color_destroy(TsfpMsaaColorSurface *p)
{
    rhi_view_destroy(p->source_view);rhi_view_destroy(p->rtv);
    rhi_image_destroy(p->source);rhi_image_destroy(p->image);
    rhi_shader_destroy(p->vs);rhi_shader_destroy(p->ps);
    rhi_raster_state_destroy(p->raster);rhi_depth_state_destroy(p->depth);rhi_blend_state_destroy(p->blend);
    memset(p,0,sizeof *p);
}
/* Like the display-copy pass, restore output state; the caller rebinds the
 * guest draw pipeline. The source view exposes exactly one array layer/mip. */
static int tsfp_msaa_color_seed(TsfpMsaaColorSurface *p)
{
    RhiOutputState saved;RhiView *none=NULL;
    if(!p||!p->image||!p->source_view)return 0;
    RhiViewport vp={0,0,(float)p->width,(float)p->height,0,1};
    rhi_output_save(&saved);rhi_set_render_target(p->rtv,NULL);
    rhi_set_viewports(1,&vp);rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,p->vs);rhi_set_shader(RHI_STAGE_PIXEL,p->ps);
    rhi_set_raster_state(p->raster);rhi_set_depth_state(p->depth,0);rhi_set_blend_state(p->blend,NULL,~0u);
    rhi_set_textures(0,1,&p->source_view);rhi_draw(3,0);rhi_set_textures(0,1,&none);
    rhi_output_restore(&saved);return 1;
}
static int tsfp_msaa_color_resolve(TsfpMsaaColorSurface *p)
{
    return p&&p->image&&rhi_image_resolve_subresource(p->image,0,p->source,p->subresource)==0;
}
static int tsfp_msaa_color_create(TsfpMsaaColorSurface *p,RhiImage *source,unsigned sub,unsigned samples)
{
    static const char shader[]= "Texture2DArray<float4> original:register(t0);"
        "struct O{float4 p:SV_POSITION;};O vs(uint id:SV_VertexID){"
        "float2 q=float2((id<<1)&2,id&2);O o;o.p=float4(q*float2(2,-2)+float2(-1,1),0,1);return o;}"
        "float4 ps(O i):SV_TARGET{return original.Load(int4(int2(i.p.xy),0,0));}";
    TsfpMsaaColorSurface next={0};RhiImageDesc d;RhiViewDesc vd={0};char error[2048];
    RhiRasterDesc rd={0};RhiDepthDesc dd={0};RhiBlendDesc bd={0};
    RhiShaderSource code={shader,sizeof shader-1,"msaa-color-seed",NULL,"vs","vs_5_0",1};
    if(!p||!source||(samples!=2&&samples!=4&&samples!=8))return 0;
    rhi_image_get_desc(source,&d);
    if(d.type!=RHI_IMAGE_2D||d.samples>1||!d.mip_levels||sub/d.mip_levels>=d.depth||
       sub%d.mip_levels>=32||!(d.bind&RHI_BIND_SAMPLED)||(d.bind&RHI_BIND_DEPTH)||
       !rhi_sample_count_supported(d.format,samples))return 0;
    vd.dim=RHI_VIEW_DIM_2D_ARRAY;vd.base_mip=sub%d.mip_levels;vd.base_layer=sub/d.mip_levels;
    vd.mip_count=vd.layer_count=1;
    next.source_view=rhi_view_create(source,RHI_VIEW_SAMPLED,&vd);if(!next.source_view)goto fail;
    next.source=rhi_image_retain(source);next.subresource=sub;next.samples=samples;
    d.width>>=vd.base_mip;d.height>>=vd.base_mip;
    if(!d.width)d.width=1;if(!d.height)d.height=1;
    next.width=d.width;next.height=d.height;
    d.depth=d.mip_levels=1;d.samples=samples;d.sample_quality=0;d.cube=0;
    d.bind=RHI_BIND_RENDER_TARGET;d.usage=RHI_USAGE_DEFAULT;d.cpu_access=0;
    next.image=rhi_image_create(&d,NULL);if(!next.image)goto fail;
    next.rtv=rhi_view_create(next.image,RHI_VIEW_RENDER_TARGET,NULL);if(!next.rtv)goto fail;
    next.vs=rhi_shader_create(RHI_STAGE_VERTEX,&code,error,sizeof error);if(!next.vs)goto fail;
    code.entry="ps";code.target="ps_5_0";
    next.ps=rhi_shader_create(RHI_STAGE_PIXEL,&code,error,sizeof error);if(!next.ps)goto fail;
    rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.multisample=1;
    next.raster=rhi_raster_state_create(&rd);next.depth=rhi_depth_state_create(&dd);
    bd.write_mask=RHI_WRITE_ALL;next.blend=rhi_blend_state_create(&bd);
    if(!next.raster||!next.depth||!next.blend||!tsfp_msaa_color_seed(&next))goto fail;
    tsfp_msaa_color_destroy(p);*p=next;return 1;
fail:
    tsfp_msaa_color_destroy(&next);return 0;
}
#endif
