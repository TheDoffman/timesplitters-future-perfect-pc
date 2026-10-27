/* Execute the real renderer adapter's failure path with allocation injection.
 * No shaders run: default Original60 reset and failed preparation must not draw. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_RHI_RENDERER 1
#include "surface_gpu_fake_rhi.h"
typedef struct D3D8Texture {RhiImage *image;RhiView *srv;} D3D8Texture;
typedef D3D8Texture IDirect3DBaseTexture8;
typedef struct D3D8TextureInfo {unsigned unused;} D3D8TextureInfo;
typedef struct texture_entry {D3D8Texture *host;uint32_t va,data,format,size;int rendered;} texture_entry;
static texture_entry g_textures[4];static int g_texture_count=4;
static unsigned texture_binds;
static void (*cleanup)(void);
static int d3d8_texture_info(IDirect3DBaseTexture8 *t,D3D8TextureInfo *i){(void)i;return t!=NULL;}
static void d3d8_surface_field(IDirect3DBaseTexture8 *t,RhiView *v){assert(!t && !v);}
static void d3d8_surface_cleanup(void (*f)(void)){cleanup=f;}
static int hle_d3d8_defer_recording(void){return 0;}
static void hle_d3d8_defer_op(void (*f)(const void*),const void *a,size_t n){(void)f;(void)a;(void)n;assert(0);}
RhiImage *rhi_view_image(const RhiView *v){(void)v;assert(0);return NULL;}
void rhi_buffer_update(RhiBuffer *b,const void *p){(void)b;(void)p;assert(0);}
void rhi_set_render_target(RhiView *a,RhiView *b){(void)a;(void)b;assert(0);}
void rhi_output_save(RhiOutputState *s){(void)s;assert(0);}
void rhi_output_restore(RhiOutputState *s){(void)s;assert(0);}
void rhi_set_viewports(uint32_t n,const RhiViewport *v){(void)n;(void)v;assert(0);}
void rhi_set_topology(uint32_t t){(void)t;assert(0);}
void rhi_set_vertex_layout(RhiVertexLayout *l){(void)l;assert(0);}
void rhi_set_shader(uint32_t n,RhiShader *s){(void)n;(void)s;assert(0);}
void rhi_set_uniform_buffers(uint32_t a,uint32_t b,uint32_t n,RhiBuffer *const *v){(void)a;(void)b;(void)n;(void)v;assert(0);}
void rhi_set_textures(uint32_t a,uint32_t n,RhiView *const *v){(void)a;(void)n;(void)v;texture_binds++;}
void rhi_set_blend_state(RhiBlendState *s,const float f[4],uint32_t mask){(void)s;(void)f;(void)mask;assert(0);}
void rhi_set_depth_state(RhiDepthState *s,uint32_t r){(void)s;(void)r;assert(0);}
void rhi_set_raster_state(RhiRasterState *s){(void)s;assert(0);}
void rhi_draw(uint32_t n,uint32_t first){(void)n;(void)first;assert(0);}
#include "pc_surface_native_hle.inc"
int main(void) {
    hle_d3d8_surface_native_reset();assert(!texture_binds && !attempt);
    D3D8Texture textures[4];PcSurfaceNativeSample sample={0};sample.feedback.resource=100;
    sample.feedback.whole=1;
    float coefficients[4]={.17f,.105f,.65f,.98f};memcpy(sample.feedback.coefficient_bits,coefficients,16);
    RhiImageDesc d={0};d.width=d.height=128;d.depth=d.mip_levels=d.samples=1;
    d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
    for(unsigned i=0;i<4;i++) {
        textures[i].image=rhi_image_create(&d,NULL);
        textures[i].srv=rhi_view_create(textures[i].image,RHI_VIEW_SAMPLED,NULL);
        g_textures[i]=(texture_entry){&textures[i],1000+i,2000+i,3000+i,4000+i,1};
        uint32_t *id=i<3?sample.feedback.textures[i]:sample.final_texture;
        memcpy(id,&g_textures[i].va,16);
    }
    unsigned original_live=live;fail_at=attempt+1;
    hle_d3d8_surface_native_prepare(&sample);
    assert(pc_surface_native_failed && cleanup && live==original_live && !texture_binds);
    unsigned attempts=attempt;fail_at=0;
    for(unsigned frame=0;frame<144;frame++)hle_d3d8_surface_native_prepare(&sample);
    assert(attempt==attempts && live==original_live && !texture_binds);
    /* Rate changes don't repeat a failed shader creation. Scene/device reset
     * permits a fresh attempt, and an empty reset leaves renderer state alone. */
    hle_d3d8_surface_native_suspend();assert(pc_surface_native_failed);
    cleanup();assert(!pc_surface_native_failed && attempt==attempts);
    unsigned bindings=texture_binds;fail_at=attempt+1;
    hle_d3d8_surface_native_prepare(&sample);
    assert(pc_surface_native_failed && attempt==attempts+1 && texture_binds==bindings);
    cleanup();
    for(unsigned i=0;i<4;i++){rhi_view_destroy(textures[i].srv);rhi_image_destroy(textures[i].image);}
    assert(!live);
    puts("PASS: original reset preserves bindings; renderer failure retries only after scene/device reset");
    return 0;
}
