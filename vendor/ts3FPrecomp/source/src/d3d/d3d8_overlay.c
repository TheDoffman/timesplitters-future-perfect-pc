/*
 * d3d8_overlay.c -- one line of text over the finished frame.
 *
 * See d3d8_overlay.h for what this touches on the device and what it
 * deliberately does not. The short version: no sampler, no vertex buffer, no
 * input layout worth restoring, and the render target and viewport are put
 * back, so a title's own drawing is unaffected.
 *
 * The text is drawn by GDI into a 32-bit DIB and uploaded to a texture when
 * it changes, which is roughly twice a second for a frame-rate counter. A
 * real font at a real size costs less code than an embedded glyph table and
 * reads better at any window size.
 */
#include "d3d8_internal.h"
#include "d3d8_overlay.h"

#include <stdio.h>
#include <string.h>

#define OVERLAY_W    512
#define OVERLAY_H     28
#define OVERLAY_PAD    8      /* from the top-left corner of the back buffer */

static struct {
    int             tried;     /* creation attempted */
    int             failed;    /* and gave up: never try again */
    RhiImage       *texture;
    RhiView        *srv;
    RhiBuffer      *cb;
    RhiShader      *vs;
    RhiShader      *ps;
    RhiBlendState  *blend;
    RhiDepthState  *depth;
    RhiRasterState *raster;
    HDC             dc;
    HBITMAP         bitmap;
    HFONT           font;
    void           *bits;      /* the DIB's pixels, BGRA top-down */
    int             text_w;    /* what the last render measured */
    char            text[128];
} g;

/* b7 in both stages: the four texture stages a title can use are b0..b3 and
 * the vertex layers take the low slots. x,y,w,h are in clip space. */
typedef struct {
    float x, y, w, h;
    float tex_w, tex_h, pad0, pad1;
} OverlayConstants;

static const char kShaderSource[] =
    "cbuffer Overlay : register(b7) {\n"
    "    float4 rect;        // x, y, w, h in clip space\n"
    "    float4 size;        // texture width, height\n"
    "};\n"
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vs_main(uint id : SV_VertexID) {\n"
    "    float2 c = float2(id & 1, id >> 1);\n"   /* 0,0  1,0  0,1  1,1 */
    "    VSOut o;\n"
    "    o.pos = float4(rect.x + c.x * rect.z, rect.y - c.y * rect.w, 0, 1);\n"
    "    o.uv  = c * size.xy;\n"
    "    return o;\n"
    "}\n"
    "Texture2D<float4> tex : register(t8);\n"
    "float4 ps_main(VSOut i) : SV_Target {\n"
    "    int2 p = int2(i.uv);\n"
    "    float a = tex.Load(int3(p, 0)).r;\n"
    /* The glyphs are white on black, so the red channel is coverage. A dark
     * panel under them keeps the text readable over a bright frame. */
    "    float3 rgb = lerp(float3(0, 0, 0), float3(1, 1, 1), a);\n"
    "    return float4(rgb, max(a, 0.45));\n"
    "}\n";

static void overlay_fail(const char *what)
{
    g.failed = 1;
    fprintf(stderr, "D3D8 overlay: %s failed; the overlay is off for "
            "this run\n", what);
    fflush(stderr);
    d3d8_overlay_shutdown();
}

static RhiShader *compile_one(uint32_t stage, const char *entry, const char *target)
{
    RhiShaderSource src;
    RhiShader *s;
    char err[2048];

    memset(&src, 0, sizeof src);
    src.hlsl = kShaderSource;
    src.len = sizeof kShaderSource - 1;
    src.name = "overlay";
    src.entry = entry;
    src.target = target;
    s = rhi_shader_create(stage, &src, err, sizeof err);
    if (!s && err[0])
        fprintf(stderr, "D3D8 overlay: %s\n", err);
    return s;
}

static int overlay_create(void)
{
    RhiImageDesc td;
    RhiBufferDesc bd;
    RhiBlendDesc bl;
    RhiDepthDesc ds;
    RhiRasterDesc rd;
    BITMAPINFO bi;

    if (g.failed) return 0;
    if (g.tried) return g.texture != NULL;
    g.tried = 1;
    if (!rhi_device_ready()) { g.failed = 1; return 0; }

    memset(&td, 0, sizeof td);
    td.type = RHI_IMAGE_2D;
    td.width = OVERLAY_W;
    td.height = OVERLAY_H;
    td.depth = 1;
    td.mip_levels = 1;
    td.format = RHI_FORMAT_B8G8R8A8_UNORM;
    td.samples = 1;
    td.usage = RHI_USAGE_DYNAMIC;
    td.bind = RHI_BIND_SAMPLED;
    td.cpu_access = RHI_CPU_WRITE;
    if (!(g.texture = rhi_image_create(&td, NULL))) { overlay_fail("CreateTexture2D"); return 0; }
    if (!(g.srv = rhi_view_create(g.texture, RHI_VIEW_SAMPLED, NULL))) {
        overlay_fail("CreateShaderResourceView");
        return 0;
    }

    memset(&bd, 0, sizeof bd);
    bd.size = sizeof(OverlayConstants);
    bd.usage = RHI_USAGE_DYNAMIC;
    bd.bind = RHI_BIND_UNIFORM;
    bd.cpu_access = RHI_CPU_WRITE;
    if (!(g.cb = rhi_buffer_create(&bd, NULL))) { overlay_fail("CreateBuffer"); return 0; }

    if (!(g.vs = compile_one(RHI_STAGE_VERTEX, "vs_main", "vs_4_0"))) {
        overlay_fail("the vertex shader");
        return 0;
    }
    if (!(g.ps = compile_one(RHI_STAGE_PIXEL, "ps_main", "ps_4_0"))) {
        overlay_fail("the pixel shader");
        return 0;
    }

    memset(&bl, 0, sizeof bl);
    bl.enable = 1;
    bl.src = RHI_BLEND_SRC_ALPHA;
    bl.dst = RHI_BLEND_INV_SRC_ALPHA;
    bl.op = RHI_BLEND_OP_ADD;
    bl.src_alpha = RHI_BLEND_ONE;
    bl.dst_alpha = RHI_BLEND_INV_SRC_ALPHA;
    bl.op_alpha = RHI_BLEND_OP_ADD;
    bl.write_mask = RHI_WRITE_ALL;
    if (!(g.blend = rhi_blend_state_create(&bl))) { overlay_fail("CreateBlendState"); return 0; }

    memset(&ds, 0, sizeof ds);
    if (!(g.depth = rhi_depth_state_create(&ds))) { overlay_fail("CreateDepthStencilState"); return 0; }

    memset(&rd, 0, sizeof rd);
    rd.fill = RHI_FILL_SOLID;
    rd.cull = RHI_CULL_NONE;
    rd.depth_clip = 1;
    if (!(g.raster = rhi_raster_state_create(&rd))) { overlay_fail("CreateRasterizerState"); return 0; }

    /* GDI side: a top-down 32-bit DIB the text is drawn into. */
    g.dc = CreateCompatibleDC(NULL);
    if (!g.dc) { overlay_fail("CreateCompatibleDC"); return 0; }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = OVERLAY_W;
    bi.bmiHeader.biHeight = -OVERLAY_H;          /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    g.bitmap = CreateDIBSection(g.dc, &bi, DIB_RGB_COLORS, &g.bits, NULL, 0);
    if (!g.bitmap) { overlay_fail("CreateDIBSection"); return 0; }
    SelectObject(g.dc, g.bitmap);
    g.font = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (g.font) SelectObject(g.dc, g.font);
    SetBkMode(g.dc, OPAQUE);
    SetBkColor(g.dc, RGB(0, 0, 0));
    SetTextColor(g.dc, RGB(255, 255, 255));
    return 1;
}

/* Draw the text into the DIB and upload it. Only when it has changed. */
static int overlay_set_text(const char *text)
{
    wchar_t wide[128];
    RECT rect;
    SIZE extent;
    int len, i;

    if (strcmp(text, g.text) == 0)
        return g.text_w > 0;

    len = (int)strlen(text);
    if (len > (int)(sizeof g.text - 1)) len = (int)(sizeof g.text - 1);
    memcpy(g.text, text, (size_t)len);
    g.text[len] = '\0';
    for (i = 0; i < len; i++)
        wide[i] = (wchar_t)(unsigned char)g.text[i];

    rect.left = rect.top = 0;
    rect.right = OVERLAY_W;
    rect.bottom = OVERLAY_H;
    memset(g.bits, 0, (size_t)OVERLAY_W * OVERLAY_H * 4);
    if (!GetTextExtentPoint32W(g.dc, wide, len, &extent))
        extent.cx = OVERLAY_W;
    g.text_w = extent.cx + 12 > OVERLAY_W ? OVERLAY_W : (int)extent.cx + 12;
    ExtTextOutW(g.dc, 6, 4, ETO_OPAQUE, &rect, wide, (UINT)len, NULL);
    GdiFlush();

    rhi_image_update(g.texture, 0, NULL, g.bits, OVERLAY_W * 4, 0);
    return g.text_w > 0;
}

void d3d8_overlay_draw(const char *text)
{
    RhiOutputState saved;
    RhiView *rtv = d3d8_GetDefaultTargetView();
    RhiViewport vp;
    OverlayConstants c;
    UINT width = d3d8_GetBackBufferWidth(), height = d3d8_GetBackBufferHeight();
    float blend_factor[4] = { 1, 1, 1, 1 };
    void *mapped;

    if (!text || !*text || !rtv || !width || !height)
        return;
    if (!overlay_create() || !overlay_set_text(text))
        return;

    rhi_output_save(&saved);

    if (!(mapped = rhi_buffer_map(g.cb, RHI_MAP_WRITE_DISCARD)))
        goto restore;
    c.x = -1.0f + 2.0f * (float)OVERLAY_PAD / (float)width;
    c.y =  1.0f - 2.0f * (float)OVERLAY_PAD / (float)height;
    c.w =  2.0f * (float)g.text_w / (float)width;
    c.h =  2.0f * (float)OVERLAY_H / (float)height;
    c.tex_w = (float)g.text_w;
    c.tex_h = (float)OVERLAY_H;
    c.pad0 = c.pad1 = 0.0f;
    memcpy(mapped, &c, sizeof c);
    rhi_buffer_unmap(g.cb);

    vp.x = vp.y = 0.0f;
    vp.width = (float)width;
    vp.height = (float)height;
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
    rhi_set_textures(8, 1, &g.srv);
    rhi_set_blend_state(g.blend, blend_factor, 0xFFFFFFFF);
    rhi_set_depth_state(g.depth, 0);
    rhi_set_raster_state(g.raster);
    rhi_draw(4, 0);

restore:
    rhi_output_restore(&saved);
}

void d3d8_overlay_shutdown(void)
{
    rhi_view_destroy(g.srv);             g.srv = NULL;
    rhi_image_destroy(g.texture);        g.texture = NULL;
    rhi_buffer_destroy(g.cb);            g.cb = NULL;
    rhi_shader_destroy(g.vs);            g.vs = NULL;
    rhi_shader_destroy(g.ps);            g.ps = NULL;
    rhi_blend_state_destroy(g.blend);    g.blend = NULL;
    rhi_depth_state_destroy(g.depth);    g.depth = NULL;
    rhi_raster_state_destroy(g.raster);  g.raster = NULL;
    if (g.bitmap)  { DeleteObject(g.bitmap);                     g.bitmap = NULL; }
    if (g.font)    { DeleteObject(g.font);                       g.font = NULL; }
    if (g.dc)      { DeleteDC(g.dc);                             g.dc = NULL; }
    g.bits = NULL;
    g.text[0] = '\0';
    g.text_w = 0;
    g.tried = 0;
}
