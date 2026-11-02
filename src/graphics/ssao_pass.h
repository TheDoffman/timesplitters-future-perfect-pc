#ifndef TSFP_SSAO_PASS_H
#define TSFP_SSAO_PASS_H
#include "rhi.h"
#include "ssao_shader.h"
#include <string.h>
#include <stdio.h>
typedef struct TsfpSsaoConstants {
    float inverse_projection[16];
    float size[4];
    float origin[4];
    float settings[4];
} TsfpSsaoConstants;
typedef struct TsfpSsaoPass {
    RhiShader *vs,*ps,*composite,*multiply;
    RhiBuffer *constants;
    RhiRasterState *raster;
    RhiDepthState *depth;
    RhiBlendState *blend,*multiply_blend;
    RhiImage *image;
    RhiView *rtv,*srv;
    unsigned width,height,depth_samples,origin_x,origin_y;
} TsfpSsaoPass;
static void tsfp_ssao_destroy(TsfpSsaoPass *p)
{
    rhi_view_destroy(p->srv);rhi_view_destroy(p->rtv);rhi_image_destroy(p->image);
    rhi_shader_destroy(p->multiply);rhi_blend_state_destroy(p->multiply_blend);
    rhi_shader_destroy(p->composite);rhi_shader_destroy(p->vs);rhi_shader_destroy(p->ps);rhi_buffer_destroy(p->constants);
    rhi_raster_state_destroy(p->raster);rhi_depth_state_destroy(p->depth);rhi_blend_state_destroy(p->blend);
    memset(p,0,sizeof *p);
}
/* Output bindings are restored; the caller must rebind its draw pipeline, as
 * with the display resolve. The returned RG image is visibility + view depth. */
static RhiView *tsfp_ssao_render_region(TsfpSsaoPass *p,RhiView *depth,unsigned x,unsigned y,unsigned width,unsigned height,
                                const float inverse_projection[16],float projection_y,
                                float radius,float bias,float strength)
{
    if(!depth||!width||!height||!inverse_projection||!(radius>0)||!(strength>=0))return NULL;
    RhiImageDesc depth_desc;
    rhi_image_get_desc(rhi_view_image(depth),&depth_desc);
    if(x>=depth_desc.width||y>=depth_desc.height||width>depth_desc.width-x||height>depth_desc.height-y)return NULL;
    unsigned samples=depth_desc.samples?depth_desc.samples:1;
    if(samples!=1&&samples!=2&&samples!=4&&samples!=8)return NULL;
    if(p->vs&&p->depth_samples!=samples)tsfp_ssao_destroy(p);
    if(!p->vs) {
        char sample_text[12];snprintf(sample_text,sizeof sample_text,"%u",samples);
        RhiMacro macros[2]={{"DEPTH_SAMPLES",sample_text},{NULL,NULL}};
        p->depth_samples=samples;
        char errors[2048]={0};
        RhiShaderSource code={tsfp_ssao_shader,sizeof(tsfp_ssao_shader)-1,"tsfp-ssao",macros,"vs_main","vs_5_0",1};
        p->vs=rhi_shader_create(RHI_STAGE_VERTEX,&code,errors,sizeof errors);
        code.entry="ps_main";code.target="ps_5_0";
        p->ps=rhi_shader_create(RHI_STAGE_PIXEL,&code,errors,sizeof errors);
        code.entry="composite_main";
        p->composite=rhi_shader_create(RHI_STAGE_PIXEL,&code,errors,sizeof errors);
        code.entry="multiply_main";
        p->multiply=rhi_shader_create(RHI_STAGE_PIXEL,&code,errors,sizeof errors);
        RhiBufferDesc bd={0};bd.size=sizeof(TsfpSsaoConstants);bd.usage=RHI_USAGE_DYNAMIC;
        bd.bind=RHI_BIND_UNIFORM;bd.cpu_access=RHI_CPU_WRITE;
        p->constants=rhi_buffer_create(&bd,NULL);
        RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.depth_clip=1;
        p->raster=rhi_raster_state_create(&rd);
        RhiDepthDesc dd={0};p->depth=rhi_depth_state_create(&dd);
        RhiBlendDesc bl={0};bl.write_mask=RHI_WRITE_ALL;p->blend=rhi_blend_state_create(&bl);
        bl.enable=1;bl.src=RHI_BLEND_ZERO;bl.dst=RHI_BLEND_SRC_COLOR;bl.op=RHI_BLEND_OP_ADD;
        bl.src_alpha=RHI_BLEND_ZERO;bl.dst_alpha=RHI_BLEND_ONE;bl.op_alpha=RHI_BLEND_OP_ADD;
        bl.write_mask=RHI_WRITE_R|RHI_WRITE_G|RHI_WRITE_B;
        p->multiply_blend=rhi_blend_state_create(&bl);
        if(!p->multiply||!p->multiply_blend||!p->vs||!p->ps||!p->composite||!p->constants||!p->raster||!p->depth||!p->blend) {
            fprintf(stderr,"SSAO setup failed: %s\n",errors);tsfp_ssao_destroy(p);return NULL;
        }
    }
    unsigned w=(width+1)/2,h=(height+1)/2;
    if(p->width!=w||p->height!=h) {
        RhiImageDesc desc={0};desc.type=RHI_IMAGE_2D;desc.width=w;desc.height=h;
        desc.depth=desc.mip_levels=desc.samples=1;desc.format=RHI_FORMAT_R32G32_FLOAT;
        desc.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
        RhiImage *image=rhi_image_create(&desc,NULL);
        RhiView *rtv=image?rhi_view_create(image,RHI_VIEW_RENDER_TARGET,NULL):NULL;
        RhiView *srv=rtv?rhi_view_create(image,RHI_VIEW_SAMPLED,NULL):NULL;
        if(!srv){rhi_view_destroy(rtv);rhi_image_destroy(image);return NULL;}
        rhi_view_destroy(p->srv);rhi_view_destroy(p->rtv);rhi_image_destroy(p->image);
        p->image=image;p->rtv=rtv;p->srv=srv;p->width=w;p->height=h;
    }
    TsfpSsaoConstants data;
    memcpy(data.inverse_projection,inverse_projection,sizeof data.inverse_projection);
    data.size[0]=(float)width;data.size[1]=(float)height;data.size[2]=(float)w;data.size[3]=(float)h;
    data.origin[0]=(float)x;data.origin[1]=(float)y;data.origin[2]=data.origin[3]=0;
    p->origin_x=x;p->origin_y=y;
    data.settings[0]=radius;data.settings[1]=bias;data.settings[2]=strength;data.settings[3]=projection_y;
    void *mapped=rhi_buffer_map(p->constants,RHI_MAP_WRITE_DISCARD);if(!mapped)return NULL;
    memcpy(mapped,&data,sizeof data);rhi_buffer_unmap(p->constants);
    RhiOutputState saved;rhi_output_save(&saved);
    RhiViewport vp={0,0,(float)w,(float)h,0,1};
    rhi_set_render_target(p->rtv,NULL);rhi_set_viewports(1,&vp);
    rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,p->vs);rhi_set_shader(RHI_STAGE_PIXEL,p->ps);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&p->constants);rhi_set_textures(9,1,&depth);
    rhi_set_raster_state(p->raster);rhi_set_depth_state(p->depth,0);rhi_set_blend_state(p->blend,NULL,~0u);
    rhi_draw(3,0);
    RhiView *empty=NULL;rhi_set_textures(9,1,&empty);rhi_output_restore(&saved);
    return p->srv;
}
static RhiView *tsfp_ssao_render(TsfpSsaoPass *p,RhiView *depth,unsigned width,unsigned height,
                                const float inverse[16],float projection_y,float radius,float bias,float strength)
{
    return tsfp_ssao_render_region(p,depth,0,0,width,height,inverse,projection_y,radius,bias,strength);
}
/* Bilateral upsample prevents contact shadows leaking across depth edges.
 * Source and destination must be distinct, full-resolution images. */
static int tsfp_ssao_composite(TsfpSsaoPass *p,RhiView *scene,RhiView *depth,RhiView *out,
                              unsigned width,unsigned height)
{
    if(!p->srv||!p->composite||!scene||!depth||!out||!width||!height ||
       p->width!=(width+1)/2||p->height!=(height+1)/2)return 0;
    RhiOutputState saved;rhi_output_save(&saved);
    RhiViewport vp={(float)p->origin_x,(float)p->origin_y,(float)width,(float)height,0,1};
    RhiView *inputs[3]={depth,p->srv,scene},*empty[3]={0};
    rhi_set_render_target(out,NULL);rhi_set_viewports(1,&vp);
    rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,p->vs);rhi_set_shader(RHI_STAGE_PIXEL,p->composite);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&p->constants);rhi_set_textures(9,3,inputs);
    rhi_set_raster_state(p->raster);rhi_set_depth_state(p->depth,0);rhi_set_blend_state(p->blend,NULL,~0u);
    rhi_draw(3,0);rhi_set_textures(9,3,empty);rhi_output_restore(&saved);
    return 1;
}
/* In-place world-color composition: no scene feedback or full-color copy. */
static int tsfp_ssao_apply(TsfpSsaoPass *p,RhiView *depth,RhiView *out,unsigned width,unsigned height)
{
    if(!p->srv||!depth||!out||!width||!height || p->width!=(width+1)/2||p->height!=(height+1)/2)return 0;
    RhiOutputState saved;rhi_output_save(&saved);
    RhiViewport vp={(float)p->origin_x,(float)p->origin_y,(float)width,(float)height,0,1};
    RhiView *inputs[2]={depth,p->srv},*empty[2]={0};
    rhi_set_render_target(out,NULL);rhi_set_viewports(1,&vp);
    rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,p->vs);rhi_set_shader(RHI_STAGE_PIXEL,p->multiply);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&p->constants);rhi_set_textures(9,2,inputs);
    rhi_set_raster_state(p->raster);rhi_set_depth_state(p->depth,0);
    rhi_set_blend_state(p->multiply_blend,NULL,~0u);
    rhi_draw(3,0);rhi_set_textures(9,2,empty);rhi_output_restore(&saved);return 1;
}
#endif
