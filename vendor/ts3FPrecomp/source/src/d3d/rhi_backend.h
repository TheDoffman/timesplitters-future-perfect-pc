/**
 * rhi_backend.h -- what a backend supplies to rhi.c.
 *
 * One table per backend, chosen once at device creation. rhi.c forwards
 * every call in rhi.h through the chosen table, so a Windows build carries
 * Direct3D 11 and Vulkan side by side and RECOMP_D3D8_BACKEND picks one at
 * run time -- which is what lets one executable replay the same capture
 * through both and compare them (scripts/replay_ab.py).
 */
#ifndef XBOXRECOMP_RHI_BACKEND_H
#define XBOXRECOMP_RHI_BACKEND_H

#include "rhi.h"

typedef struct RhiBackend {
    const char *name;

    int      (*device_create)(const RhiDeviceDesc *);
    void     (*device_destroy)(void);
    int      (*device_ready)(void);
    RhiView *(*swapchain_view)(void);
    int      (*swapchain_resize)(uint32_t, uint32_t);
    int32_t  (*present)(uint32_t);
    int      (*swapchain_readback)(void *, uint32_t, uint32_t *, uint32_t *);
    void     (*swapchain_set_vrr)(int);

    RhiBuffer *(*buffer_create)(const RhiBufferDesc *, const void *);
    void  (*buffer_destroy)(RhiBuffer *);
    void *(*buffer_map)(RhiBuffer *, uint32_t);
    void  (*buffer_unmap)(RhiBuffer *);
    void  (*buffer_update)(RhiBuffer *, const void *);

    RhiImage *(*image_create)(const RhiImageDesc *, const RhiSubresourceData *);
    RhiImage *(*image_retain)(RhiImage *);
    void  (*image_destroy)(RhiImage *);
    void  (*image_get_desc)(const RhiImage *, RhiImageDesc *);
    void  (*image_update)(RhiImage *, uint32_t, const RhiBox *, const void *, uint32_t, uint32_t);
    int   (*image_readback)(RhiImage *, uint32_t, void *, uint32_t);

    RhiView  *(*view_create)(RhiImage *, uint32_t, const RhiViewDesc *);
    void      (*view_destroy)(RhiView *);
    RhiImage *(*view_image)(const RhiView *);

    int   (*sample_count_supported)(RhiFormat, uint32_t);

    RhiShader *(*shader_create)(uint32_t, const RhiShaderSource *, char *, size_t);
    void       (*shader_destroy)(RhiShader *);
    RhiVertexLayout *(*vertex_layout_create)(const RhiVertexElement *, uint32_t, const RhiShader *);
    void       (*vertex_layout_destroy)(RhiVertexLayout *);

    RhiBlendState  *(*blend_state_create)(const RhiBlendDesc *);
    RhiDepthState  *(*depth_state_create)(const RhiDepthDesc *);
    RhiRasterState *(*raster_state_create)(const RhiRasterDesc *);
    RhiSampler     *(*sampler_create)(const RhiSamplerDesc *);
    void (*blend_state_destroy)(RhiBlendState *);
    void (*depth_state_destroy)(RhiDepthState *);
    void (*raster_state_destroy)(RhiRasterState *);
    void (*sampler_destroy)(RhiSampler *);

    void (*set_render_target)(RhiView *, RhiView *);
    void (*output_save)(RhiOutputState *);
    void (*output_restore)(RhiOutputState *);
    int  (*output_color_is)(const RhiOutputState *, const RhiView *);
    void (*set_viewports)(uint32_t, const RhiViewport *);
    void (*set_scissor)(const RhiRect *);
    void (*set_topology)(uint32_t);
    void (*set_vertex_layout)(RhiVertexLayout *);
    void (*set_vertex_buffer)(uint32_t, RhiBuffer *, uint32_t, uint32_t);
    void (*set_index_buffer)(RhiBuffer *, uint32_t, uint32_t);
    void (*set_shader)(uint32_t, RhiShader *);
    void (*set_uniform_buffers)(uint32_t, uint32_t, uint32_t, RhiBuffer *const *);
    void (*set_textures)(uint32_t, uint32_t, RhiView *const *);
    void (*set_samplers)(uint32_t, uint32_t, RhiSampler *const *);
    void (*set_blend_state)(RhiBlendState *, const float *, uint32_t);
    void (*set_depth_state)(RhiDepthState *, uint32_t);
    void (*set_raster_state)(RhiRasterState *);
    void (*draw)(uint32_t, uint32_t);
    void (*draw_indexed)(uint32_t, uint32_t, int32_t);
    void (*clear_color)(RhiView *, const float *);
    void (*clear_depth)(RhiView *, uint32_t, float, uint8_t);
} RhiBackend;

extern const RhiBackend rhi_d3d11_backend;
extern const RhiBackend rhi_vulkan_backend;     /* rhi_vulkan.c, RHI_HAVE_VULKAN */

/* The backend in use (rhi.c). */
extern const RhiBackend *g_rhi;

#endif /* XBOXRECOMP_RHI_BACKEND_H */
