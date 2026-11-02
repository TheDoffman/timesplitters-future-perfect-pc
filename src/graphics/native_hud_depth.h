#ifndef TSFP_NATIVE_HUD_DEPTH_H
#define TSFP_NATIVE_HUD_DEPTH_H
#include "rhi.h"
#include <stdio.h>
#include <string.h>

/* A HUD still uses the world's depth for projected markers. Copy the depth
 * at each output pixel's centre; never average depths across silhouettes.
 * MSAA uses the nearest covered sample, like the existing AO depth reader. */
typedef struct TsfpHudDepth {
    RhiShader *vs,*ps,*stencil;
    RhiBuffer *size;
    RhiDepthState *depth;
    RhiDepthState *stencil_bits[8];
    RhiRasterState *raster;
    unsigned samples;
} TsfpHudDepth;
static void tsfp_hud_depth_destroy(TsfpHudDepth *p)
{
    rhi_shader_destroy(p->vs);rhi_shader_destroy(p->ps);rhi_shader_destroy(p->stencil);
    for(unsigned i=0;i<8;i++)rhi_depth_state_destroy(p->stencil_bits[i]);
    rhi_buffer_destroy(p->size);rhi_depth_state_destroy(p->depth);
    rhi_raster_state_destroy(p->raster);memset(p,0,sizeof *p);
}
static int tsfp_hud_depth_copy(TsfpHudDepth *p,RhiView *source,RhiView *out,
                              unsigned width,unsigned height)
{
    static const char shader[]=
        "cbuffer Size:register(b7){float4 sizes;uint bit;uint3 padding;}\n"
        "float4 vs_main(uint id:SV_VertexID):SV_Position{"
        "float2 c=float2((id<<1)&2,id&2);return float4(c*float2(2,-2)+float2(-1,1),0,1);}\n"
        "#if DEPTH_SAMPLES>1\nTexture2DMS<float> world:register(t10);Texture2DMS<uint2> masks:register(t11);\n"
        "#else\nTexture2D<float> world:register(t10);Texture2D<uint2> masks:register(t11);\n#endif\n"
        "int2 coordinate(float2 pos){return min(int2(pos*sizes.xy/sizes.zw),int2(sizes.xy)-1);}\n"
        "uint nearest_sample(int2 xy){\n"
        "#if DEPTH_SAMPLES>1\n"
        "float z=2;uint pick=0;[unroll]for(uint s=0;s<DEPTH_SAMPLES;s++){float v=world.Load(xy,s);if(v<z){z=v;pick=s;}}return pick;\n"
        "#else\nreturn 0;\n#endif\n}\n"
        "float ps_main(float4 pos:SV_Position):SV_Depth{int2 xy=coordinate(pos.xy);\n"
        "#if DEPTH_SAMPLES>1\nreturn world.Load(xy,nearest_sample(xy));\n"
        "#else\nreturn world.Load(int3(xy,0));\n#endif\n}\n"
        "void stencil_main(float4 pos:SV_Position){int2 xy=coordinate(pos.xy);uint2 v;\n"
        "#if DEPTH_SAMPLES>1\nv=masks.Load(xy,nearest_sample(xy));\n"
        "#else\nv=masks.Load(int3(xy,0));\n#endif\n"
        /* D3D11's stencil view is G; Vulkan's stencil-only view is R.
         * The absent channel is zero, so this normalizes both adapters. */
        "clip(((v.x|v.y)&bit)!=0?1.0:-1.0);}\n";
    RhiImageDesc image;RhiOutputState saved;RhiViewport vp={0};
    if(!source || !out || !width || !height)return 0;
    rhi_image_get_desc(rhi_view_image(source),&image);
    if(!p->ps || p->samples!=image.samples) {
        char count[16],error[2048]={0};
        RhiMacro macros[]={{"DEPTH_SAMPLES",count},{NULL,NULL}};
        RhiShaderSource code={shader,sizeof shader-1,"native_hud_depth",macros,"vs_main","vs_5_0",1};
        TsfpHudDepth next={0};RhiBufferDesc cb={0};RhiDepthDesc depth={0};RhiRasterDesc raster={0};
        snprintf(count,sizeof count,"%u",image.samples);
        next.vs=rhi_shader_create(RHI_STAGE_VERTEX,&code,error,sizeof error);
        code.entry="ps_main";code.target="ps_5_0";
        next.ps=rhi_shader_create(RHI_STAGE_PIXEL,&code,error,sizeof error);
        code.entry="stencil_main";next.stencil=rhi_shader_create(RHI_STAGE_PIXEL,&code,error,sizeof error);
        cb.size=32;cb.usage=RHI_USAGE_DYNAMIC;cb.bind=RHI_BIND_UNIFORM;cb.cpu_access=RHI_CPU_WRITE;
        next.size=rhi_buffer_create(&cb,NULL);
        depth.depth_enable=depth.depth_write=1;depth.depth_func=RHI_CMP_ALWAYS;
        next.depth=rhi_depth_state_create(&depth);
        memset(&depth,0,sizeof depth);depth.stencil_enable=1;depth.stencil_read_mask=255;
        depth.front.func=depth.back.func=RHI_CMP_ALWAYS;
        depth.front.fail=depth.back.fail=RHI_STENCIL_KEEP;
        depth.front.depth_fail=depth.back.depth_fail=RHI_STENCIL_KEEP;
        depth.front.pass=depth.back.pass=RHI_STENCIL_REPLACE;
        int stencil_ok=1;
        for(unsigned i=0;i<8;i++) {
            depth.stencil_write_mask=(uint8_t)(1u<<i);
            next.stencil_bits[i]=rhi_depth_state_create(&depth);
            if(!next.stencil_bits[i])stencil_ok=0;
        }
        raster.fill=RHI_FILL_SOLID;raster.cull=RHI_CULL_NONE;raster.depth_clip=1;
        next.raster=rhi_raster_state_create(&raster);next.samples=image.samples;
        if(!next.vs || !next.ps || !next.stencil || !stencil_ok || !next.size || !next.depth || !next.raster) {
            fprintf(stderr,"[NATIVE-HUD] Depth preparation failed: %s\n",error);
            tsfp_hud_depth_destroy(&next);return 0;
        }
        tsfp_hud_depth_destroy(p);*p=next;
    }
    struct {float size[4];uint32_t bit,padding[3];} data={{(float)image.width,(float)image.height,(float)width,(float)height},0,{0}};
    RhiViewDesc stencil_desc={0};stencil_desc.format=RHI_FORMAT_X24_TYPELESS_G8_UINT;
    stencil_desc.dim=image.samples>1?RHI_VIEW_DIM_2D_MS:RHI_VIEW_DIM_2D;
    stencil_desc.mip_count=stencil_desc.layer_count=1;
    RhiView *stencil=rhi_view_create(rhi_view_image(source),RHI_VIEW_SAMPLED,&stencil_desc);
    if(!stencil)return 0;
    void *mapped=rhi_buffer_map(p->size,RHI_MAP_WRITE_DISCARD);
    if(!mapped){rhi_view_destroy(stencil);return 0;}
    memcpy(mapped,&data,sizeof data);rhi_buffer_unmap(p->size);
    rhi_output_save(&saved);rhi_set_render_target(NULL,out);
    vp.width=(float)width;vp.height=(float)height;vp.max_depth=1;
    rhi_set_viewports(1,&vp);rhi_set_vertex_layout(NULL);
    rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_shader(RHI_STAGE_VERTEX,p->vs);rhi_set_shader(RHI_STAGE_PIXEL,p->ps);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&p->size);
    rhi_set_textures(10,1,&source);rhi_set_depth_state(p->depth,0);
    rhi_set_raster_state(p->raster);rhi_draw(3,0);
    /* Portable stencil export: eight masked replace passes reconstruct all
     * bits without requiring optional shader-stencil-export extensions. */
    rhi_set_shader(RHI_STAGE_PIXEL,p->stencil);rhi_set_textures(11,1,&stencil);
    int ok=1;
    for(unsigned i=0;i<8;i++) {
        data.bit=1u<<i;mapped=rhi_buffer_map(p->size,RHI_MAP_WRITE_DISCARD);
        if(!mapped){ok=0;break;}
        memcpy(mapped,&data,sizeof data);rhi_buffer_unmap(p->size);
        rhi_set_uniform_buffers(RHI_STAGE_PIXEL,7,1,&p->size);
        rhi_set_depth_state(p->stencil_bits[i],255);rhi_draw(3,0);
    }
    RhiView *none[2]={NULL,NULL};rhi_set_textures(10,2,none);rhi_output_restore(&saved);
    rhi_view_destroy(stencil);return ok;
}
#endif
