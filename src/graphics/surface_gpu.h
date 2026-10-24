#ifndef TSFP_SURFACE_GPU_H
#define TSFP_SURFACE_GPU_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rhi.h"
#include "graphics/surface_basis.h"
#include "graphics/surface_mode.h"

/* Render-thread owner of the coupled field's GPU resources. Original event,
 * smoothing and boundary passes remain the caller's responsibility. Inputs are
 * full, single-mip sampled float4 views: state.xy=(green-.5,height), residual.x
 * is held forcing. Only world may be borrowed from this owner's internals. Destroy
 * before the RHI device; reset on resource/scene identity changes. No global
 * cache or guest pointer is retained. This does not install a game hook. */
typedef struct PcSurfaceGpuImage { RhiImage *image;RhiView *read,*write; } PcSurfaceGpuImage;
typedef struct PcSurfaceGpuConfig { unsigned side;double a,b,d,k,center; } PcSurfaceGpuConfig;
typedef struct PcSurfaceGpu {
    PcSurfaceGpuConfig config;
    PcSurfaceGpuImage mode[2],basis,state[2],forcing,world,scratch,input_residual;
    RhiShader *vertex,*copy,*evolve,*unpack,*residual,*transform[2][2];
    RhiBuffer *step;
    RhiRasterState *raster;
    RhiDepthState *depth;
    RhiBlendState *blend;
    unsigned current,ready;
} PcSurfaceGpu;

static inline void pc_surface_gpu_image_destroy(PcSurfaceGpuImage *image) {
    if(image->read)rhi_view_destroy(image->read);
    if(image->write)rhi_view_destroy(image->write);
    if(image->image)rhi_image_destroy(image->image);
    memset(image,0,sizeof *image);
}
static inline void pc_surface_gpu_destroy(PcSurfaceGpu *s) {
    if(!s)return;
    for(unsigned i=0;i<2;i++) {
        pc_surface_gpu_image_destroy(&s->mode[i]);pc_surface_gpu_image_destroy(&s->state[i]);
        for(unsigned j=0;j<2;j++)if(s->transform[i][j])rhi_shader_destroy(s->transform[i][j]);
    }
    pc_surface_gpu_image_destroy(&s->basis);pc_surface_gpu_image_destroy(&s->forcing);
    pc_surface_gpu_image_destroy(&s->world);pc_surface_gpu_image_destroy(&s->scratch);
    pc_surface_gpu_image_destroy(&s->input_residual);
    if(s->vertex)rhi_shader_destroy(s->vertex);
    if(s->copy)rhi_shader_destroy(s->copy);
    if(s->evolve)rhi_shader_destroy(s->evolve);
    if(s->unpack)rhi_shader_destroy(s->unpack);
    if(s->residual)rhi_shader_destroy(s->residual);
    if(s->step)rhi_buffer_destroy(s->step);
    if(s->raster)rhi_raster_state_destroy(s->raster);
    if(s->depth)rhi_depth_state_destroy(s->depth);
    if(s->blend)rhi_blend_state_destroy(s->blend);
    free(s);
}
static inline int pc_surface_gpu_image_create(PcSurfaceGpuImage *image,unsigned side,int writable,const float *pixels) {
    RhiImageDesc desc={0};desc.width=desc.height=side;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R32G32B32A32_FLOAT;
    desc.bind=RHI_BIND_SAMPLED|(writable?RHI_BIND_RENDER_TARGET:0);
    RhiSubresourceData data={pixels,side*16,side*side*16};
    image->image=rhi_image_create(&desc,pixels?&data:NULL);
    if(!image->image)return 0;
    image->read=rhi_view_create(image->image,RHI_VIEW_SAMPLED,NULL);
    if(writable)image->write=rhi_view_create(image->image,RHI_VIEW_RENDER_TARGET,NULL);
    return image->read && (!writable || image->write);
}
static inline RhiShader *pc_surface_gpu_shader(unsigned stage,const char *text,char *errors,size_t capacity) {
    RhiShaderSource source={text,strlen(text),"native-water",NULL,"main",stage==RHI_STAGE_VERTEX?"vs_5_0":"ps_5_0",1};
    return rhi_shader_create(stage,&source,errors,capacity);
}
#define TSFP_SURFACE_VEC2 "float2"
#define TSFP_SURFACE_VEC4 "float4"
#define TSFP_SURFACE_VEC3 "float3"
#define TSFP_SURFACE_IVEC2 "int2"
#include "graphics/surface_evolve_shader.h"
#include "graphics/surface_transform_shader.h"
#include "graphics/surface_gradient_shader.h"
static inline PcSurfaceGpu *pc_surface_gpu_create(const PcSurfaceGpuConfig *config,char *errors,size_t capacity) {
    PcSurfaceMode check;
    if(!config || !config->side || config->side>PC_SURFACE_MAX_EXTENT || !isfinite(config->center) ||
        !pc_surface_mode(config->a,config->b,config->d,config->k,0,0,&check))return NULL;
    PcSurfaceGpu *s=calloc(1,sizeof *s);if(!s)return NULL;s->config=*config;
    float *pixels=malloc(config->side*config->side*16);if(!pixels){pc_surface_gpu_destroy(s);return NULL;}
    unsigned side=config->side;
    for(unsigned row=0;row<2;row++) {
        for(unsigned y=0;y<side;y++)for(unsigned x=0;x<side;x++) {
            PcSurfaceMode mode;double dc=(!x && !y)?side:0;
            if(!pc_surface_mode(config->a,config->b,config->d,
                config->k+pc_surface_stencil_shift(side,side,x,y,PC_SURFACE_WRAP),
                dc*(config->center-.5+config->k*config->center),dc*(config->center-.5),&mode))goto fail;
            memcpy(pixels+(y*side+x)*4,row?mode.fixed:mode.eigen,16);
        }
        if(!pc_surface_gpu_image_create(&s->mode[row],side,0,pixels))goto fail;
    }
    for(unsigned y=0;y<side;y++)for(unsigned x=0;x<side;x++) {
        unsigned at=(y*side+x)*4;pixels[at]=(float)pc_surface_basis_weight(side,y,x,PC_SURFACE_WRAP);
        pixels[at+1]=pixels[at+2]=pixels[at+3]=0;
    }
    if(!pc_surface_gpu_image_create(&s->basis,side,0,pixels))goto fail;
    PcSurfaceGpuImage *fields[]={&s->state[0],&s->state[1],&s->forcing,&s->world,&s->scratch,&s->input_residual};
    for(unsigned i=0;i<6;i++)if(!pc_surface_gpu_image_create(fields[i],side,1,NULL))goto fail;
    s->vertex=pc_surface_gpu_shader(RHI_STAGE_VERTEX,
        "float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id==2?3.0:-1.0,id==1?3.0:-1.0,0,1);}",errors,capacity);
    s->copy=pc_surface_gpu_shader(RHI_STAGE_PIXEL,
        "Texture2D source:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return source.Load(int3(p.xy,0));}",errors,capacity);
    s->evolve=pc_surface_gpu_shader(RHI_STAGE_PIXEL,
        "Texture2D state:register(t0);Texture2D eigen:register(t1);Texture2D fixed:register(t2);Texture2D residual:register(t3);"
        "cbuffer SurfaceStep:register(b1){float4 surface_step;}"
        TSFP_SURFACE_EVOLVE_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {int3 at=int3(p.xy,0);return float4(surface_evolve(state.Load(at).xy,eigen.Load(at),fixed.Load(at),residual.Load(at).x,surface_step.x,surface_step.y),0,1);}",errors,capacity);
    if(!s->vertex || !s->copy || !s->evolve)goto fail;
    const char prefix[]="Texture2D source:register(t0);Texture2D basis:register(t1);"
        "float2 surface_load(int2 at){return source.Load(int3(at,0)).xy;}"
        "float surface_basis(int frequency,int position){return basis.Load(int3(position,frequency,0)).x;}"
        TSFP_SURFACE_TRANSFORM_SHADER;
    char program[4096];
    s->unpack=pc_surface_gpu_shader(RHI_STAGE_PIXEL,
        "Texture2D source:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET {"
        "float4 s=source.Load(int3(p.xy,0));return float4(s.g-.5,s.b,0,1);}",errors,capacity);
    const char residual_prefix[]="Texture2D rounded:register(t0);Texture2D source:register(t1);"
        TSFP_SURFACE_GRADIENT_SHADER;
    int length=snprintf(program,sizeof program,
        "%sfloat4 main(float4 p:SV_POSITION):SV_TARGET {int2 size=int2(%u,%u),at=int2(p.xy);"
        "float h=source.Load(int3(at,0)).b;float3 n=float3("
        "source.Load(int3((at+size+int2(-1,-1))%%size,0)).b,"
        "source.Load(int3((at+size+int2(1,-1))%%size,0)).b,"
        "source.Load(int3((at+size+int2(1,1))%%size,0)).b);"
        "float diagonal=source.Load(int3((at+size+int2(-1,1))%%size,0)).b;"
        "return float4(surface_feedback_residual(h,n,diagonal,rounded.Load(int3(at,0)).r,%.17g),0,0,1);}",
        residual_prefix,side,side,config->k);
    if(length<0 || (size_t)length>=sizeof program)goto fail;
    s->residual=pc_surface_gpu_shader(RHI_STAGE_PIXEL,program,errors,capacity);
    if(!s->unpack || !s->residual)goto fail;
    for(unsigned inverse=0;inverse<2;inverse++)for(unsigned axis=0;axis<2;axis++) {
        int size=snprintf(program,sizeof program,"%sfloat4 main(float4 p:SV_POSITION):SV_TARGET{return float4(surface_transform(int2(p.xy),%u,%u,%u),0,1);}",prefix,axis,inverse,side);
        if(size<0 || (size_t)size>=sizeof program)goto fail;
        s->transform[inverse][axis]=pc_surface_gpu_shader(RHI_STAGE_PIXEL,program,errors,capacity);
        if(!s->transform[inverse][axis])goto fail;
    }
    RhiBufferDesc buffer={16,RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};s->step=rhi_buffer_create(&buffer,NULL);
    RhiRasterDesc raster={0};raster.fill=RHI_FILL_SOLID;raster.cull=RHI_CULL_NONE;raster.depth_clip=1;
    s->raster=rhi_raster_state_create(&raster);
    RhiDepthDesc depth={0};depth.depth_func=RHI_CMP_ALWAYS;
    depth.front.fail=depth.front.depth_fail=depth.front.pass=RHI_STENCIL_KEEP;depth.front.func=RHI_CMP_ALWAYS;depth.back=depth.front;
    s->depth=rhi_depth_state_create(&depth);
    RhiBlendDesc blend={0};blend.write_mask=RHI_WRITE_ALL;blend.src=blend.src_alpha=RHI_BLEND_ONE;
    blend.dst=blend.dst_alpha=RHI_BLEND_ZERO;blend.op=blend.op_alpha=RHI_BLEND_OP_ADD;
    s->blend=rhi_blend_state_create(&blend);
    if(!s->step || !s->raster || !s->depth || !s->blend)goto fail;
    free(pixels);return s;
fail:
    free(pixels);pc_surface_gpu_destroy(s);return NULL;
}
#undef TSFP_SURFACE_VEC2
#undef TSFP_SURFACE_VEC3
#undef TSFP_SURFACE_VEC4
#undef TSFP_SURFACE_IVEC2

static inline void pc_surface_gpu_scope_begin(PcSurfaceGpu *s,RhiOutputState *output) {
    rhi_output_save(output);
    RhiViewport viewport={0,0,(float)s->config.side,(float)s->config.side,0,1};
    rhi_set_viewports(1,&viewport);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    rhi_set_vertex_layout(NULL);rhi_set_shader(RHI_STAGE_VERTEX,s->vertex);
    rhi_set_raster_state(s->raster);rhi_set_depth_state(s->depth,0);rhi_set_blend_state(s->blend,NULL,~0u);
}
static inline void pc_surface_gpu_scope_end(RhiOutputState *output) {
    RhiView *none[4]={0};rhi_set_textures(0,4,none);
    RhiBuffer *empty=NULL;rhi_set_uniform_buffers(RHI_STAGE_PIXEL,1,1,&empty);
    rhi_output_restore(output);
    /* The current RHI restore adapter skips an empty viewport list. */
    if(!output->viewport_count)rhi_set_viewports(0,NULL);
}
static inline void pc_surface_gpu_draw(RhiShader *shader,RhiView **inputs,unsigned count,RhiView *output) {
    RhiView *none[4]={0};rhi_set_textures(0,4,none);rhi_set_render_target(output,NULL);
    rhi_set_textures(0,count,inputs);rhi_set_shader(RHI_STAGE_PIXEL,shader);rhi_draw(3,0);
    rhi_set_textures(0,4,none);rhi_set_render_target(NULL,NULL);
}
static inline void pc_surface_gpu_transform(PcSurfaceGpu *s,RhiView *input,RhiView *output,unsigned inverse) {
    for(unsigned axis=0;axis<2;axis++) {
        RhiView *inputs[2]={axis?s->scratch.read:input,s->basis.read};
        pc_surface_gpu_draw(s->transform[inverse][axis],inputs,2,axis?output:s->scratch.write);
    }
}
static inline int pc_surface_gpu_input(PcSurfaceGpu *s,RhiView *view) {
    if(!s || !view)return 0;RhiImage *image=rhi_view_image(view);if(!image)return 0;
    RhiImageDesc desc;rhi_image_get_desc(image,&desc);
    return desc.type==RHI_IMAGE_2D && desc.width==s->config.side && desc.height==s->config.side &&
        desc.depth==1 && desc.mip_levels==1 && desc.format==RHI_FORMAT_R32G32B32A32_FLOAT && desc.samples==1;
}
static inline int pc_surface_gpu_internal(PcSurfaceGpu *s,RhiView *view) {
    RhiImage *image=rhi_view_image(view);
    return image==s->state[0].image || image==s->state[1].image || image==s->forcing.image ||
        image==s->scratch.image || image==s->input_residual.image || image==s->basis.image || image==s->mode[0].image || image==s->mode[1].image;
}
static inline void pc_surface_gpu_reset(PcSurfaceGpu *s) {if(s)s->ready=0;}
/* Borrowed only until reset/destruction; never expose stale output as ready. */
static inline RhiView *pc_surface_gpu_output(PcSurfaceGpu *s) {return s && s->ready?s->world.read:NULL;}
static inline int pc_surface_gpu_begin(PcSurfaceGpu *s,RhiView *state,RhiView *residual) {
    if(!pc_surface_gpu_input(s,state) || !pc_surface_gpu_input(s,residual))return 0;
    if(pc_surface_gpu_internal(s,state) || pc_surface_gpu_internal(s,residual) || rhi_view_image(residual)==s->world.image)return 0;
    RhiOutputState output;pc_surface_gpu_scope_begin(s,&output);
    if(rhi_view_image(state)!=s->world.image)pc_surface_gpu_draw(s->copy,&state,1,s->world.write);
    pc_surface_gpu_transform(s,s->world.read,s->state[0].write,0);
    pc_surface_gpu_transform(s,residual,s->forcing.write,0);
    s->current=0;s->ready=1;pc_surface_gpu_scope_end(&output);return 1;
}
static inline int pc_surface_gpu_advance(PcSurfaceGpu *s,double delta) {
    if(!s || !s->ready || !isfinite(delta) || delta<0 || delta>1)return 0;
    if(!delta)return 1;
    RhiOutputState output;pc_surface_gpu_scope_begin(s,&output);
    float step[4]={(float)delta,(float)pow(s->config.d,delta*.5),0,0};
    rhi_buffer_update(s->step,step);rhi_set_uniform_buffers(RHI_STAGE_PIXEL,1,1,&s->step);
    RhiView *inputs[4]={s->state[s->current].read,s->mode[0].read,s->mode[1].read,s->forcing.read};
    pc_surface_gpu_draw(s->evolve,inputs,4,s->state[s->current^1].write);s->current^=1;
    pc_surface_gpu_transform(s,s->state[s->current].read,s->world.write,1);
    pc_surface_gpu_scope_end(&output);return 1;
}
/* Completed original byte input/first pass are sampled, never modified. This
 * preserves clipping/rounding and the second pass's acceleration saturation. */
static inline int pc_surface_gpu_begin_bytes(PcSurfaceGpu *s,RhiView *source,RhiView *first) {
    if(!s || s->config.center!=128./255. || !source || !first ||
       rhi_view_image(source)==rhi_view_image(first))return 0;
    RhiView *views[2]={source,first};
    for(unsigned i=0;i<2;i++) {
        RhiImage *image=rhi_view_image(views[i]);if(!image)return 0;
        RhiImageDesc desc;rhi_image_get_desc(image,&desc);
        if(desc.type!=RHI_IMAGE_2D || desc.width!=s->config.side || desc.height!=s->config.side ||
           desc.depth!=1 || desc.mip_levels!=1 || desc.samples!=1 || !(desc.bind&RHI_BIND_SAMPLED) ||
           (desc.format!=RHI_FORMAT_R8G8B8A8_UNORM && desc.format!=RHI_FORMAT_B8G8R8A8_UNORM))return 0;
    }
    RhiOutputState output;pc_surface_gpu_scope_begin(s,&output);
    RhiView *inputs[2]={first,source};
    pc_surface_gpu_draw(s->residual,inputs,2,s->input_residual.write);
    pc_surface_gpu_draw(s->unpack,&source,1,s->world.write);
    pc_surface_gpu_transform(s,s->world.read,s->state[0].write,0);
    pc_surface_gpu_transform(s,s->input_residual.read,s->forcing.write,0);
    s->current=0;s->ready=1;pc_surface_gpu_scope_end(&output);return 1;
}
#endif
