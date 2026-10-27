/* The complete HLE texture unit also defines water hooks. Link their graphics
 * dependencies explicitly instead of depending on compiler dead stripping.
 * Screen-cache sampling must never enter these paths; every stub fails loudly.
 * Water rendering itself is covered by the native-surface and GPU fixtures. */
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
#include "rhi.h"
RhiBuffer *rhi_buffer_create(const RhiBufferDesc *d,const void *p){assert(0);return NULL;}
void rhi_buffer_destroy(RhiBuffer *p){assert(0);}
void rhi_buffer_update(RhiBuffer *b,const void *p){assert(0);}
RhiImage *rhi_image_create(const RhiImageDesc *d,const RhiSubresourceData *p){assert(0);return NULL;}
RhiImage *rhi_image_retain(RhiImage *p){assert(0);return NULL;}
void rhi_image_destroy(RhiImage *p){assert(0);}
void rhi_image_get_desc(const RhiImage *p,RhiImageDesc *d){assert(0);}
int rhi_image_readback(RhiImage *p,uint32_t sub,void *out,uint32_t pitch){assert(0);return -1;}
RhiView *rhi_view_create(RhiImage *p,uint32_t kind,const RhiViewDesc *d){assert(0);return NULL;}
void rhi_view_destroy(RhiView *p){assert(0);}
RhiImage *rhi_view_image(const RhiView *p){assert(0);return NULL;}
RhiShader *rhi_shader_create(uint32_t stage,const RhiShaderSource *s,char *e,size_t n){assert(0);return NULL;}
void rhi_shader_destroy(RhiShader *p){assert(0);}
RhiBlendState *rhi_blend_state_create(const RhiBlendDesc *d){assert(0);return NULL;}
RhiDepthState *rhi_depth_state_create(const RhiDepthDesc *d){assert(0);return NULL;}
RhiRasterState *rhi_raster_state_create(const RhiRasterDesc *d){assert(0);return NULL;}
void rhi_blend_state_destroy(RhiBlendState *p){assert(0);}
void rhi_depth_state_destroy(RhiDepthState *p){assert(0);}
void rhi_raster_state_destroy(RhiRasterState *p){assert(0);}
void rhi_set_render_target(RhiView *a,RhiView *b){assert(0);}
void rhi_output_save(RhiOutputState *s){assert(0);}
void rhi_output_restore(RhiOutputState *s){assert(0);}
void rhi_set_viewports(uint32_t n,const RhiViewport *v){assert(0);}
void rhi_set_topology(uint32_t t){assert(0);}
void rhi_set_vertex_layout(RhiVertexLayout *p){assert(0);}
void rhi_set_shader(uint32_t stage,RhiShader *p){assert(0);}
void rhi_set_uniform_buffers(uint32_t stage,uint32_t slot,uint32_t n,RhiBuffer *const *p){assert(0);}
void rhi_set_textures(uint32_t slot,uint32_t n,RhiView *const *p){assert(0);}
void rhi_set_blend_state(RhiBlendState *p,const float f[4],uint32_t mask){assert(0);}
void rhi_set_depth_state(RhiDepthState *p,uint32_t stencil){assert(0);}
void rhi_set_raster_state(RhiRasterState *p){assert(0);}
void rhi_draw(uint32_t n,uint32_t first){assert(0);}
BOOL d3d8_texture_info(IDirect3DBaseTexture8 *p,D3D8TextureInfo *out){assert(0);return FALSE;}
void d3d8_surface_field(IDirect3DBaseTexture8 *p,RhiView *v){assert(0);}
void d3d8_surface_cleanup(void (*f)(void)){assert(0);}
#endif
