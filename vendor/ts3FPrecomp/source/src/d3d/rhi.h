/**
 * rhi.h -- the renderer's host graphics interface.
 *
 * Everything in src/d3d that talks to a GPU API goes through here, so the
 * Xbox knowledge above it -- format tables, swizzle, P8, cube and volume
 * layout, FVF, the vertex-program and register-combiner translators -- is
 * written once and drawn by either backend: Direct3D 11 (rhi_d3d11.c) or
 * Vulkan. docs/technical/vulkan-backend.md, sections 2 and 3, is the design.
 *
 * The shape, and why:
 *
 *  - Objects are explicit handles (buffers, images, views, shaders, vertex
 *    layouts, state objects, samplers). Each is one D3D11 object and one
 *    Vulkan object (a VkImageView per view), so the D3D11 backend is a thin
 *    wrapper whose calls are the ones the renderer made before -- which is
 *    what lets every captured frame replay byte-identically through it.
 *  - Enumerations keep the D3D11 / DXGI numbering. RhiFormat is the DXGI
 *    format number, so capture files (which store DXGI format numbers in
 *    D3D8CapVsInput) need no version bump. rhi_d3d11.c asserts every value
 *    at compile time.
 *  - Binding looks immediate: set a shader, a layout, state objects,
 *    buffers, textures, then draw. A Vulkan backend resolves the pipeline at
 *    draw time from what is bound; the renderer already assembles its state
 *    at draw time (d3d8_shaders_prepare_draw, d3d8_states_apply), so nothing
 *    above this line has to change shape for it.
 *  - Two places depart from D3D11 on purpose, because Vulkan cannot do them
 *    the D3D11 way cheaply: reading an image back is one synchronous call
 *    (rhi_image_readback) rather than a staging texture and a Map, and
 *    writing a dynamic image is rhi_image_update rather than a Map.
 *
 * Contract every caller relies on (it was the D3D11 code's contract too):
 * the main draw path re-binds shaders, layouts, state objects, constant
 * buffers and samplers on every draw, so a self-contained pass (overlay,
 * movie, screen copy, display resolve) may leave those changed. It must put
 * back the render target and viewport (rhi_output_save / restore) and must
 * not leave its own texture bound in slots 0-3.
 *
 * No platform header is included: this compiles on Linux.
 */
#ifndef XBOXRECOMP_RHI_H
#define XBOXRECOMP_RHI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- handles --------------------------------------------------------------- */

typedef struct RhiBuffer       RhiBuffer;
typedef struct RhiImage        RhiImage;
typedef struct RhiView         RhiView;
typedef struct RhiShader       RhiShader;
typedef struct RhiVertexLayout RhiVertexLayout;
typedef struct RhiBlendState   RhiBlendState;
typedef struct RhiDepthState   RhiDepthState;
typedef struct RhiRasterState  RhiRasterState;
typedef struct RhiSampler      RhiSampler;

/* ---- formats: DXGI_FORMAT numbers ------------------------------------------ */

typedef uint32_t RhiFormat;
enum {
    RHI_FORMAT_UNKNOWN            = 0,
    RHI_FORMAT_R32G32B32A32_FLOAT = 2,
    RHI_FORMAT_R32G32B32A32_SINT  = 4,
    RHI_FORMAT_R32G32B32_FLOAT    = 6,
    RHI_FORMAT_R16G16B16A16_FLOAT = 10,
    RHI_FORMAT_R16G16B16A16_UNORM = 11,
    RHI_FORMAT_R16G16B16A16_SNORM = 13,
    RHI_FORMAT_R32G32_FLOAT       = 16,
    RHI_FORMAT_R10G10B10A2_UNORM  = 24,
    RHI_FORMAT_R11G11B10_FLOAT    = 26,
    RHI_FORMAT_R8G8B8A8_UNORM     = 28,
    RHI_FORMAT_R8G8B8A8_SNORM     = 31,
    RHI_FORMAT_R16G16_FLOAT       = 34,
    RHI_FORMAT_R16G16_UNORM       = 35,
    RHI_FORMAT_R16G16_SNORM       = 37,
    RHI_FORMAT_D32_FLOAT          = 40,
    RHI_FORMAT_R32_FLOAT          = 41,
    RHI_FORMAT_R32_UINT           = 42,
    RHI_FORMAT_D24_UNORM_S8_UINT  = 45,
    RHI_FORMAT_R8G8_UNORM         = 49,
    RHI_FORMAT_R8G8_SNORM         = 51,
    RHI_FORMAT_R16_FLOAT          = 54,
    RHI_FORMAT_D16_UNORM          = 55,
    RHI_FORMAT_R16_UNORM          = 56,
    RHI_FORMAT_R16_UINT           = 57,
    RHI_FORMAT_R16_SNORM          = 58,
    RHI_FORMAT_R8_UNORM           = 61,
    RHI_FORMAT_A8_UNORM           = 65,
    RHI_FORMAT_BC1_UNORM          = 71,
    RHI_FORMAT_BC2_UNORM          = 74,
    RHI_FORMAT_BC3_UNORM          = 77,
    RHI_FORMAT_BC5_UNORM          = 83,
    RHI_FORMAT_B5G6R5_UNORM       = 85,
    RHI_FORMAT_B5G5R5A1_UNORM     = 86,
    RHI_FORMAT_B8G8R8A8_UNORM     = 87,
    RHI_FORMAT_B8G8R8X8_UNORM     = 88,
    RHI_FORMAT_B4G4R4A4_UNORM     = 115
};

/* ---- enumerations: D3D11 numbers -------------------------------------------- */

enum { /* RhiUsage: D3D11_USAGE */
    RHI_USAGE_DEFAULT = 0, RHI_USAGE_IMMUTABLE = 1, RHI_USAGE_DYNAMIC = 2
};
enum { /* bind flags: D3D11_BIND_* */
    RHI_BIND_VERTEX = 0x1, RHI_BIND_INDEX = 0x2, RHI_BIND_UNIFORM = 0x4,
    RHI_BIND_SAMPLED = 0x8, RHI_BIND_RENDER_TARGET = 0x20, RHI_BIND_DEPTH = 0x40
};
enum { /* D3D11_CPU_ACCESS_WRITE */
    RHI_CPU_WRITE = 0x10000
};
enum { /* D3D11_MAP */
    RHI_MAP_WRITE_DISCARD = 4, RHI_MAP_WRITE_NO_OVERWRITE = 5
};
enum { /* D3D11_PRIMITIVE_TOPOLOGY */
    RHI_TOPOLOGY_UNDEFINED = 0, RHI_TOPOLOGY_POINTS = 1, RHI_TOPOLOGY_LINES = 2,
    RHI_TOPOLOGY_LINE_STRIP = 3, RHI_TOPOLOGY_TRIANGLES = 4, RHI_TOPOLOGY_TRIANGLE_STRIP = 5
};
enum { /* D3D11_BLEND */
    RHI_BLEND_ZERO = 1, RHI_BLEND_ONE = 2, RHI_BLEND_SRC_COLOR = 3, RHI_BLEND_INV_SRC_COLOR = 4,
    RHI_BLEND_SRC_ALPHA = 5, RHI_BLEND_INV_SRC_ALPHA = 6, RHI_BLEND_DEST_ALPHA = 7,
    RHI_BLEND_INV_DEST_ALPHA = 8, RHI_BLEND_DEST_COLOR = 9, RHI_BLEND_INV_DEST_COLOR = 10,
    RHI_BLEND_SRC_ALPHA_SAT = 11, RHI_BLEND_FACTOR = 14, RHI_BLEND_INV_FACTOR = 15
};
enum { /* D3D11_BLEND_OP */
    RHI_BLEND_OP_ADD = 1, RHI_BLEND_OP_SUBTRACT = 2, RHI_BLEND_OP_REV_SUBTRACT = 3,
    RHI_BLEND_OP_MIN = 4, RHI_BLEND_OP_MAX = 5
};
enum { /* D3D11_COLOR_WRITE_ENABLE */
    RHI_WRITE_R = 1, RHI_WRITE_G = 2, RHI_WRITE_B = 4, RHI_WRITE_A = 8, RHI_WRITE_ALL = 15
};
enum { /* D3D11_COMPARISON_FUNC */
    RHI_CMP_NEVER = 1, RHI_CMP_LESS = 2, RHI_CMP_EQUAL = 3, RHI_CMP_LESS_EQUAL = 4,
    RHI_CMP_GREATER = 5, RHI_CMP_NOT_EQUAL = 6, RHI_CMP_GREATER_EQUAL = 7, RHI_CMP_ALWAYS = 8
};
enum { /* D3D11_STENCIL_OP */
    RHI_STENCIL_KEEP = 1, RHI_STENCIL_ZERO = 2, RHI_STENCIL_REPLACE = 3, RHI_STENCIL_INCR_SAT = 4,
    RHI_STENCIL_DECR_SAT = 5, RHI_STENCIL_INVERT = 6, RHI_STENCIL_INCR = 7, RHI_STENCIL_DECR = 8
};
enum { /* D3D11_CULL_MODE */
    RHI_CULL_NONE = 1, RHI_CULL_FRONT = 2, RHI_CULL_BACK = 3
};
enum { /* D3D11_FILL_MODE */
    RHI_FILL_WIREFRAME = 2, RHI_FILL_SOLID = 3
};
enum { /* D3D11_TEXTURE_ADDRESS_MODE */
    RHI_ADDRESS_WRAP = 1, RHI_ADDRESS_MIRROR = 2, RHI_ADDRESS_CLAMP = 3,
    RHI_ADDRESS_BORDER = 4, RHI_ADDRESS_MIRROR_ONCE = 5
};
/* D3D11_FILTER encoding: bit 0 mip linear, bit 2 mag linear, bit 4 min
 * linear, 0x55 anisotropic, 0x80 comparison. */
enum {
    RHI_FILTER_POINT                      = 0x00,
    RHI_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT = 0x04,
    RHI_FILTER_MIN_LINEAR_MAG_MIP_POINT   = 0x10,
    RHI_FILTER_MIN_MAG_LINEAR_MIP_POINT   = 0x14,
    RHI_FILTER_LINEAR                     = 0x15,
    RHI_FILTER_ANISOTROPIC                = 0x55
};

/* ---- descriptions ------------------------------------------------------------ */

typedef struct {
    uint32_t size;                  /* bytes */
    uint32_t usage;                 /* RHI_USAGE_* */
    uint32_t bind;                  /* RHI_BIND_VERTEX / INDEX / UNIFORM */
    uint32_t cpu_access;            /* RHI_CPU_WRITE for dynamic buffers */
} RhiBufferDesc;

enum { RHI_IMAGE_2D = 0, RHI_IMAGE_3D = 1 };

typedef struct {
    uint32_t  type;                 /* RHI_IMAGE_2D or RHI_IMAGE_3D */
    uint32_t  width, height;
    uint32_t  depth;                /* 3D: depth; 2D: array layers (6 for a cube) */
    uint32_t  mip_levels;
    RhiFormat format;
    uint32_t  samples, sample_quality;
    uint32_t  usage;                /* RHI_USAGE_* */
    uint32_t  bind;                 /* RHI_BIND_SAMPLED / RENDER_TARGET / DEPTH */
    uint32_t  cpu_access;           /* RHI_CPU_WRITE for dynamic images */
    uint32_t  cube;                 /* nonzero: the six layers are a cube */
} RhiImageDesc;

/* Initial contents of one subresource (D3D11_SUBRESOURCE_DATA). */
typedef struct {
    const void *data;
    uint32_t    row_pitch, slice_pitch;
} RhiSubresourceData;

enum { RHI_VIEW_SAMPLED = 0, RHI_VIEW_RENDER_TARGET = 1, RHI_VIEW_DEPTH = 2 };
enum {
    RHI_VIEW_DIM_2D = 0, RHI_VIEW_DIM_2D_ARRAY = 1, RHI_VIEW_DIM_2D_MS = 2,
    RHI_VIEW_DIM_CUBE = 3, RHI_VIEW_DIM_3D = 4
};

typedef struct {
    RhiFormat format;               /* RHI_FORMAT_UNKNOWN: the image's own */
    uint32_t  dim;                  /* RHI_VIEW_DIM_* */
    uint32_t  base_mip, mip_count;
    uint32_t  base_layer, layer_count;
} RhiViewDesc;

typedef struct {
    uint32_t enable;
    uint32_t src, dst, op;          /* colour: RHI_BLEND_*, RHI_BLEND_OP_* */
    uint32_t src_alpha, dst_alpha, op_alpha;
    uint32_t write_mask;            /* RHI_WRITE_* */
    uint32_t alpha_to_coverage;
} RhiBlendDesc;

typedef struct {
    uint32_t fail, depth_fail, pass;    /* RHI_STENCIL_* */
    uint32_t func;                      /* RHI_CMP_* */
} RhiStencilFace;

typedef struct {
    uint32_t depth_enable, depth_write;
    uint32_t depth_func;                /* RHI_CMP_* */
    uint32_t stencil_enable;
    uint8_t  stencil_read_mask, stencil_write_mask;
    RhiStencilFace front, back;
} RhiDepthDesc;

typedef struct {
    uint32_t fill, cull;                /* RHI_FILL_*, RHI_CULL_* */
    uint32_t front_ccw;
    int32_t  depth_bias;
    float    depth_bias_clamp, slope_scaled_depth_bias;
    uint32_t depth_clip, scissor, multisample, antialiased_lines;
} RhiRasterDesc;

typedef struct {
    uint32_t filter;                    /* RHI_FILTER_* (D3D11 encoding) */
    uint32_t address_u, address_v, address_w;
    float    mip_lod_bias;
    uint32_t max_anisotropy;
    uint32_t compare;                   /* RHI_CMP_* */
    float    border[4];
    float    min_lod, max_lod;
} RhiSamplerDesc;

typedef struct {
    const char *semantic;
    uint32_t    semantic_index;
    RhiFormat   format;
    uint32_t    slot;
    uint32_t    offset;                 /* RHI_APPEND: after the previous element */
} RhiVertexElement;
#define RHI_APPEND 0xFFFFFFFFu

enum { RHI_STAGE_VERTEX = 0, RHI_STAGE_PIXEL = 1 };

typedef struct {
    const char *name, *value;
} RhiMacro;

/* One HLSL source. It is compiled through d3d8_compile_hlsl's door, so
 * RECOMP_D3D8_HLSL_DIR sees it whichever backend is running. `target` is
 * the D3D11 profile (vs_5_0, ps_4_0 ...); other backends map it. */
typedef struct {
    const char     *hlsl;
    size_t          len;
    const char     *name;
    const RhiMacro *macros;             /* NULL, or ended by a NULL name */
    const char     *entry;
    const char     *target;
    uint32_t        optimize;           /* nonzero: the highest level */
} RhiShaderSource;

typedef struct {
    float x, y, width, height, min_depth, max_depth;
} RhiViewport;

typedef struct {
    int32_t left, top, right, bottom;
} RhiRect;

typedef struct {
    uint32_t left, top, front, right, bottom, back;
} RhiBox;

/* What a self-contained pass saves and puts back: the render target, the
 * depth target and the viewports. Opaque; its members belong to the
 * backend. */
typedef struct {
    void       *color, *depth;
    uint32_t    viewport_count;
    RhiViewport viewports[16];
} RhiOutputState;

enum { RHI_CLEAR_DEPTH = 1, RHI_CLEAR_STENCIL = 2 };

/* ---- the calls ----------------------------------------------------------------- */

/* The device and its swap chain */
typedef struct {
    void    *window;            /* the native window: an HWND on Windows */
    uint32_t width, height;     /* the swap chain's size */
    uint32_t buffer_count;
    uint32_t windowed;
    uint32_t debug;             /* ask for the debug / validation layer */
} RhiDeviceDesc;

/* Chooses the backend before the device is created. NULL or "" means
 * RECOMP_D3D8_BACKEND, and that unset means d3d11. Returns 0 when the
 * named backend exists; otherwise it says so and keeps d3d11. */
int         rhi_select_backend(const char *name);
const char *rhi_backend_name(void);
int         rhi_device_create(const RhiDeviceDesc *desc);   /* 0 on success */
void        rhi_device_destroy(void);
/* Whether a device exists: the test the renderer makes before touching
 * the GPU at all (it used to ask for the D3D11 device pointer). */
int         rhi_device_ready(void);
/* The render-target view of the swap chain's back buffer, which the
 * display resolve draws into; NULL while it is being recreated. */
RhiView    *rhi_swapchain_view(void);
/* Follow the window: new back buffers of this size. 0 on success. */
int         rhi_swapchain_resize(uint32_t width, uint32_t height);
/* Show the back buffer. interval 0 must not block (vulkan-backend.md,
 * section 4.10). Returns the backend's status, 0 or positive on success
 * (D3D11 passes its HRESULT through). */
int32_t     rhi_present(uint32_t interval);
/* The swap chain's back buffer as the next present will show it, as RGBA8
 * rows at dst_pitch, whatever the swap chain's own format (a BGRA swap
 * chain is swizzled on the way). dst NULL only reports the size. 0 on
 * success. Replay's --present and the gate read the display resolve's
 * output through this. */
int         rhi_swapchain_readback(void *dst, uint32_t dst_pitch,
                                   uint32_t *width, uint32_t *height);
/* Present for a variable-refresh display, or stop. On: each present goes to
 * the display as soon as it is made (no waiting for a vblank, nothing held
 * back for the next one), so the display can refresh when a frame arrives;
 * only right while the window owns its screen. Off: the default, where a
 * windowed swap chain is paced by the compositor. Neither blocks (rhi_present
 * with interval 0). Takes effect at the next present; call it from the
 * thread that presents. */
void        rhi_swapchain_set_vrr(int on);

/* Buffers */
RhiBuffer *rhi_buffer_create(const RhiBufferDesc *desc, const void *initial);
void       rhi_buffer_destroy(RhiBuffer *b);
/* A dynamic buffer: WRITE_DISCARD for new contents, WRITE_NO_OVERWRITE to
 * append to a ring the GPU is still reading. NULL on failure. */
void      *rhi_buffer_map(RhiBuffer *b, uint32_t mode);
void       rhi_buffer_unmap(RhiBuffer *b);
/* A default-usage buffer's whole contents. */
void       rhi_buffer_update(RhiBuffer *b, const void *data);

/* Images and views */
RhiImage  *rhi_image_create(const RhiImageDesc *desc, const RhiSubresourceData *initial);
/* Images are reference counted: a surface aliasing a level of a texture
 * holds the texture's image. create returns one reference; destroy drops
 * one, and the image goes with the last. */
RhiImage  *rhi_image_retain(RhiImage *img);
void       rhi_image_destroy(RhiImage *img);
void       rhi_image_get_desc(const RhiImage *img, RhiImageDesc *out);
/* New texels for one subresource (mip + layer * mip_levels), or a box of
 * it. Dynamic images replace the whole subresource and box must be NULL. */
void       rhi_image_update(RhiImage *img, uint32_t subresource, const RhiBox *box,
                            const void *data, uint32_t row_pitch, uint32_t slice_pitch);
/* The texels of one 2D subresource, copied into dst at dst_pitch, resolved
 * first if the image is multisampled. Synchronous: work recorded before it
 * is finished before it returns. Returns 0 on success. This is the one door
 * every probe that looks at pixels uses -- frame dumps, F11, the brightness
 * and frame-buffer probes, replay --out -- so it keeps working on any
 * backend. */
int        rhi_image_readback(RhiImage *img, uint32_t subresource,
                              void *dst, uint32_t dst_pitch);

/* desc may be NULL: the whole image, viewed as the image says. */
RhiView   *rhi_view_create(RhiImage *img, uint32_t kind, const RhiViewDesc *desc);
void       rhi_view_destroy(RhiView *v);
RhiImage  *rhi_view_image(const RhiView *v);

/* Whether a render target or depth image of this format can have this
 * many samples. */
int        rhi_sample_count_supported(RhiFormat format, uint32_t samples);

/* Sizes of a format's storage, backend-independent: bytes in one row of
 * `width` texels (a row of 4x4 blocks for BC formats), and how many such
 * rows `height` texels take. 0 for a format this table does not know. */
uint32_t   rhi_format_row_pitch(RhiFormat format, uint32_t width);
uint32_t   rhi_format_rows(RhiFormat format, uint32_t height);

/* Shaders and vertex layouts. err may be NULL. */
RhiShader *rhi_shader_create(uint32_t stage, const RhiShaderSource *src,
                             char *err, size_t err_len);
void       rhi_shader_destroy(RhiShader *s);
RhiVertexLayout *rhi_vertex_layout_create(const RhiVertexElement *elements, uint32_t count,
                                          const RhiShader *vs);
void       rhi_vertex_layout_destroy(RhiVertexLayout *l);

/* State objects */
RhiBlendState  *rhi_blend_state_create(const RhiBlendDesc *d);
RhiDepthState  *rhi_depth_state_create(const RhiDepthDesc *d);
RhiRasterState *rhi_raster_state_create(const RhiRasterDesc *d);
RhiSampler     *rhi_sampler_create(const RhiSamplerDesc *d);
void rhi_blend_state_destroy(RhiBlendState *s);
void rhi_depth_state_destroy(RhiDepthState *s);
void rhi_raster_state_destroy(RhiRasterState *s);
void rhi_sampler_destroy(RhiSampler *s);

/* Binding and drawing */
void rhi_set_render_target(RhiView *color, RhiView *depth);
void rhi_output_save(RhiOutputState *s);
void rhi_output_restore(RhiOutputState *s);   /* also releases what save held */
/* Whether the colour target a save recorded is this view: how a pass asks
 * "was drawing going to the scene?" without knowing the backend. */
int  rhi_output_color_is(const RhiOutputState *s, const RhiView *v);
void rhi_set_viewports(uint32_t count, const RhiViewport *vps);
void rhi_set_scissor(const RhiRect *rect);
void rhi_set_topology(uint32_t topology);
void rhi_set_vertex_layout(RhiVertexLayout *l);
void rhi_set_vertex_buffer(uint32_t slot, RhiBuffer *b, uint32_t stride, uint32_t offset);
void rhi_set_index_buffer(RhiBuffer *b, uint32_t bits, uint32_t offset);  /* bits: 16 or 32 */
void rhi_set_shader(uint32_t stage, RhiShader *s);
void rhi_set_uniform_buffers(uint32_t stage, uint32_t slot, uint32_t count, RhiBuffer *const *b);
void rhi_set_textures(uint32_t slot, uint32_t count, RhiView *const *views);     /* pixel stage */
void rhi_set_samplers(uint32_t slot, uint32_t count, RhiSampler *const *samplers); /* pixel stage */
void rhi_set_blend_state(RhiBlendState *s, const float factor[4], uint32_t sample_mask);
void rhi_set_depth_state(RhiDepthState *s, uint32_t stencil_ref);
void rhi_set_raster_state(RhiRasterState *s);
void rhi_draw(uint32_t vertex_count, uint32_t first_vertex);
void rhi_draw_indexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);
void rhi_clear_color(RhiView *v, const float rgba[4]);
void rhi_clear_depth(RhiView *v, uint32_t flags, float depth, uint8_t stencil);

/* ---- backends ----------------------------------------------------------------- */

/* For a test that makes its own D3D11 device: hand it to the backend in
 * place of rhi_device_create (no swap chain). */
int  rhi_d3d11_adopt(void *id3d11_device, void *id3d11_context);
/* For the D3D11-only code that has not moved behind this interface yet:
 * the native object behind a handle. */
void *rhi_d3d11_native_buffer(const RhiBuffer *b);
void *rhi_d3d11_native_image(const RhiImage *img);
void *rhi_d3d11_native_view(const RhiView *v);
RhiView *rhi_d3d11_wrap_view(void *id3d11_view, uint32_t kind);  /* AddRefs it */
RhiImage *rhi_d3d11_wrap_image(void *id3d11_resource);           /* AddRefs it */

#ifdef __cplusplus
}
#endif

#endif /* XBOXRECOMP_RHI_H */
