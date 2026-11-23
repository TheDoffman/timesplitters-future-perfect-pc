/* Typed synchronous host dispatch; included only by opengl_backend.c. */
typedef struct { const RhiDeviceDesc * a0; int result; } Call_device_create;
static void run_device_create(void *opaque)
{
    Call_device_create *data=opaque;
    if(!enter_backend(2))return;
    data->result=g_device_create(data->a0);leave_backend();
}
static int lease_device_create(const RhiDeviceDesc * a0)
{
    Call_device_create data={a0, -1};
    if(!tsfp_gl_host_run(run_device_create,&data,2))return -1;
    return data.result;
}

typedef struct {  int unused; } Call_device_destroy;
static void run_device_destroy(void *opaque)
{
    Call_device_destroy *data=opaque;(void)data;
    if(!enter_backend(3))return;
    g_device_destroy();leave_backend();
}
static void lease_device_destroy(void)
{
    Call_device_destroy data={0};
    tsfp_gl_host_run(run_device_destroy,&data,3);
    tsfp_gl_host_stop();
}

typedef struct {  int result; } Call_device_ready;
static void run_device_ready(void *opaque)
{
    Call_device_ready *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_ready();leave_backend();
}
static int lease_device_ready(void)
{
    Call_device_ready data={0};
    if(!tsfp_gl_host_run(run_device_ready,&data,0))return 0;
    return data.result;
}

typedef struct {  RhiView * result; } Call_swapchain_view;
static void run_swapchain_view(void *opaque)
{
    Call_swapchain_view *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_swap_view();leave_backend();
}
static RhiView * lease_swapchain_view(void)
{
    Call_swapchain_view data={NULL};
    if(!tsfp_gl_host_run(run_swapchain_view,&data,0))return NULL;
    return data.result;
}

typedef struct { uint32_t a0; uint32_t a1; int result; } Call_swapchain_resize;
static void run_swapchain_resize(void *opaque)
{
    Call_swapchain_resize *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_resize(data->a0, data->a1);leave_backend();
}
static int lease_swapchain_resize(uint32_t a0, uint32_t a1)
{
    Call_swapchain_resize data={a0, a1, -1};
    if(!tsfp_gl_host_run(run_swapchain_resize,&data,1))return -1;
    return data.result;
}

typedef struct { uint32_t a0; int32_t result; } Call_present;
static void run_present(void *opaque)
{
    Call_present *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_present(data->a0);leave_backend();
}
static int32_t lease_present(uint32_t a0)
{
    Call_present data={a0, -1};
    if(!tsfp_gl_host_run(run_present,&data,1))return -1;
    return data.result;
}

typedef struct { void * a0; uint32_t a1; uint32_t * a2; uint32_t * a3; int result; } Call_swapchain_readback;
static void run_swapchain_readback(void *opaque)
{
    Call_swapchain_readback *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_swap_readback(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static int lease_swapchain_readback(void * a0, uint32_t a1, uint32_t * a2, uint32_t * a3)
{
    Call_swapchain_readback data={a0, a1, a2, a3, -1};
    if(!tsfp_gl_host_run(run_swapchain_readback,&data,1))return -1;
    return data.result;
}

typedef struct { int a0; int unused; } Call_swapchain_set_vrr;
static void run_swapchain_set_vrr(void *opaque)
{
    Call_swapchain_set_vrr *data=opaque;
    if(!enter_backend(0))return;
    g_vrr(data->a0);leave_backend();
}
static void lease_swapchain_set_vrr(int a0)
{
    Call_swapchain_set_vrr data={a0, 0};
    tsfp_gl_host_run(run_swapchain_set_vrr,&data,0);
}

typedef struct { const RhiBufferDesc * a0; const void * a1; RhiBuffer * result; } Call_buffer_create;
static void run_buffer_create(void *opaque)
{
    Call_buffer_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_buffer_create(data->a0, data->a1);leave_backend();
}
static RhiBuffer * lease_buffer_create(const RhiBufferDesc * a0, const void * a1)
{
    Call_buffer_create data={a0, a1, NULL};
    if(!tsfp_gl_host_run(run_buffer_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiBuffer * a0; int unused; } Call_buffer_destroy;
static void run_buffer_destroy(void *opaque)
{
    Call_buffer_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_buffer_destroy(data->a0);leave_backend();
}
static void lease_buffer_destroy(RhiBuffer * a0)
{
    Call_buffer_destroy data={a0, 0};
    tsfp_gl_host_run(run_buffer_destroy,&data,0);
}

typedef struct { RhiBuffer * a0; uint32_t a1; void * result; } Call_buffer_map;
static void run_buffer_map(void *opaque)
{
    Call_buffer_map *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_buffer_map(data->a0, data->a1);leave_backend();
}
static void * lease_buffer_map(RhiBuffer * a0, uint32_t a1)
{
    Call_buffer_map data={a0, a1, NULL};
    if(!tsfp_gl_host_run(run_buffer_map,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiBuffer * a0; int unused; } Call_buffer_unmap;
static void run_buffer_unmap(void *opaque)
{
    Call_buffer_unmap *data=opaque;
    if(!enter_backend(1))return;
    g_buffer_unmap(data->a0);leave_backend();
}
static void lease_buffer_unmap(RhiBuffer * a0)
{
    Call_buffer_unmap data={a0, 0};
    tsfp_gl_host_run(run_buffer_unmap,&data,1);
}

typedef struct { RhiBuffer * a0; const void * a1; int unused; } Call_buffer_update;
static void run_buffer_update(void *opaque)
{
    Call_buffer_update *data=opaque;
    if(!enter_backend(1))return;
    g_buffer_update(data->a0, data->a1);leave_backend();
}
static void lease_buffer_update(RhiBuffer * a0, const void * a1)
{
    Call_buffer_update data={a0, a1, 0};
    tsfp_gl_host_run(run_buffer_update,&data,1);
}

typedef struct { const RhiImageDesc * a0; const RhiSubresourceData * a1; RhiImage * result; } Call_image_create;
static void run_image_create(void *opaque)
{
    Call_image_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_image_create(data->a0, data->a1);leave_backend();
}
static RhiImage * lease_image_create(const RhiImageDesc * a0, const RhiSubresourceData * a1)
{
    Call_image_create data={a0, a1, NULL};
    if(!tsfp_gl_host_run(run_image_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiImage * a0; RhiImage * result; } Call_image_retain;
static void run_image_retain(void *opaque)
{
    Call_image_retain *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_image_retain(data->a0);leave_backend();
}
static RhiImage * lease_image_retain(RhiImage * a0)
{
    Call_image_retain data={a0, NULL};
    if(!tsfp_gl_host_run(run_image_retain,&data,0))return NULL;
    return data.result;
}

typedef struct { RhiImage * a0; int unused; } Call_image_destroy;
static void run_image_destroy(void *opaque)
{
    Call_image_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_image_destroy(data->a0);leave_backend();
}
static void lease_image_destroy(RhiImage * a0)
{
    Call_image_destroy data={a0, 0};
    tsfp_gl_host_run(run_image_destroy,&data,0);
}

typedef struct { const RhiImage * a0; RhiImageDesc * a1; int unused; } Call_image_get_desc;
static void run_image_get_desc(void *opaque)
{
    Call_image_get_desc *data=opaque;
    if(!enter_backend(0))return;
    g_image_desc(data->a0, data->a1);leave_backend();
}
static void lease_image_get_desc(const RhiImage * a0, RhiImageDesc * a1)
{
    Call_image_get_desc data={a0, a1, 0};
    tsfp_gl_host_run(run_image_get_desc,&data,0);
}

typedef struct { RhiImage * a0; uint32_t a1; const RhiBox * a2; const void * a3; uint32_t a4; uint32_t a5; int unused; } Call_image_update;
static void run_image_update(void *opaque)
{
    Call_image_update *data=opaque;
    if(!enter_backend(1))return;
    g_image_update(data->a0, data->a1, data->a2, data->a3, data->a4, data->a5);leave_backend();
}
static void lease_image_update(RhiImage * a0, uint32_t a1, const RhiBox * a2, const void * a3, uint32_t a4, uint32_t a5)
{
    Call_image_update data={a0, a1, a2, a3, a4, a5, 0};
    tsfp_gl_host_run(run_image_update,&data,1);
}

typedef struct { RhiImage * a0; uint32_t a1; void * a2; uint32_t a3; int result; } Call_image_readback;
static void run_image_readback(void *opaque)
{
    Call_image_readback *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_image_readback(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static int lease_image_readback(RhiImage * a0, uint32_t a1, void * a2, uint32_t a3)
{
    Call_image_readback data={a0, a1, a2, a3, -1};
    if(!tsfp_gl_host_run(run_image_readback,&data,1))return -1;
    return data.result;
}

typedef struct { RhiImage * a0; uint32_t a1; const RhiViewDesc * a2; RhiView  * result; } Call_view_create;
static void run_view_create(void *opaque)
{
    Call_view_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_view_create(data->a0, data->a1, data->a2);leave_backend();
}
static RhiView  * lease_view_create(RhiImage * a0, uint32_t a1, const RhiViewDesc * a2)
{
    Call_view_create data={a0, a1, a2, NULL};
    if(!tsfp_gl_host_run(run_view_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiView * a0; int unused; } Call_view_destroy;
static void run_view_destroy(void *opaque)
{
    Call_view_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_view_destroy(data->a0);leave_backend();
}
static void lease_view_destroy(RhiView * a0)
{
    Call_view_destroy data={a0, 0};
    tsfp_gl_host_run(run_view_destroy,&data,0);
}

typedef struct { const RhiView * a0; RhiImage * result; } Call_view_image;
static void run_view_image(void *opaque)
{
    Call_view_image *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_view_image(data->a0);leave_backend();
}
static RhiImage * lease_view_image(const RhiView * a0)
{
    Call_view_image data={a0, NULL};
    if(!tsfp_gl_host_run(run_view_image,&data,0))return NULL;
    return data.result;
}

typedef struct { RhiFormat a0; uint32_t a1; int result; } Call_sample_count_supported;
static void run_sample_count_supported(void *opaque)
{
    Call_sample_count_supported *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_sample_count(data->a0, data->a1);leave_backend();
}
static int lease_sample_count_supported(RhiFormat a0, uint32_t a1)
{
    Call_sample_count_supported data={a0, a1, 0};
    if(!tsfp_gl_host_run(run_sample_count_supported,&data,1))return 0;
    return data.result;
}

typedef struct { uint32_t a0; const RhiShaderSource * a1; char * a2; size_t a3; RhiShader * result; } Call_shader_create;
static void run_shader_create(void *opaque)
{
    Call_shader_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_shader_create(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static RhiShader * lease_shader_create(uint32_t a0, const RhiShaderSource * a1, char * a2, size_t a3)
{
    Call_shader_create data={a0, a1, a2, a3, NULL};
    if(!tsfp_gl_host_run(run_shader_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiShader * a0; int unused; } Call_shader_destroy;
static void run_shader_destroy(void *opaque)
{
    Call_shader_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_shader_destroy(data->a0);leave_backend();
}
static void lease_shader_destroy(RhiShader * a0)
{
    Call_shader_destroy data={a0, 0};
    tsfp_gl_host_run(run_shader_destroy,&data,0);
}

typedef struct { const RhiVertexElement * a0; uint32_t a1; const RhiShader * a2; RhiVertexLayout * result; } Call_vertex_layout_create;
static void run_vertex_layout_create(void *opaque)
{
    Call_vertex_layout_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_layout_create(data->a0, data->a1, data->a2);leave_backend();
}
static RhiVertexLayout * lease_vertex_layout_create(const RhiVertexElement * a0, uint32_t a1, const RhiShader * a2)
{
    Call_vertex_layout_create data={a0, a1, a2, NULL};
    if(!tsfp_gl_host_run(run_vertex_layout_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiVertexLayout * a0; int unused; } Call_vertex_layout_destroy;
static void run_vertex_layout_destroy(void *opaque)
{
    Call_vertex_layout_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_layout_destroy(data->a0);leave_backend();
}
static void lease_vertex_layout_destroy(RhiVertexLayout * a0)
{
    Call_vertex_layout_destroy data={a0, 0};
    tsfp_gl_host_run(run_vertex_layout_destroy,&data,0);
}

typedef struct { const RhiBlendDesc * a0; RhiBlendState  * result; } Call_blend_state_create;
static void run_blend_state_create(void *opaque)
{
    Call_blend_state_create *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_blend_create(data->a0);leave_backend();
}
static RhiBlendState  * lease_blend_state_create(const RhiBlendDesc * a0)
{
    Call_blend_state_create data={a0, NULL};
    if(!tsfp_gl_host_run(run_blend_state_create,&data,0))return NULL;
    return data.result;
}

typedef struct { const RhiDepthDesc * a0; RhiDepthState  * result; } Call_depth_state_create;
static void run_depth_state_create(void *opaque)
{
    Call_depth_state_create *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_depth_create(data->a0);leave_backend();
}
static RhiDepthState  * lease_depth_state_create(const RhiDepthDesc * a0)
{
    Call_depth_state_create data={a0, NULL};
    if(!tsfp_gl_host_run(run_depth_state_create,&data,0))return NULL;
    return data.result;
}

typedef struct { const RhiRasterDesc * a0; RhiRasterState * result; } Call_raster_state_create;
static void run_raster_state_create(void *opaque)
{
    Call_raster_state_create *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_raster_create(data->a0);leave_backend();
}
static RhiRasterState * lease_raster_state_create(const RhiRasterDesc * a0)
{
    Call_raster_state_create data={a0, NULL};
    if(!tsfp_gl_host_run(run_raster_state_create,&data,0))return NULL;
    return data.result;
}

typedef struct { const RhiSamplerDesc * a0; RhiSampler     * result; } Call_sampler_create;
static void run_sampler_create(void *opaque)
{
    Call_sampler_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_sampler_create(data->a0);leave_backend();
}
static RhiSampler     * lease_sampler_create(const RhiSamplerDesc * a0)
{
    Call_sampler_create data={a0, NULL};
    if(!tsfp_gl_host_run(run_sampler_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiBlendState * a0; int unused; } Call_blend_state_destroy;
static void run_blend_state_destroy(void *opaque)
{
    Call_blend_state_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_blend_destroy(data->a0);leave_backend();
}
static void lease_blend_state_destroy(RhiBlendState * a0)
{
    Call_blend_state_destroy data={a0, 0};
    tsfp_gl_host_run(run_blend_state_destroy,&data,0);
}

typedef struct { RhiDepthState * a0; int unused; } Call_depth_state_destroy;
static void run_depth_state_destroy(void *opaque)
{
    Call_depth_state_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_depth_destroy(data->a0);leave_backend();
}
static void lease_depth_state_destroy(RhiDepthState * a0)
{
    Call_depth_state_destroy data={a0, 0};
    tsfp_gl_host_run(run_depth_state_destroy,&data,0);
}

typedef struct { RhiRasterState * a0; int unused; } Call_raster_state_destroy;
static void run_raster_state_destroy(void *opaque)
{
    Call_raster_state_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_raster_destroy(data->a0);leave_backend();
}
static void lease_raster_state_destroy(RhiRasterState * a0)
{
    Call_raster_state_destroy data={a0, 0};
    tsfp_gl_host_run(run_raster_state_destroy,&data,0);
}

typedef struct { RhiSampler * a0; int unused; } Call_sampler_destroy;
static void run_sampler_destroy(void *opaque)
{
    Call_sampler_destroy *data=opaque;
    if(!enter_backend(0))return;
    g_sampler_destroy(data->a0);leave_backend();
}
static void lease_sampler_destroy(RhiSampler * a0)
{
    Call_sampler_destroy data={a0, 0};
    tsfp_gl_host_run(run_sampler_destroy,&data,0);
}

typedef struct { RhiView * a0; RhiView * a1; int unused; } Call_set_render_target;
static void run_set_render_target(void *opaque)
{
    Call_set_render_target *data=opaque;
    if(!enter_backend(1))return;
    g_set_render_target(data->a0, data->a1);leave_backend();
}
static void lease_set_render_target(RhiView * a0, RhiView * a1)
{
    Call_set_render_target data={a0, a1, 0};
    tsfp_gl_host_run(run_set_render_target,&data,1);
}

typedef struct { RhiOutputState * a0; int unused; } Call_output_save;
static void run_output_save(void *opaque)
{
    Call_output_save *data=opaque;
    if(!enter_backend(0))return;
    g_output_save(data->a0);leave_backend();
}
static void lease_output_save(RhiOutputState * a0)
{
    Call_output_save data={a0, 0};
    tsfp_gl_host_run(run_output_save,&data,0);
}

typedef struct { RhiOutputState * a0; int unused; } Call_output_restore;
static void run_output_restore(void *opaque)
{
    Call_output_restore *data=opaque;
    if(!enter_backend(1))return;
    g_output_restore(data->a0);leave_backend();
}
static void lease_output_restore(RhiOutputState * a0)
{
    Call_output_restore data={a0, 0};
    tsfp_gl_host_run(run_output_restore,&data,1);
}

typedef struct { const RhiOutputState * a0; const RhiView * a1; int result; } Call_output_color_is;
static void run_output_color_is(void *opaque)
{
    Call_output_color_is *data=opaque;
    if(!enter_backend(0))return;
    data->result=g_output_color_is(data->a0, data->a1);leave_backend();
}
static int lease_output_color_is(const RhiOutputState * a0, const RhiView * a1)
{
    Call_output_color_is data={a0, a1, 0};
    if(!tsfp_gl_host_run(run_output_color_is,&data,0))return 0;
    return data.result;
}

typedef struct { uint32_t a0; const RhiViewport * a1; int unused; } Call_set_viewports;
static void run_set_viewports(void *opaque)
{
    Call_set_viewports *data=opaque;
    if(!enter_backend(0))return;
    g_viewports(data->a0, data->a1);leave_backend();
}
static void lease_set_viewports(uint32_t a0, const RhiViewport * a1)
{
    Call_set_viewports data={a0, a1, 0};
    tsfp_gl_host_run(run_set_viewports,&data,0);
}

typedef struct { const RhiRect * a0; int unused; } Call_set_scissor;
static void run_set_scissor(void *opaque)
{
    Call_set_scissor *data=opaque;
    if(!enter_backend(0))return;
    g_scissor(data->a0);leave_backend();
}
static void lease_set_scissor(const RhiRect * a0)
{
    Call_set_scissor data={a0, 0};
    tsfp_gl_host_run(run_set_scissor,&data,0);
}

typedef struct { uint32_t a0; int unused; } Call_set_topology;
static void run_set_topology(void *opaque)
{
    Call_set_topology *data=opaque;
    if(!enter_backend(0))return;
    g_topology(data->a0);leave_backend();
}
static void lease_set_topology(uint32_t a0)
{
    Call_set_topology data={a0, 0};
    tsfp_gl_host_run(run_set_topology,&data,0);
}

typedef struct { RhiVertexLayout * a0; int unused; } Call_set_vertex_layout;
static void run_set_vertex_layout(void *opaque)
{
    Call_set_vertex_layout *data=opaque;
    if(!enter_backend(0))return;
    g_set_layout(data->a0);leave_backend();
}
static void lease_set_vertex_layout(RhiVertexLayout * a0)
{
    Call_set_vertex_layout data={a0, 0};
    tsfp_gl_host_run(run_set_vertex_layout,&data,0);
}

typedef struct { uint32_t a0; RhiBuffer * a1; uint32_t a2; uint32_t a3; int unused; } Call_set_vertex_buffer;
static void run_set_vertex_buffer(void *opaque)
{
    Call_set_vertex_buffer *data=opaque;
    if(!enter_backend(0))return;
    g_vertex_buffer(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static void lease_set_vertex_buffer(uint32_t a0, RhiBuffer * a1, uint32_t a2, uint32_t a3)
{
    Call_set_vertex_buffer data={a0, a1, a2, a3, 0};
    tsfp_gl_host_run(run_set_vertex_buffer,&data,0);
}

typedef struct { RhiBuffer * a0; uint32_t a1; uint32_t a2; int unused; } Call_set_index_buffer;
static void run_set_index_buffer(void *opaque)
{
    Call_set_index_buffer *data=opaque;
    if(!enter_backend(0))return;
    g_index_buffer(data->a0, data->a1, data->a2);leave_backend();
}
static void lease_set_index_buffer(RhiBuffer * a0, uint32_t a1, uint32_t a2)
{
    Call_set_index_buffer data={a0, a1, a2, 0};
    tsfp_gl_host_run(run_set_index_buffer,&data,0);
}

typedef struct { uint32_t a0; RhiShader * a1; int unused; } Call_set_shader;
static void run_set_shader(void *opaque)
{
    Call_set_shader *data=opaque;
    if(!enter_backend(0))return;
    g_set_shader(data->a0, data->a1);leave_backend();
}
static void lease_set_shader(uint32_t a0, RhiShader * a1)
{
    Call_set_shader data={a0, a1, 0};
    tsfp_gl_host_run(run_set_shader,&data,0);
}

typedef struct { uint32_t a0; uint32_t a1; uint32_t a2; RhiBuffer *const * a3; int unused; } Call_set_uniform_buffers;
static void run_set_uniform_buffers(void *opaque)
{
    Call_set_uniform_buffers *data=opaque;
    if(!enter_backend(0))return;
    g_uniforms(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static void lease_set_uniform_buffers(uint32_t a0, uint32_t a1, uint32_t a2, RhiBuffer *const * a3)
{
    Call_set_uniform_buffers data={a0, a1, a2, a3, 0};
    tsfp_gl_host_run(run_set_uniform_buffers,&data,0);
}

typedef struct { uint32_t a0; uint32_t a1; RhiView *const * a2; int unused; } Call_set_textures;
static void run_set_textures(void *opaque)
{
    Call_set_textures *data=opaque;
    if(!enter_backend(0))return;
    g_textures(data->a0, data->a1, data->a2);leave_backend();
}
static void lease_set_textures(uint32_t a0, uint32_t a1, RhiView *const * a2)
{
    Call_set_textures data={a0, a1, a2, 0};
    tsfp_gl_host_run(run_set_textures,&data,0);
}

typedef struct { uint32_t a0; uint32_t a1; RhiSampler *const * a2; int unused; } Call_set_samplers;
static void run_set_samplers(void *opaque)
{
    Call_set_samplers *data=opaque;
    if(!enter_backend(0))return;
    g_samplers(data->a0, data->a1, data->a2);leave_backend();
}
static void lease_set_samplers(uint32_t a0, uint32_t a1, RhiSampler *const * a2)
{
    Call_set_samplers data={a0, a1, a2, 0};
    tsfp_gl_host_run(run_set_samplers,&data,0);
}

typedef struct { RhiBlendState * a0; const float * a1; uint32_t a2; int unused; } Call_set_blend_state;
static void run_set_blend_state(void *opaque)
{
    Call_set_blend_state *data=opaque;
    if(!enter_backend(0))return;
    g_blend(data->a0, data->a1, data->a2);leave_backend();
}
static void lease_set_blend_state(RhiBlendState * a0, const float * a1, uint32_t a2)
{
    Call_set_blend_state data={a0, a1, a2, 0};
    tsfp_gl_host_run(run_set_blend_state,&data,0);
}

typedef struct { RhiDepthState * a0; uint32_t a1; int unused; } Call_set_depth_state;
static void run_set_depth_state(void *opaque)
{
    Call_set_depth_state *data=opaque;
    if(!enter_backend(0))return;
    g_depth(data->a0, data->a1);leave_backend();
}
static void lease_set_depth_state(RhiDepthState * a0, uint32_t a1)
{
    Call_set_depth_state data={a0, a1, 0};
    tsfp_gl_host_run(run_set_depth_state,&data,0);
}

typedef struct { RhiRasterState * a0; int unused; } Call_set_raster_state;
static void run_set_raster_state(void *opaque)
{
    Call_set_raster_state *data=opaque;
    if(!enter_backend(0))return;
    g_raster(data->a0);leave_backend();
}
static void lease_set_raster_state(RhiRasterState * a0)
{
    Call_set_raster_state data={a0, 0};
    tsfp_gl_host_run(run_set_raster_state,&data,0);
}

typedef struct { uint32_t a0; uint32_t a1; int unused; } Call_draw;
static void run_draw(void *opaque)
{
    Call_draw *data=opaque;
    if(!enter_backend(1))return;
    g_draw(data->a0, data->a1);leave_backend();
}
static void lease_draw(uint32_t a0, uint32_t a1)
{
    Call_draw data={a0, a1, 0};
    tsfp_gl_host_run(run_draw,&data,1);
}

typedef struct { uint32_t a0; uint32_t a1; int32_t a2; int unused; } Call_draw_indexed;
static void run_draw_indexed(void *opaque)
{
    Call_draw_indexed *data=opaque;
    if(!enter_backend(1))return;
    g_draw_indexed(data->a0, data->a1, data->a2);leave_backend();
}
static void lease_draw_indexed(uint32_t a0, uint32_t a1, int32_t a2)
{
    Call_draw_indexed data={a0, a1, a2, 0};
    tsfp_gl_host_run(run_draw_indexed,&data,1);
}

typedef struct { RhiView * a0; const float * a1; int unused; } Call_clear_color;
static void run_clear_color(void *opaque)
{
    Call_clear_color *data=opaque;
    if(!enter_backend(1))return;
    g_clear_color(data->a0, data->a1);leave_backend();
}
static void lease_clear_color(RhiView * a0, const float * a1)
{
    Call_clear_color data={a0, a1, 0};
    tsfp_gl_host_run(run_clear_color,&data,1);
}

typedef struct { RhiView * a0; uint32_t a1; float a2; uint8_t a3; int unused; } Call_clear_depth;
static void run_clear_depth(void *opaque)
{
    Call_clear_depth *data=opaque;
    if(!enter_backend(1))return;
    g_clear_depth(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static void lease_clear_depth(RhiView * a0, uint32_t a1, float a2, uint8_t a3)
{
    Call_clear_depth data={a0, a1, a2, a3, 0};
    tsfp_gl_host_run(run_clear_depth,&data,1);
}

typedef struct { RhiImage * a0; uint32_t a1; RhiImage * a2; uint32_t a3; int result; } Call_image_resolve;
static void run_image_resolve(void *opaque)
{
    Call_image_resolve *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_resolve(data->a0, data->a1, data->a2, data->a3);leave_backend();
}
static int lease_image_resolve(RhiImage * a0, uint32_t a1, RhiImage * a2, uint32_t a3)
{
    Call_image_resolve data={a0, a1, a2, a3, -1};
    if(!tsfp_gl_host_run(run_image_resolve,&data,1))return -1;
    return data.result;
}

typedef struct {  RhiOcclusion * result; } Call_occlusion_create;
static void run_occlusion_create(void *opaque)
{
    Call_occlusion_create *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_query_create();leave_backend();
}
static RhiOcclusion * lease_occlusion_create(void)
{
    Call_occlusion_create data={NULL};
    if(!tsfp_gl_host_run(run_occlusion_create,&data,1))return NULL;
    return data.result;
}

typedef struct { RhiOcclusion * a0; int unused; } Call_occlusion_destroy;
static void run_occlusion_destroy(void *opaque)
{
    Call_occlusion_destroy *data=opaque;
    if(!enter_backend(1))return;
    g_query_destroy(data->a0);leave_backend();
}
static void lease_occlusion_destroy(RhiOcclusion * a0)
{
    Call_occlusion_destroy data={a0, 0};
    tsfp_gl_host_run(run_occlusion_destroy,&data,1);
}

typedef struct { RhiOcclusion * a0; int result; } Call_occlusion_begin;
static void run_occlusion_begin(void *opaque)
{
    Call_occlusion_begin *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_query_begin(data->a0);leave_backend();
}
static int lease_occlusion_begin(RhiOcclusion * a0)
{
    Call_occlusion_begin data={a0, 0};
    if(!tsfp_gl_host_run(run_occlusion_begin,&data,1))return 0;
    return data.result;
}

typedef struct { RhiOcclusion * a0; int result; } Call_occlusion_end;
static void run_occlusion_end(void *opaque)
{
    Call_occlusion_end *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_query_end(data->a0);leave_backend();
}
static int lease_occlusion_end(RhiOcclusion * a0)
{
    Call_occlusion_end data={a0, 0};
    if(!tsfp_gl_host_run(run_occlusion_end,&data,1))return 0;
    return data.result;
}

typedef struct { RhiOcclusion * a0; uint64_t * a1; int result; } Call_occlusion_result;
static void run_occlusion_result(void *opaque)
{
    Call_occlusion_result *data=opaque;
    if(!enter_backend(1))return;
    data->result=g_query_result(data->a0, data->a1);leave_backend();
}
static int lease_occlusion_result(RhiOcclusion * a0, uint64_t * a1)
{
    Call_occlusion_result data={a0, a1, -1};
    if(!tsfp_gl_host_run(run_occlusion_result,&data,1))return -1;
    return data.result;
}
