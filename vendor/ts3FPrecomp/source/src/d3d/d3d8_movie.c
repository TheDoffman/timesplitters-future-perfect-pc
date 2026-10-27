/*
 * d3d8_movie.c -- see d3d8_movie.h.
 *
 * Built the way d3d8_overlay.c is: its own texture, shaders and states, the
 * quad from SV_VertexID, no sampler (the pixel shader filters by Load, four
 * texels, so the title's sampler state is never touched and the picture is
 * still smooth at any window size), and the render target and viewport put
 * back afterwards.
 *
 * Frames arrive on whichever guest thread polls the movie and are drawn at
 * Swap, so the CPU copy is guarded and the upload happens at draw time, on
 * the device's thread.
 */
#include "d3d8_internal.h"
#include "d3d8_movie.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    int             tried, failed;
    RhiImage       *texture;
    RhiView        *srv;
    uint32_t        tex_w, tex_h;
    RhiBuffer      *cb;
    RhiShader      *vs;
    RhiShader      *ps;
    RhiBlendState  *blend;
    RhiDepthState  *depth;
    RhiRasterState *raster;

    SRWLOCK         lock;
    uint8_t        *frame;       /* latest frame, BGRA */
    uint32_t        frame_w, frame_h;
    int             dirty;       /* not uploaded yet */
    int             have;        /* a frame to draw */
} g = { 0, 0, NULL, NULL, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, SRWLOCK_INIT };

typedef struct {
    float x, y, w, h;          /* the picture's rectangle, clip space */
    float tex_w, tex_h, pad0, pad1;
} MovieConstants;

static const char kShaderSource[] =
    "cbuffer Movie : register(b7) {\n"
    "    float4 rect;\n"
    "    float4 size;\n"
    "};\n"
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vs_main(uint id : SV_VertexID) {\n"
    "    float2 c = float2(id & 1, id >> 1);\n"
    "    VSOut o;\n"
    "    o.pos = float4(rect.x + c.x * rect.z, rect.y - c.y * rect.w, 0, 1);\n"
    "    o.uv  = c * size.xy;\n"
    "    return o;\n"
    "}\n"
    "Texture2D<float4> tex : register(t9);\n"
    "float4 texel(int2 p) {\n"
    "    p = clamp(p, int2(0, 0), int2(size.xy) - 1);\n"
    "    return tex.Load(int3(p, 0));\n"
    "}\n"
    "float4 ps_main(VSOut i) : SV_Target {\n"
    "    float2 t = i.uv - 0.5;\n"
    "    int2 p = int2(floor(t));\n"
    "    float2 f = t - floor(t);\n"
    "    float4 a = lerp(texel(p), texel(p + int2(1, 0)), f.x);\n"
    "    float4 b = lerp(texel(p + int2(0, 1)), texel(p + int2(1, 1)), f.x);\n"
    "    return float4(lerp(a, b, f.y).rgb, 1);\n"
    "}\n";

static void movie_fail(const char *what)
{
    g.failed = 1;
    fprintf(stderr, "D3D8 movie: %s failed; movies are not drawn this run\n", what);
    fflush(stderr);
}

static RhiShader *compile(uint32_t stage, const char *entry, const char *profile)
{
    RhiShaderSource src;
    RhiShader *s;
    char err[2048];

    memset(&src, 0, sizeof src);
    src.hlsl = kShaderSource;
    src.len = sizeof kShaderSource - 1;
    src.name = "movie";
    src.entry = entry;
    src.target = profile;
    s = rhi_shader_create(stage, &src, err, sizeof err);
    if (!s) {
        if (err[0])
            fprintf(stderr, "D3D8 movie: %s\n", err);
        movie_fail("shader creation");
    }
    return s;
}

static int movie_create(void)
{
    RhiBufferDesc bd;
    RhiBlendDesc bl;
    RhiDepthDesc ds;
    RhiRasterDesc rd;

    if (g.failed)
        return 0;
    if (g.tried)
        return g.vs != NULL;
    g.tried = 1;
    if (!rhi_device_ready()) {
        g.failed = 1;
        return 0;
    }
    memset(&bd, 0, sizeof bd);
    bd.size = sizeof(MovieConstants);
    bd.usage = RHI_USAGE_DYNAMIC;
    bd.bind = RHI_BIND_UNIFORM;
    bd.cpu_access = RHI_CPU_WRITE;
    if (!(g.cb = rhi_buffer_create(&bd, NULL))) { movie_fail("CreateBuffer"); return 0; }

    if (!(g.vs = compile(RHI_STAGE_VERTEX, "vs_main", "vs_4_0")))
        return 0;
    if (!(g.ps = compile(RHI_STAGE_PIXEL, "ps_main", "ps_4_0"))) {
        rhi_shader_destroy(g.vs);
        g.vs = NULL;
        return 0;
    }

    memset(&bl, 0, sizeof bl);
    bl.write_mask = RHI_WRITE_ALL;
    if (!(g.blend = rhi_blend_state_create(&bl))) { movie_fail("CreateBlendState"); return 0; }
    memset(&ds, 0, sizeof ds);
    if (!(g.depth = rhi_depth_state_create(&ds))) { movie_fail("CreateDepthStencilState"); return 0; }
    memset(&rd, 0, sizeof rd);
    rd.fill = RHI_FILL_SOLID;
    rd.cull = RHI_CULL_NONE;
    rd.depth_clip = 1;
    if (!(g.raster = rhi_raster_state_create(&rd))) { movie_fail("CreateRasterizerState"); return 0; }
    return 1;
}

/* A texture of the frame's size, remade when the size changes. */
static int movie_texture(uint32_t w, uint32_t h)
{
    RhiImageDesc td;

    if (g.texture && g.tex_w == w && g.tex_h == h)
        return 1;
    rhi_view_destroy(g.srv);      g.srv = NULL;
    rhi_image_destroy(g.texture); g.texture = NULL;
    memset(&td, 0, sizeof td);
    td.type = RHI_IMAGE_2D;
    td.width = w;
    td.height = h;
    td.depth = 1;
    td.mip_levels = 1;
    td.format = RHI_FORMAT_B8G8R8A8_UNORM;
    td.samples = 1;
    td.usage = RHI_USAGE_DYNAMIC;
    td.bind = RHI_BIND_SAMPLED;
    td.cpu_access = RHI_CPU_WRITE;
    if (!(g.texture = rhi_image_create(&td, NULL))) { movie_fail("CreateTexture2D"); return 0; }
    if (!(g.srv = rhi_view_create(g.texture, RHI_VIEW_SAMPLED, NULL))) {
        movie_fail("CreateShaderResourceView");
        return 0;
    }
    g.tex_w = w;
    g.tex_h = h;
    return 1;
}

void d3d8_movie_set_frame(const uint8_t *bgra, uint32_t width, uint32_t height)
{
    size_t bytes = (size_t)width * height * 4u;

    if (!bgra || !width || !height)
        return;
    AcquireSRWLockExclusive(&g.lock);
    if (g.frame_w != width || g.frame_h != height) {
        free(g.frame);
        g.frame = (uint8_t *)malloc(bytes);
        g.frame_w = g.frame ? width : 0;
        g.frame_h = g.frame ? height : 0;
    }
    if (g.frame) {
        memcpy(g.frame, bgra, bytes);
        g.dirty = g.have = 1;
    }
    ReleaseSRWLockExclusive(&g.lock);
}

void d3d8_movie_clear(void)
{
    AcquireSRWLockExclusive(&g.lock);
    g.have = 0;
    ReleaseSRWLockExclusive(&g.lock);
}

void d3d8_movie_draw(void)
{
    RhiView *rtv = d3d8_GetDefaultTargetView();
    RhiOutputState saved;
    RhiViewport vp;
    UINT bw = d3d8_GetBackBufferWidth(), bh = d3d8_GetBackBufferHeight();
    MovieConstants c;
    float blend_factor[4] = { 1, 1, 1, 1 }, black[4] = { 0, 0, 0, 1 };
    float fa, ba;
    uint32_t w, h;
    void *mapped;

    if (!rtv || !bw || !bh || !g.have || !movie_create())
        return;

    AcquireSRWLockExclusive(&g.lock);
    w = g.frame_w;
    h = g.frame_h;
    if (!g.have || !g.frame || !movie_texture(w, h)) {
        ReleaseSRWLockExclusive(&g.lock);
        return;
    }
    if (g.dirty) {
        rhi_image_update(g.texture, 0, NULL, g.frame, w * 4u, 0);
        g.dirty = 0;
    }
    ReleaseSRWLockExclusive(&g.lock);

    rhi_output_save(&saved);

    /* The whole screen goes black first, then the picture at its own aspect:
     * a 720x576 PAL movie and a 640x480 one both fill a 4:3 window, and
     * neither is stretched to a wide one. Xbox movies are encoded for a 4:3
     * screen whatever their pixel size, so 4:3 is the aspect used. */
    rhi_clear_color(rtv, black);
    if (!(mapped = rhi_buffer_map(g.cb, RHI_MAP_WRITE_DISCARD)))
        goto restore;
    fa = 4.0f / 3.0f;
    ba = (float)bw / (float)bh;
    c.w = ba > fa ? 2.0f * fa / ba : 2.0f;
    c.h = ba > fa ? 2.0f : 2.0f * ba / fa;
    c.x = -c.w / 2.0f;
    c.y = c.h / 2.0f;
    c.tex_w = (float)w;
    c.tex_h = (float)h;
    c.pad0 = c.pad1 = 0.0f;
    memcpy(mapped, &c, sizeof c);
    rhi_buffer_unmap(g.cb);

    vp.x = vp.y = 0.0f;
    vp.width = (float)bw;
    vp.height = (float)bh;
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    rhi_set_render_target(rtv, NULL);
    rhi_set_viewports(1, &vp);
    rhi_set_vertex_layout(NULL);
    rhi_set_topology(RHI_TOPOLOGY_TRIANGLE_STRIP);
    rhi_set_shader(RHI_STAGE_VERTEX, g.vs);
    rhi_set_shader(RHI_STAGE_PIXEL, g.ps);
    rhi_set_uniform_buffers(RHI_STAGE_VERTEX, 7, 1, &g.cb);
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL, 7, 1, &g.cb);
    rhi_set_textures(9, 1, &g.srv);
    rhi_set_blend_state(g.blend, blend_factor, 0xFFFFFFFF);
    rhi_set_depth_state(g.depth, 0);
    rhi_set_raster_state(g.raster);
    rhi_draw(4, 0);

restore:
    rhi_output_restore(&saved);
}

void d3d8_movie_shutdown(void)
{
    rhi_view_destroy(g.srv);             g.srv = NULL;
    rhi_image_destroy(g.texture);        g.texture = NULL;
    rhi_buffer_destroy(g.cb);            g.cb = NULL;
    rhi_shader_destroy(g.vs);            g.vs = NULL;
    rhi_shader_destroy(g.ps);            g.ps = NULL;
    rhi_blend_state_destroy(g.blend);    g.blend = NULL;
    rhi_depth_state_destroy(g.depth);    g.depth = NULL;
    rhi_raster_state_destroy(g.raster);  g.raster = NULL;
    g.tex_w = g.tex_h = 0;
    g.tried = 0;
}
