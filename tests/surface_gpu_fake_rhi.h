/* Asset-free handle/creation spy. No shader or renderer emulation. */
#include "rhi.h"
enum { IMAGE=1,VIEW,SHADER,BUFFER,RASTER,DEPTH,BLEND };
typedef struct FakeSurfaceHandle {void *pointer;unsigned kind,refs;RhiImageDesc image;} FakeSurfaceHandle;
static FakeSurfaceHandle handles[512];
static unsigned attempt,fail_at,live;
static void *allocate(unsigned kind) {
    if(++attempt==fail_at)return NULL;
    for(unsigned i=0;i<512;i++)if(!handles[i].pointer) {
        handles[i].pointer=malloc(1);assert(handles[i].pointer);handles[i].kind=kind;handles[i].refs=1;live++;
        return handles[i].pointer;
    }
    assert(0);return NULL;
}
static void release(void *pointer,unsigned kind) {
    assert(pointer);
    for(unsigned i=0;i<512;i++)if(handles[i].pointer==pointer) {
        assert(handles[i].kind==kind && handles[i].refs);
        if(--handles[i].refs)return;
        free(pointer);handles[i].pointer=NULL;live--;return;
    }
    assert(0);
}
static FakeSurfaceHandle *lookup(void *pointer,unsigned kind) {
    for(unsigned i=0;i<512;i++)if(handles[i].pointer==pointer) {
        assert(handles[i].kind==kind && handles[i].refs);return &handles[i];
    }
    assert(0);return NULL;
}
RhiImage *rhi_image_retain(RhiImage *p){lookup(p,IMAGE)->refs++;return p;}
void rhi_image_get_desc(const RhiImage *p,RhiImageDesc *desc){*desc=lookup((void*)p,IMAGE)->image;}
RhiImage *rhi_image_create(const RhiImageDesc *d,const RhiSubresourceData *data) {
    assert(d->width && d->height);(void)data;RhiImage *p=allocate(IMAGE);
    if(p)lookup(p,IMAGE)->image=*d;return p;
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
