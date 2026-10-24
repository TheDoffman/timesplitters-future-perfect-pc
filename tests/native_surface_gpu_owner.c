/* Asset-free construction failure injection; actual shader pixels/state are
 * covered separately through real RHI adapters. Every RHI allocation stage
 * must unwind all prior owned handles, without modifying renderer bindings. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "rhi.h"
enum { IMAGE=1,VIEW,SHADER,BUFFER,RASTER,DEPTH,BLEND };
static struct {void *pointer;unsigned kind;} handles[64];
static unsigned attempt,fail_at,live;
static void *allocate(unsigned kind) {
    if(++attempt==fail_at)return NULL;
    for(unsigned i=0;i<64;i++)if(!handles[i].pointer) {
        handles[i].pointer=malloc(1);assert(handles[i].pointer);handles[i].kind=kind;live++;
        return handles[i].pointer;
    }
    assert(0);return NULL;
}
static void release(void *pointer,unsigned kind) {
    assert(pointer);
    for(unsigned i=0;i<64;i++)if(handles[i].pointer==pointer) {
        assert(handles[i].kind==kind);free(pointer);handles[i].pointer=NULL;live--;return;
    }
    assert(0);
}
RhiImage *rhi_image_create(const RhiImageDesc *d,const RhiSubresourceData *data) {
    assert(d->width==128 && d->height==128);(void)data;return allocate(IMAGE);
}
void rhi_image_destroy(RhiImage *p){release(p,IMAGE);}
RhiView *rhi_view_create(RhiImage *image,uint32_t type,const RhiViewDesc *d) {
    assert(image && (type==RHI_VIEW_SAMPLED || type==RHI_VIEW_RENDER_TARGET));(void)d;return allocate(VIEW);
}
void rhi_view_destroy(RhiView *p){release(p,VIEW);}
RhiShader *rhi_shader_create(uint32_t stage,const RhiShaderSource *source,char *errors,size_t capacity) {
    assert(stage==RHI_STAGE_VERTEX || stage==RHI_STAGE_PIXEL);assert(source && source->hlsl);
    (void)errors;(void)capacity;return allocate(SHADER);
}
void rhi_shader_destroy(RhiShader *p){release(p,SHADER);}
RhiBuffer *rhi_buffer_create(const RhiBufferDesc *d,const void *data) {(void)d;(void)data;return allocate(BUFFER);}
void rhi_buffer_destroy(RhiBuffer *p){release(p,BUFFER);}
RhiRasterState *rhi_raster_state_create(const RhiRasterDesc *d){(void)d;return allocate(RASTER);}
void rhi_raster_state_destroy(RhiRasterState *p){release(p,RASTER);}
RhiDepthState *rhi_depth_state_create(const RhiDepthDesc *d){(void)d;return allocate(DEPTH);}
void rhi_depth_state_destroy(RhiDepthState *p){release(p,DEPTH);}
RhiBlendState *rhi_blend_state_create(const RhiBlendDesc *d){(void)d;return allocate(BLEND);}
void rhi_blend_state_destroy(RhiBlendState *p){release(p,BLEND);}
#include "graphics/surface_gpu.h"
int main(void) {
    PcSurfaceGpuConfig config={128,26./255,165./255,249./255,43./255,128./255};
    char errors[256];PcSurfaceGpu *owner=pc_surface_gpu_create(&config,errors,sizeof errors);
    assert(owner && !owner->ready);unsigned stages=attempt;assert(stages>20);
    pc_surface_gpu_destroy(owner);assert(!live);pc_surface_gpu_destroy(NULL);
    for(unsigned stage=1;stage<=stages;stage++) {
        attempt=0;fail_at=stage;owner=pc_surface_gpu_create(&config,errors,sizeof errors);
        assert(!owner && !live);
    }
    attempt=fail_at=0;owner=pc_surface_gpu_create(&config,errors,sizeof errors);
    assert(owner);pc_surface_gpu_destroy(owner);assert(!live);
    printf("PASS: surface GPU owner cleans all %u allocation-failure stages and recreates\n",stages);
    return 0;
}
