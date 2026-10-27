/**
 * rhi_d3d11.c -- the Direct3D 11 backend of rhi.h.
 *
 * Each call is the D3D11 call the renderer made before it went through the
 * interface, with the same arguments, so a captured frame replays
 * byte-identically through it (scripts/replay_ab.py is the test). The RHI's
 * enumerations carry D3D11 and DXGI numbers; the asserts below are what
 * keeps that true.
 */

#if defined(_WIN32)

#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rhi_backend.h"
#include "rhi_shader_cache.h"
#include "d3d8_internal.h"

/* ---- the numbering the RHI borrows ------------------------------------------ */

#define SAME(a, b) typedef char rhi_same_##a[((int)(a) == (int)(b)) ? 1 : -1]
SAME(RHI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN);
SAME(RHI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT);
SAME(RHI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32G32B32A32_SINT);
SAME(RHI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT);
SAME(RHI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT);
SAME(RHI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM);
SAME(RHI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_SNORM);
SAME(RHI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32_FLOAT);
SAME(RHI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM);
SAME(RHI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT);
SAME(RHI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM);
SAME(RHI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SNORM);
SAME(RHI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16_FLOAT);
SAME(RHI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UNORM);
SAME(RHI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_SNORM);
SAME(RHI_FORMAT_D32_FLOAT, DXGI_FORMAT_D32_FLOAT);
SAME(RHI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT);
SAME(RHI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT);
SAME(RHI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D24_UNORM_S8_UINT);
SAME(RHI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM);
SAME(RHI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_SNORM);
SAME(RHI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT);
SAME(RHI_FORMAT_D16_UNORM, DXGI_FORMAT_D16_UNORM);
SAME(RHI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM);
SAME(RHI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT);
SAME(RHI_FORMAT_R16_SNORM, DXGI_FORMAT_R16_SNORM);
SAME(RHI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM);
SAME(RHI_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM);
SAME(RHI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM);
SAME(RHI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM);
SAME(RHI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM);
SAME(RHI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC5_UNORM);
SAME(RHI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G6R5_UNORM);
SAME(RHI_FORMAT_B5G5R5A1_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM);
SAME(RHI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM);
SAME(RHI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM);
SAME(RHI_FORMAT_B4G4R4A4_UNORM, DXGI_FORMAT_B4G4R4A4_UNORM);
SAME(RHI_USAGE_DEFAULT, D3D11_USAGE_DEFAULT);
SAME(RHI_USAGE_IMMUTABLE, D3D11_USAGE_IMMUTABLE);
SAME(RHI_USAGE_DYNAMIC, D3D11_USAGE_DYNAMIC);
SAME(RHI_BIND_VERTEX, D3D11_BIND_VERTEX_BUFFER);
SAME(RHI_BIND_INDEX, D3D11_BIND_INDEX_BUFFER);
SAME(RHI_BIND_UNIFORM, D3D11_BIND_CONSTANT_BUFFER);
SAME(RHI_BIND_SAMPLED, D3D11_BIND_SHADER_RESOURCE);
SAME(RHI_BIND_RENDER_TARGET, D3D11_BIND_RENDER_TARGET);
SAME(RHI_BIND_DEPTH, D3D11_BIND_DEPTH_STENCIL);
SAME(RHI_CPU_WRITE, D3D11_CPU_ACCESS_WRITE);
SAME(RHI_MAP_WRITE_DISCARD, D3D11_MAP_WRITE_DISCARD);
SAME(RHI_MAP_WRITE_NO_OVERWRITE, D3D11_MAP_WRITE_NO_OVERWRITE);
SAME(RHI_TOPOLOGY_UNDEFINED, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
SAME(RHI_TOPOLOGY_POINTS, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
SAME(RHI_TOPOLOGY_LINES, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
SAME(RHI_TOPOLOGY_LINE_STRIP, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
SAME(RHI_TOPOLOGY_TRIANGLES, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
SAME(RHI_TOPOLOGY_TRIANGLE_STRIP, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
SAME(RHI_BLEND_ZERO, D3D11_BLEND_ZERO);
SAME(RHI_BLEND_ONE, D3D11_BLEND_ONE);
SAME(RHI_BLEND_SRC_COLOR, D3D11_BLEND_SRC_COLOR);
SAME(RHI_BLEND_INV_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR);
SAME(RHI_BLEND_SRC_ALPHA, D3D11_BLEND_SRC_ALPHA);
SAME(RHI_BLEND_INV_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA);
SAME(RHI_BLEND_DEST_ALPHA, D3D11_BLEND_DEST_ALPHA);
SAME(RHI_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA);
SAME(RHI_BLEND_DEST_COLOR, D3D11_BLEND_DEST_COLOR);
SAME(RHI_BLEND_INV_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR);
SAME(RHI_BLEND_SRC_ALPHA_SAT, D3D11_BLEND_SRC_ALPHA_SAT);
SAME(RHI_BLEND_FACTOR, D3D11_BLEND_BLEND_FACTOR);
SAME(RHI_BLEND_INV_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR);
SAME(RHI_BLEND_OP_ADD, D3D11_BLEND_OP_ADD);
SAME(RHI_BLEND_OP_SUBTRACT, D3D11_BLEND_OP_SUBTRACT);
SAME(RHI_BLEND_OP_REV_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT);
SAME(RHI_BLEND_OP_MIN, D3D11_BLEND_OP_MIN);
SAME(RHI_BLEND_OP_MAX, D3D11_BLEND_OP_MAX);
SAME(RHI_WRITE_ALL, D3D11_COLOR_WRITE_ENABLE_ALL);
SAME(RHI_CMP_NEVER, D3D11_COMPARISON_NEVER);
SAME(RHI_CMP_LESS, D3D11_COMPARISON_LESS);
SAME(RHI_CMP_EQUAL, D3D11_COMPARISON_EQUAL);
SAME(RHI_CMP_LESS_EQUAL, D3D11_COMPARISON_LESS_EQUAL);
SAME(RHI_CMP_GREATER, D3D11_COMPARISON_GREATER);
SAME(RHI_CMP_NOT_EQUAL, D3D11_COMPARISON_NOT_EQUAL);
SAME(RHI_CMP_GREATER_EQUAL, D3D11_COMPARISON_GREATER_EQUAL);
SAME(RHI_CMP_ALWAYS, D3D11_COMPARISON_ALWAYS);
SAME(RHI_STENCIL_KEEP, D3D11_STENCIL_OP_KEEP);
SAME(RHI_STENCIL_ZERO, D3D11_STENCIL_OP_ZERO);
SAME(RHI_STENCIL_REPLACE, D3D11_STENCIL_OP_REPLACE);
SAME(RHI_STENCIL_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT);
SAME(RHI_STENCIL_DECR_SAT, D3D11_STENCIL_OP_DECR_SAT);
SAME(RHI_STENCIL_INVERT, D3D11_STENCIL_OP_INVERT);
SAME(RHI_STENCIL_INCR, D3D11_STENCIL_OP_INCR);
SAME(RHI_STENCIL_DECR, D3D11_STENCIL_OP_DECR);
SAME(RHI_CULL_NONE, D3D11_CULL_NONE);
SAME(RHI_CULL_FRONT, D3D11_CULL_FRONT);
SAME(RHI_CULL_BACK, D3D11_CULL_BACK);
SAME(RHI_FILL_WIREFRAME, D3D11_FILL_WIREFRAME);
SAME(RHI_FILL_SOLID, D3D11_FILL_SOLID);
SAME(RHI_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
SAME(RHI_ADDRESS_MIRROR, D3D11_TEXTURE_ADDRESS_MIRROR);
SAME(RHI_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
SAME(RHI_ADDRESS_BORDER, D3D11_TEXTURE_ADDRESS_BORDER);
SAME(RHI_ADDRESS_MIRROR_ONCE, D3D11_TEXTURE_ADDRESS_MIRROR_ONCE);
SAME(RHI_FILTER_POINT, D3D11_FILTER_MIN_MAG_MIP_POINT);
SAME(RHI_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
SAME(RHI_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
SAME(RHI_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
SAME(RHI_FILTER_LINEAR, D3D11_FILTER_MIN_MAG_MIP_LINEAR);
SAME(RHI_FILTER_ANISOTROPIC, D3D11_FILTER_ANISOTROPIC);
SAME(RHI_CLEAR_DEPTH, D3D11_CLEAR_DEPTH);
SAME(RHI_CLEAR_STENCIL, D3D11_CLEAR_STENCIL);
SAME(RHI_APPEND, D3D11_APPEND_ALIGNED_ELEMENT);
#undef SAME

/* ---- the objects --------------------------------------------------------------- */

struct RhiBuffer       { ID3D11Buffer *b; RhiBufferDesc desc; };
struct RhiImage        { ID3D11Resource *res; RhiImageDesc desc; LONG refs; };
struct RhiView         { ID3D11View *v; uint32_t kind; RhiImage *image; };
struct RhiShader       { uint32_t stage; ID3D11DeviceChild *sh; ID3DBlob *blob; };
struct RhiVertexLayout { ID3D11InputLayout *il; };
struct RhiBlendState   { ID3D11BlendState *s; };
struct RhiDepthState   { ID3D11DepthStencilState *s; };
struct RhiRasterState  { ID3D11RasterizerState *s; };
struct RhiSampler      { ID3D11SamplerState *s; };

static ID3D11Device        *g_dev;
static ID3D11DeviceContext *g_ctx;
static IDXGISwapChain      *g_swap;
static ID3D11RenderTargetView *g_present_rtv;
static RhiView             *g_present_view;

int rhi_d3d11_adopt(void *device, void *context)
{
    g_dev = (ID3D11Device *)device;
    g_ctx = (ID3D11DeviceContext *)context;
    return g_dev && g_ctx ? 0 : -1;
}

void *rhi_d3d11_native_buffer(const RhiBuffer *b) { return b ? b->b : NULL; }
void *rhi_d3d11_native_image(const RhiImage *i)   { return i ? i->res : NULL; }
void *rhi_d3d11_native_view(const RhiView *v)     { return v ? v->v : NULL; }

RhiView *rhi_d3d11_wrap_view(void *native, uint32_t kind)
{
    RhiView *v;
    if (!native)
        return NULL;
    v = calloc(1, sizeof *v);
    if (!v)
        return NULL;
    v->v = (ID3D11View *)native;
    v->kind = kind;
    ID3D11View_AddRef(v->v);
    return v;
}

RhiImage *rhi_d3d11_wrap_image(void *native)
{
    ID3D11Resource *res = (ID3D11Resource *)native;
    D3D11_RESOURCE_DIMENSION dim;
    RhiImage *img;

    if (!res)
        return NULL;
    img = calloc(1, sizeof *img);
    if (!img)
        return NULL;
    ID3D11Resource_GetType(res, &dim);
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D11_TEXTURE3D_DESC d;
        ID3D11Texture3D_GetDesc((ID3D11Texture3D *)res, &d);
        img->desc.type = RHI_IMAGE_3D;
        img->desc.width = d.Width;
        img->desc.height = d.Height;
        img->desc.depth = d.Depth;
        img->desc.mip_levels = d.MipLevels;
        img->desc.format = d.Format;
        img->desc.usage = d.Usage;
        img->desc.bind = d.BindFlags;
        img->desc.cpu_access = d.CPUAccessFlags;
    } else {
        D3D11_TEXTURE2D_DESC d;
        ID3D11Texture2D_GetDesc((ID3D11Texture2D *)res, &d);
        img->desc.type = RHI_IMAGE_2D;
        img->desc.width = d.Width;
        img->desc.height = d.Height;
        img->desc.depth = d.ArraySize;
        img->desc.mip_levels = d.MipLevels;
        img->desc.format = d.Format;
        img->desc.samples = d.SampleDesc.Count;
        img->desc.sample_quality = d.SampleDesc.Quality;
        img->desc.usage = d.Usage;
        img->desc.bind = d.BindFlags;
        img->desc.cpu_access = d.CPUAccessFlags;
        img->desc.cube = (d.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;
    }
    img->res = res;
    img->refs = 1;
    ID3D11Resource_AddRef(res);
    return img;
}

static void staging_release(void);

/* ---- the device and swap chain -------------------------------------------------------- */

static void release_back_buffer_view(void)
{
    if (g_present_view) {
        ID3D11View_Release(((struct RhiView *)g_present_view)->v);
        free(g_present_view);
        g_present_view = NULL;
    }
    if (g_present_rtv) {
        ID3D11RenderTargetView_Release(g_present_rtv);
        g_present_rtv = NULL;
    }
}

static int make_back_buffer_view(void)
{
    ID3D11Texture2D *bb = NULL;
    HRESULT hr = IDXGISwapChain_GetBuffer(g_swap, 0, &IID_ID3D11Texture2D, (void **)&bb);

    if (FAILED(hr))
        return -1;
    hr = ID3D11Device_CreateRenderTargetView(g_dev, (ID3D11Resource *)bb, NULL, &g_present_rtv);
    ID3D11Texture2D_Release(bb);
    if (FAILED(hr)) {
        g_present_rtv = NULL;
        return -1;
    }
    g_present_view = rhi_d3d11_wrap_view(g_present_rtv, RHI_VIEW_RENDER_TARGET);
    return g_present_view ? 0 : -1;
}

/* The swap chain is the flip model where the system has it: a variable-
 * refresh display can only follow a flip-model swap chain, and only one
 * created to allow tearing can present without waiting for a vblank while
 * it owns its screen. Tried in that order, then the flip model without
 * tearing, then the blit model (DISCARD) it always used. ResizeBuffers must
 * repeat the creation flags. */
#define D3D11_SC_FLAG_ALLOW_TEARING 2048u           /* DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING */
#ifndef DXGI_PRESENT_ALLOW_TEARING
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200UL
#endif
#ifndef DXGI_SWAP_EFFECT_FLIP_DISCARD
#define DXGI_SWAP_EFFECT_FLIP_DISCARD ((DXGI_SWAP_EFFECT)4)
#endif
static UINT g_swap_flags;   /* the swap chain's creation flags */
static int  g_tearing;      /* created with ALLOW_TEARING */
static int  g_vrr;          /* rhi_swapchain_set_vrr */

static HRESULT create_device_and_swap(DXGI_SWAP_CHAIN_DESC *scd, UINT *create_flags,
                                      D3D_FEATURE_LEVEL *feature_level)
{
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                               *create_flags, NULL, 0, D3D11_SDK_VERSION,
                                               scd, &g_swap, &g_dev, feature_level, &g_ctx);

    /* The debug layer is only present with Graphics Tools installed. Without
     * it a Debug build would have no device at all. */
    if (FAILED(hr) && (*create_flags & D3D11_CREATE_DEVICE_DEBUG)) {
        fprintf(stderr, "D3D8: D3D11 debug layer unavailable (0x%08lX), creating without it\n", hr);
        *create_flags &= ~(UINT)D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                           *create_flags, NULL, 0, D3D11_SDK_VERSION,
                                           scd, &g_swap, &g_dev, feature_level, &g_ctx);
    }
    return hr;
}

static int d_device_create(const RhiDeviceDesc *dd)
{
    static const struct { DXGI_SWAP_EFFECT effect; UINT flags; const char *name; } tries[] = {
        { DXGI_SWAP_EFFECT_FLIP_DISCARD, D3D11_SC_FLAG_ALLOW_TEARING,
          "flip model, tearing allowed (variable refresh possible)" },
        { DXGI_SWAP_EFFECT_FLIP_DISCARD, 0, "flip model, no tearing (no variable refresh)" },
        { DXGI_SWAP_EFFECT_DISCARD, 0, "blit model (no variable refresh)" },
    };
    DXGI_SWAP_CHAIN_DESC scd;
    D3D_FEATURE_LEVEL feature_level;
    UINT create_flags = dd->debug ? D3D11_CREATE_DEVICE_DEBUG : 0;
    HRESULT hr = E_FAIL;
    int t;

    memset(&scd, 0, sizeof(scd));
    scd.BufferCount = dd->buffer_count ? dd->buffer_count : 1;
    scd.BufferDesc.Width = dd->width;
    scd.BufferDesc.Height = dd->height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    /* SHADER_INPUT as well: a title that post-processes its own image reads
     * the finished frame back (d3d8_screencopy.c), and that reads this. */
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    scd.OutputWindow = (HWND)dd->window;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = dd->windowed ? TRUE : FALSE;

    for (t = 0; t < (int)(sizeof tries / sizeof tries[0]); t++) {
        UINT buffers = dd->buffer_count ? dd->buffer_count : 1;

        /* The flip model wants two buffers at least, and a window. */
        if (tries[t].effect != DXGI_SWAP_EFFECT_DISCARD) {
            if (!scd.Windowed)
                continue;
            if (buffers < 2)
                buffers = 2;
        }
        scd.BufferCount = buffers;
        scd.SwapEffect = tries[t].effect;
        scd.Flags = tries[t].flags;
        hr = create_device_and_swap(&scd, &create_flags, &feature_level);
        if (SUCCEEDED(hr)) {
            g_swap_flags = tries[t].flags;
            g_tearing = (tries[t].flags & D3D11_SC_FLAG_ALLOW_TEARING) != 0;
            fprintf(stderr, "[RHI] d3d11: %s swap chain\n", tries[t].name);
            break;
        }
    }

    if (FAILED(hr)) {
        fprintf(stderr, "D3D8: Failed to create D3D11 device: 0x%08lX\n", hr);
        g_swap = NULL;
        g_dev = NULL;
        g_ctx = NULL;
        return -1;
    }
    if (make_back_buffer_view() != 0) {
        fprintf(stderr, "D3D8: the swap chain's back buffer has no render-target view\n");
        return -1;
    }
    return 0;
}

static void d_device_destroy(void)
{
    staging_release();
    release_back_buffer_view();
    if (g_swap) { IDXGISwapChain_Release(g_swap); g_swap = NULL; }
    if (g_ctx)  { ID3D11DeviceContext_Release(g_ctx); g_ctx = NULL; }
    if (g_dev)  { ID3D11Device_Release(g_dev); g_dev = NULL; }
}

static int d_device_ready(void)
{
    return g_dev != NULL && g_ctx != NULL;
}

static RhiView *d_swapchain_view(void)
{
    return g_present_view;
}

static int d_swapchain_resize(uint32_t w, uint32_t h)
{
    ID3D11RenderTargetView *saved_rtv = NULL;
    ID3D11DepthStencilView *saved_dsv = NULL;
    int ok = -1;

    if (!g_swap)
        return -1;
    /* ResizeBuffers wants every reference to the old back buffer gone,
     * including any the context still holds. */
    ID3D11DeviceContext_OMGetRenderTargets(g_ctx, 1, &saved_rtv, &saved_dsv);
    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 0, NULL, NULL);
    release_back_buffer_view();
    if (SUCCEEDED(IDXGISwapChain_ResizeBuffers(g_swap, 0, w, h, DXGI_FORMAT_UNKNOWN, g_swap_flags)))
        ok = make_back_buffer_view();
    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &saved_rtv, saved_dsv);
    if (saved_rtv) ID3D11RenderTargetView_Release(saved_rtv);
    if (saved_dsv) ID3D11DepthStencilView_Release(saved_dsv);
    return ok;
}

/* rhi_swapchain_set_vrr: while on, a present that does not wait for a vblank
 * (interval 0) is allowed to tear, which with the window owning its screen
 * is what hands it to a variable-refresh display at once. Only for a swap
 * chain created to allow it; otherwise said once and ignored. */
static void d_swapchain_set_vrr(int on)
{
    static int said;

    on = on ? 1 : 0;
    if (on == g_vrr)
        return;
    g_vrr = on;
    if (on && !g_tearing) {
        if (!said++)
            fprintf(stderr, "[RHI] d3d11: this swap chain cannot present for variable "
                    "refresh (no tearing support); presenting as before\n");
    } else {
        fprintf(stderr, "[RHI] d3d11: variable refresh %s\n", on ? "on" : "off");
    }
    fflush(stderr);
}

static int32_t d_present(uint32_t interval)
{
    UINT flags = (interval == 0 && g_vrr && g_tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    return g_swap ? (int32_t)IDXGISwapChain_Present(g_swap, interval, flags) : -1;
}

static int d_image_readback(RhiImage *img, uint32_t sub, void *dst, uint32_t dst_pitch);

/* The swap chain here is R8G8B8A8, so its rows are already RGBA8. */
static int d_swapchain_readback(void *dst, uint32_t pitch, uint32_t *w, uint32_t *h)
{
    ID3D11Texture2D *bb = NULL;
    RhiImage *img;
    int rc = -1;

    if (!g_swap || FAILED(IDXGISwapChain_GetBuffer(g_swap, 0, &IID_ID3D11Texture2D, (void **)&bb)))
        return -1;
    img = rhi_d3d11_wrap_image(bb);
    ID3D11Texture2D_Release(bb);
    if (!img)
        return -1;
    if (w) *w = img->desc.width;
    if (h) *h = img->desc.height;
    rc = dst ? d_image_readback(img, 0, dst, pitch) : 0;
    rhi_image_destroy(img);                 /* drops the wrapper's reference */
    return rc;
}

/* ---- buffers -------------------------------------------------------------------- */

static RhiBuffer *d_buffer_create(const RhiBufferDesc *d, const void *initial)
{
    D3D11_BUFFER_DESC bd;
    D3D11_SUBRESOURCE_DATA sd;
    RhiBuffer *b;

    memset(&bd, 0, sizeof bd);
    bd.ByteWidth = d->size;
    bd.Usage = (D3D11_USAGE)d->usage;
    bd.BindFlags = d->bind;
    bd.CPUAccessFlags = d->cpu_access;
    memset(&sd, 0, sizeof sd);
    sd.pSysMem = initial;
    b = calloc(1, sizeof *b);
    if (!b)
        return NULL;
    if (FAILED(ID3D11Device_CreateBuffer(g_dev, &bd, initial ? &sd : NULL, &b->b))) {
        free(b);
        return NULL;
    }
    b->desc = *d;
    return b;
}

static void d_buffer_destroy(RhiBuffer *b)
{
    if (b->b) ID3D11Buffer_Release(b->b);
    free(b);
}

static void *d_buffer_map(RhiBuffer *b, uint32_t mode)
{
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)b->b, 0, (D3D11_MAP)mode, 0, &m)))
        return NULL;
    return m.pData;
}

static void d_buffer_unmap(RhiBuffer *b)
{
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)b->b, 0);
}

static void d_buffer_update(RhiBuffer *b, const void *data)
{
    ID3D11DeviceContext_UpdateSubresource(g_ctx, (ID3D11Resource *)b->b, 0, NULL, data, 0, 0);
}

/* ---- images ---------------------------------------------------------------------- */

static int is_block_compressed(RhiFormat f)
{
    return f == RHI_FORMAT_BC1_UNORM || f == RHI_FORMAT_BC2_UNORM ||
           f == RHI_FORMAT_BC3_UNORM || f == RHI_FORMAT_BC5_UNORM;
}

/* A depth image that is also sampled: a shadow map, which the title
 * renders depth into and then reads as a texture. D3D11 refuses a typed
 * depth format with SHADER_RESOURCE, so the resource is made typeless and
 * each view names its own format -- the depth format for the depth view, the
 * matching colour format for the sampled one. */
static DXGI_FORMAT depth_typeless(uint32_t f)
{
    switch (f) {
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
    case DXGI_FORMAT_D16_UNORM:         return DXGI_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT:         return DXGI_FORMAT_R32_TYPELESS;
    default:                            return DXGI_FORMAT_UNKNOWN;
    }
}

static DXGI_FORMAT depth_sampled(uint32_t f)
{
    switch (f) {
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D16_UNORM:         return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D32_FLOAT:         return DXGI_FORMAT_R32_FLOAT;
    default:                            return DXGI_FORMAT_UNKNOWN;
    }
}

static int is_sampled_depth(const RhiImageDesc *d)
{
    return (d->bind & RHI_BIND_DEPTH) && (d->bind & RHI_BIND_SAMPLED) &&
           depth_typeless(d->format) != DXGI_FORMAT_UNKNOWN;
}

static RhiImage *d_image_create(const RhiImageDesc *d, const RhiSubresourceData *initial)
{
    D3D11_SUBRESOURCE_DATA sd[16 * 6];
    uint32_t n = d->mip_levels * (d->type == RHI_IMAGE_3D ? 1 : d->depth), i;
    RhiImage *img = calloc(1, sizeof *img);
    HRESULT hr;

    if (!img)
        return NULL;
    if (initial && n <= sizeof sd / sizeof sd[0]) {
        for (i = 0; i < n; i++) {
            sd[i].pSysMem = initial[i].data;
            sd[i].SysMemPitch = initial[i].row_pitch;
            sd[i].SysMemSlicePitch = initial[i].slice_pitch;
        }
    } else {
        initial = NULL;
    }
    if (d->type == RHI_IMAGE_3D) {
        D3D11_TEXTURE3D_DESC td;
        memset(&td, 0, sizeof td);
        td.Width = d->width;
        td.Height = d->height;
        td.Depth = d->depth;
        td.MipLevels = d->mip_levels;
        td.Format = (DXGI_FORMAT)d->format;
        td.Usage = (D3D11_USAGE)d->usage;
        td.BindFlags = d->bind;
        td.CPUAccessFlags = d->cpu_access;
        hr = ID3D11Device_CreateTexture3D(g_dev, &td, initial ? sd : NULL,
                                          (ID3D11Texture3D **)&img->res);
    } else {
        D3D11_TEXTURE2D_DESC td;
        memset(&td, 0, sizeof td);
        td.Width = d->width;
        td.Height = d->height;
        td.MipLevels = d->mip_levels;
        td.ArraySize = d->depth ? d->depth : 1;
        td.Format = is_sampled_depth(d) ? depth_typeless(d->format) : (DXGI_FORMAT)d->format;
        td.SampleDesc.Count = d->samples ? d->samples : 1;
        td.SampleDesc.Quality = d->sample_quality;
        td.Usage = (D3D11_USAGE)d->usage;
        td.BindFlags = d->bind;
        td.CPUAccessFlags = d->cpu_access;
        td.MiscFlags = d->cube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
        hr = ID3D11Device_CreateTexture2D(g_dev, &td, initial ? sd : NULL,
                                          (ID3D11Texture2D **)&img->res);
    }
    if (FAILED(hr)) {
        free(img);
        return NULL;
    }
    img->desc = *d;
    img->refs = 1;
    return img;
}

static RhiImage *d_image_retain(RhiImage *img)
{
    InterlockedIncrement(&img->refs);
    return img;
}

static void d_image_destroy(RhiImage *img)
{
    if (InterlockedDecrement(&img->refs) > 0)
        return;
    if (img->res) ID3D11Resource_Release(img->res);
    free(img);
}

static int d_sample_count_supported(RhiFormat f, uint32_t samples)
{
    UINT levels = 0;
    return SUCCEEDED(ID3D11Device_CheckMultisampleQualityLevels(g_dev, (DXGI_FORMAT)f,
                                                                samples, &levels)) && levels > 0;
}

static void d_image_get_desc(const RhiImage *img, RhiImageDesc *out)
{
    *out = img->desc;
}

static void d_image_update(RhiImage *img, uint32_t sub, const RhiBox *box,
                           const void *data, uint32_t row_pitch, uint32_t slice_pitch)
{
    if (img->desc.usage == RHI_USAGE_DYNAMIC) {
        /* A dynamic image is rewritten whole, through Map. */
        D3D11_MAPPED_SUBRESOURCE m;
        uint32_t mip = sub % (img->desc.mip_levels ? img->desc.mip_levels : 1);
        uint32_t w = img->desc.width >> mip, h = img->desc.height >> mip, y, row;
        const uint8_t *src = data;

        if (!w) w = 1;
        if (!h) h = 1;
        if (is_block_compressed(img->desc.format))
            h = (h + 3) / 4;
        if (FAILED(ID3D11DeviceContext_Map(g_ctx, img->res, sub, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            return;
        row = row_pitch < m.RowPitch ? row_pitch : m.RowPitch;
        for (y = 0; y < h; y++)
            memcpy((uint8_t *)m.pData + (size_t)y * m.RowPitch, src + (size_t)y * row_pitch, row);
        ID3D11DeviceContext_Unmap(g_ctx, img->res, sub);
        (void)slice_pitch;
        (void)box;
        return;
    }
    ID3D11DeviceContext_UpdateSubresource(g_ctx, img->res, sub, (const D3D11_BOX *)box,
                                          data, row_pitch, slice_pitch);
}

/* One staging texture, kept between readbacks of the same shape: the frame
 * dump reads the same scene every time it fires. */
static struct {
    ID3D11Texture2D *tex;
    D3D11_TEXTURE2D_DESC desc;
} g_staging;

static void staging_release(void)
{
    if (g_staging.tex) ID3D11Texture2D_Release(g_staging.tex);
    g_staging.tex = NULL;
}

static int d_image_readback(RhiImage *img, uint32_t sub, void *dst, uint32_t dst_pitch)
{
    D3D11_TEXTURE2D_DESC src_desc, want;
    ID3D11Texture2D *src;
    ID3D11Texture2D *resolved = NULL;
    D3D11_MAPPED_SUBRESOURCE m;
    uint32_t src_sub = sub, mip, h, y, row;

    if (!img || img->desc.type != RHI_IMAGE_2D)
        return -1;
    src = (ID3D11Texture2D *)img->res;
    ID3D11Texture2D_GetDesc(src, &src_desc);
    mip = sub % src_desc.MipLevels;

    want = src_desc;
    want.Width = src_desc.Width >> mip ? src_desc.Width >> mip : 1;
    want.Height = src_desc.Height >> mip ? src_desc.Height >> mip : 1;
    want.MipLevels = 1;
    want.ArraySize = 1;
    want.SampleDesc.Count = 1;
    want.SampleDesc.Quality = 0;
    want.MiscFlags = 0;

    if (src_desc.SampleDesc.Count > 1) {
        /* A multisampled image is resolved into a plain one first; staging
         * textures cannot be a resolve destination. */
        D3D11_TEXTURE2D_DESC rd = want;
        rd.Usage = D3D11_USAGE_DEFAULT;
        rd.BindFlags = 0;
        rd.CPUAccessFlags = 0;
        if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &rd, NULL, &resolved)))
            return -1;
        ID3D11DeviceContext_ResolveSubresource(g_ctx, (ID3D11Resource *)resolved, 0,
                                               (ID3D11Resource *)src, sub, src_desc.Format);
        src = resolved;
        src_sub = 0;
    }

    want.Usage = D3D11_USAGE_STAGING;
    want.BindFlags = 0;
    want.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (!g_staging.tex || memcmp(&g_staging.desc, &want, sizeof want) != 0) {
        if (g_staging.tex) ID3D11Texture2D_Release(g_staging.tex);
        g_staging.tex = NULL;
        if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &want, NULL, &g_staging.tex))) {
            if (resolved) ID3D11Texture2D_Release(resolved);
            return -1;
        }
        g_staging.desc = want;
    }
    ID3D11DeviceContext_CopySubresourceRegion(g_ctx, (ID3D11Resource *)g_staging.tex, 0, 0, 0, 0,
                                              (ID3D11Resource *)src, src_sub, NULL);
    if (resolved) ID3D11Texture2D_Release(resolved);
    if (FAILED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)g_staging.tex, 0,
                                       D3D11_MAP_READ, 0, &m)))
        return -1;
    h = want.Height;
    if (is_block_compressed(img->desc.format))
        h = (h + 3) / 4;
    row = dst_pitch < m.RowPitch ? dst_pitch : m.RowPitch;
    for (y = 0; y < h; y++)
        memcpy((uint8_t *)dst + (size_t)y * dst_pitch, (uint8_t *)m.pData + (size_t)y * m.RowPitch, row);
    ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)g_staging.tex, 0);
    return 0;
}

/* ---- views ------------------------------------------------------------------------ */

static RhiView *d_view_create(RhiImage *img, uint32_t kind, const RhiViewDesc *d)
{
    RhiView *v = calloc(1, sizeof *v);
    HRESULT hr = E_FAIL;
    int typeless = is_sampled_depth(&img->desc);
    RhiViewDesc whole;

    if (!v)
        return NULL;
    v->kind = kind;
    v->image = img;
    /* A typeless resource has no format of its own for a default view. */
    if (typeless && !d) {
        memset(&whole, 0, sizeof whole);
        whole.format = img->desc.format;
        whole.dim = RHI_VIEW_DIM_2D;
        whole.mip_count = img->desc.mip_levels ? img->desc.mip_levels : 1;
        whole.layer_count = 1;
        d = &whole;
    }
    if (kind == RHI_VIEW_SAMPLED) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd;
        if (d) {
            memset(&sd, 0, sizeof sd);
            sd.Format = typeless ? depth_sampled(img->desc.format) : (DXGI_FORMAT)d->format;
            switch (d->dim) {
            case RHI_VIEW_DIM_CUBE:
                sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                sd.TextureCube.MostDetailedMip = d->base_mip;
                sd.TextureCube.MipLevels = d->mip_count;
                break;
            case RHI_VIEW_DIM_3D:
                sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                sd.Texture3D.MostDetailedMip = d->base_mip;
                sd.Texture3D.MipLevels = d->mip_count;
                break;
            case RHI_VIEW_DIM_2D_ARRAY:
                sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                sd.Texture2DArray.MostDetailedMip = d->base_mip;
                sd.Texture2DArray.MipLevels = d->mip_count;
                sd.Texture2DArray.FirstArraySlice = d->base_layer;
                sd.Texture2DArray.ArraySize = d->layer_count;
                break;
            case RHI_VIEW_DIM_2D_MS:
                sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                break;
            default:
                sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                sd.Texture2D.MostDetailedMip = d->base_mip;
                sd.Texture2D.MipLevels = d->mip_count;
                break;
            }
        }
        hr = ID3D11Device_CreateShaderResourceView(g_dev, img->res, d ? &sd : NULL,
                                                   (ID3D11ShaderResourceView **)&v->v);
    } else if (kind == RHI_VIEW_RENDER_TARGET) {
        D3D11_RENDER_TARGET_VIEW_DESC rd;
        if (d) {
            memset(&rd, 0, sizeof rd);
            rd.Format = (DXGI_FORMAT)d->format;
            switch (d->dim) {
            case RHI_VIEW_DIM_2D_ARRAY:
            case RHI_VIEW_DIM_CUBE:
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rd.Texture2DArray.MipSlice = d->base_mip;
                rd.Texture2DArray.FirstArraySlice = d->base_layer;
                rd.Texture2DArray.ArraySize = d->layer_count;
                break;
            case RHI_VIEW_DIM_2D_MS:
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                break;
            default:
                rd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rd.Texture2D.MipSlice = d->base_mip;
                break;
            }
        }
        hr = ID3D11Device_CreateRenderTargetView(g_dev, img->res, d ? &rd : NULL,
                                                 (ID3D11RenderTargetView **)&v->v);
    } else {
        D3D11_DEPTH_STENCIL_VIEW_DESC dd;
        if (d) {
            memset(&dd, 0, sizeof dd);
            dd.Format = typeless ? (DXGI_FORMAT)img->desc.format : (DXGI_FORMAT)d->format;
            switch (d->dim) {
            case RHI_VIEW_DIM_2D_ARRAY:
            case RHI_VIEW_DIM_CUBE:
                dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                dd.Texture2DArray.MipSlice = d->base_mip;
                dd.Texture2DArray.FirstArraySlice = d->base_layer;
                dd.Texture2DArray.ArraySize = d->layer_count;
                break;
            case RHI_VIEW_DIM_2D_MS:
                dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                break;
            default:
                dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dd.Texture2D.MipSlice = d->base_mip;
                break;
            }
        }
        hr = ID3D11Device_CreateDepthStencilView(g_dev, img->res, d ? &dd : NULL,
                                                 (ID3D11DepthStencilView **)&v->v);
    }
    if (FAILED(hr)) {
        free(v);
        return NULL;
    }
    return v;
}

static void d_view_destroy(RhiView *v)
{
    if (v->v) ID3D11View_Release(v->v);
    free(v);
}

static RhiImage *d_view_image(const RhiView *v)
{
    return v->image;
}

/* ---- shaders and layouts ---------------------------------------------------------------- */

/* Names the compiler and its settings in the disk cache's key: D3DCompile
 * with the flags d_shader_create passes. */
#define D3D11_CACHE_TAG "d3d11-fxc-1"

static RhiShader *d_shader_create(uint32_t stage, const RhiShaderSource *src, char *err, size_t err_len)
{
    D3D_SHADER_MACRO macros[16];
    ID3DBlob *code = NULL, *errors = NULL;
    RhiShader *s;
    HRESULT hr;
    int n = 0;

    if (src->macros) {
        for (; src->macros[n].name && n < 15; n++) {
            macros[n].Name = src->macros[n].name;
            macros[n].Definition = src->macros[n].value;
        }
        macros[n].Name = NULL;
        macros[n].Definition = NULL;
    }
    d3d8_hlsl_note(src->hlsl, src->len, src->name, src->macros, src->entry, src->target);
    if (err && err_len)
        err[0] = 0;
    /* A blob from an earlier run (rhi_shader_cache.c) skips the compiler. */
    {
        void *cached = NULL;
        size_t cached_bytes = 0;

        if (rhi_shader_cache_get(src, D3D11_CACHE_TAG, &cached, &cached_bytes) &&
            SUCCEEDED(D3DCreateBlob(cached_bytes, &code))) {
            memcpy(ID3D10Blob_GetBufferPointer(code), cached, cached_bytes);
            free(cached);
            goto compiled;
        }
        free(cached);
        code = NULL;
    }
    hr = D3DCompile(src->hlsl, (SIZE_T)src->len, src->name, src->macros ? macros : NULL, NULL,
                    src->entry, src->target,
                    src->optimize ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : 0, 0,
                    &code, &errors);
    if (errors) {
        if (err && err_len)
            snprintf(err, err_len, "%s", (const char *)ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    if (FAILED(hr)) {
        if (err && err_len && !err[0])
            snprintf(err, err_len, "D3DCompile failed (0x%08lX)", (unsigned long)hr);
        return NULL;
    }
    rhi_shader_cache_put(src, D3D11_CACHE_TAG, ID3D10Blob_GetBufferPointer(code),
                         ID3D10Blob_GetBufferSize(code));
compiled:
    s = calloc(1, sizeof *s);
    if (!s) {
        ID3D10Blob_Release(code);
        return NULL;
    }
    s->stage = stage;
    if (stage == RHI_STAGE_VERTEX)
        hr = ID3D11Device_CreateVertexShader(g_dev, ID3D10Blob_GetBufferPointer(code),
                                             ID3D10Blob_GetBufferSize(code), NULL,
                                             (ID3D11VertexShader **)&s->sh);
    else
        hr = ID3D11Device_CreatePixelShader(g_dev, ID3D10Blob_GetBufferPointer(code),
                                            ID3D10Blob_GetBufferSize(code), NULL,
                                            (ID3D11PixelShader **)&s->sh);
    if (FAILED(hr)) {
        if (err && err_len)
            snprintf(err, err_len, "Create%sShader failed (0x%08lX)",
                     stage == RHI_STAGE_VERTEX ? "Vertex" : "Pixel", (unsigned long)hr);
        ID3D10Blob_Release(code);
        free(s);
        return NULL;
    }
    /* A vertex shader keeps its bytecode: D3D11 validates an input layout
     * against it. */
    if (stage == RHI_STAGE_VERTEX)
        s->blob = code;
    else
        ID3D10Blob_Release(code);
    return s;
}

static void d_shader_destroy(RhiShader *s)
{
    if (s->sh) ID3D11DeviceChild_Release(s->sh);
    if (s->blob) ID3D10Blob_Release(s->blob);
    free(s);
}

static RhiVertexLayout *d_vertex_layout_create(const RhiVertexElement *e, uint32_t n, const RhiShader *vs)
{
    D3D11_INPUT_ELEMENT_DESC el[32];
    RhiVertexLayout *l;
    uint32_t i;

    if (!vs || !vs->blob || n > 32)
        return NULL;
    for (i = 0; i < n; i++) {
        el[i].SemanticName = e[i].semantic;
        el[i].SemanticIndex = e[i].semantic_index;
        el[i].Format = (DXGI_FORMAT)e[i].format;
        el[i].InputSlot = e[i].slot;
        el[i].AlignedByteOffset = e[i].offset;
        el[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        el[i].InstanceDataStepRate = 0;
    }
    l = calloc(1, sizeof *l);
    if (!l)
        return NULL;
    if (FAILED(ID3D11Device_CreateInputLayout(g_dev, el, n, ID3D10Blob_GetBufferPointer(vs->blob),
                                              ID3D10Blob_GetBufferSize(vs->blob), &l->il))) {
        free(l);
        return NULL;
    }
    return l;
}

static void d_vertex_layout_destroy(RhiVertexLayout *l)
{
    if (l->il) ID3D11InputLayout_Release(l->il);
    free(l);
}

/* ---- state objects --------------------------------------------------------------------------- */

static RhiBlendState *d_blend_state_create(const RhiBlendDesc *d)
{
    D3D11_BLEND_DESC bd;
    RhiBlendState *s = calloc(1, sizeof *s);

    if (!s)
        return NULL;
    memset(&bd, 0, sizeof bd);
    bd.AlphaToCoverageEnable = d->alpha_to_coverage ? TRUE : FALSE;
    bd.RenderTarget[0].BlendEnable = d->enable ? TRUE : FALSE;
    bd.RenderTarget[0].SrcBlend = (D3D11_BLEND)d->src;
    bd.RenderTarget[0].DestBlend = (D3D11_BLEND)d->dst;
    bd.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)d->op;
    bd.RenderTarget[0].SrcBlendAlpha = (D3D11_BLEND)d->src_alpha;
    bd.RenderTarget[0].DestBlendAlpha = (D3D11_BLEND)d->dst_alpha;
    bd.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)d->op_alpha;
    bd.RenderTarget[0].RenderTargetWriteMask = (UINT8)d->write_mask;
    if (FAILED(ID3D11Device_CreateBlendState(g_dev, &bd, &s->s))) {
        free(s);
        return NULL;
    }
    return s;
}

static void stencil_face(D3D11_DEPTH_STENCILOP_DESC *o, const RhiStencilFace *f)
{
    o->StencilFailOp = (D3D11_STENCIL_OP)f->fail;
    o->StencilDepthFailOp = (D3D11_STENCIL_OP)f->depth_fail;
    o->StencilPassOp = (D3D11_STENCIL_OP)f->pass;
    o->StencilFunc = (D3D11_COMPARISON_FUNC)f->func;
}

static RhiDepthState *d_depth_state_create(const RhiDepthDesc *d)
{
    D3D11_DEPTH_STENCIL_DESC dd;
    RhiDepthState *s = calloc(1, sizeof *s);

    if (!s)
        return NULL;
    memset(&dd, 0, sizeof dd);
    dd.DepthEnable = d->depth_enable ? TRUE : FALSE;
    dd.DepthWriteMask = d->depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc = (D3D11_COMPARISON_FUNC)d->depth_func;
    dd.StencilEnable = d->stencil_enable ? TRUE : FALSE;
    dd.StencilReadMask = d->stencil_read_mask;
    dd.StencilWriteMask = d->stencil_write_mask;
    stencil_face(&dd.FrontFace, &d->front);
    stencil_face(&dd.BackFace, &d->back);
    if (FAILED(ID3D11Device_CreateDepthStencilState(g_dev, &dd, &s->s))) {
        free(s);
        return NULL;
    }
    return s;
}

static RhiRasterState *d_raster_state_create(const RhiRasterDesc *d)
{
    D3D11_RASTERIZER_DESC rd;
    RhiRasterState *s = calloc(1, sizeof *s);

    if (!s)
        return NULL;
    memset(&rd, 0, sizeof rd);
    rd.FillMode = (D3D11_FILL_MODE)d->fill;
    rd.CullMode = (D3D11_CULL_MODE)d->cull;
    rd.FrontCounterClockwise = d->front_ccw ? TRUE : FALSE;
    rd.DepthBias = d->depth_bias;
    rd.DepthBiasClamp = d->depth_bias_clamp;
    rd.SlopeScaledDepthBias = d->slope_scaled_depth_bias;
    rd.DepthClipEnable = d->depth_clip ? TRUE : FALSE;
    rd.ScissorEnable = d->scissor ? TRUE : FALSE;
    rd.MultisampleEnable = d->multisample ? TRUE : FALSE;
    rd.AntialiasedLineEnable = d->antialiased_lines ? TRUE : FALSE;
    if (FAILED(ID3D11Device_CreateRasterizerState(g_dev, &rd, &s->s))) {
        free(s);
        return NULL;
    }
    return s;
}

static RhiSampler *d_sampler_create(const RhiSamplerDesc *d)
{
    D3D11_SAMPLER_DESC sd;
    RhiSampler *s = calloc(1, sizeof *s);

    if (!s)
        return NULL;
    memset(&sd, 0, sizeof sd);
    sd.Filter = (D3D11_FILTER)d->filter;
    sd.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)d->address_u;
    sd.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)d->address_v;
    sd.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)d->address_w;
    sd.MipLODBias = d->mip_lod_bias;
    sd.MaxAnisotropy = d->max_anisotropy;
    sd.ComparisonFunc = (D3D11_COMPARISON_FUNC)d->compare;
    memcpy(sd.BorderColor, d->border, sizeof sd.BorderColor);
    sd.MinLOD = d->min_lod;
    sd.MaxLOD = d->max_lod;
    if (FAILED(ID3D11Device_CreateSamplerState(g_dev, &sd, &s->s))) {
        free(s);
        return NULL;
    }
    return s;
}

static void d_blend_state_destroy(RhiBlendState *s)   { if (s->s) ID3D11BlendState_Release(s->s); free(s); }
static void d_depth_state_destroy(RhiDepthState *s)   { if (s->s) ID3D11DepthStencilState_Release(s->s); free(s); }
static void d_raster_state_destroy(RhiRasterState *s) { if (s->s) ID3D11RasterizerState_Release(s->s); free(s); }
static void d_sampler_destroy(RhiSampler *s)          { if (s->s) ID3D11SamplerState_Release(s->s); free(s); }

/* ---- binding and drawing ------------------------------------------------------------------------ */

static void d_set_render_target(RhiView *color, RhiView *depth)
{
    ID3D11RenderTargetView *rtv = color ? (ID3D11RenderTargetView *)color->v : NULL;
    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &rtv,
                                           depth ? (ID3D11DepthStencilView *)depth->v : NULL);
}

static void d_output_save(RhiOutputState *s)
{
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11DepthStencilView *dsv = NULL;
    D3D11_VIEWPORT vp[16];
    UINT n = 16, i;

    ID3D11DeviceContext_RSGetViewports(g_ctx, &n, vp);
    ID3D11DeviceContext_OMGetRenderTargets(g_ctx, 1, &rtv, &dsv);
    s->color = rtv;
    s->depth = dsv;
    s->viewport_count = n;
    for (i = 0; i < n; i++) {
        s->viewports[i].x = vp[i].TopLeftX;
        s->viewports[i].y = vp[i].TopLeftY;
        s->viewports[i].width = vp[i].Width;
        s->viewports[i].height = vp[i].Height;
        s->viewports[i].min_depth = vp[i].MinDepth;
        s->viewports[i].max_depth = vp[i].MaxDepth;
    }
}

static void d_set_viewports(uint32_t n, const RhiViewport *vps)
{
    D3D11_VIEWPORT vp[16];
    uint32_t i;

    if (n > 16) n = 16;
    for (i = 0; i < n; i++) {
        vp[i].TopLeftX = vps[i].x;
        vp[i].TopLeftY = vps[i].y;
        vp[i].Width = vps[i].width;
        vp[i].Height = vps[i].height;
        vp[i].MinDepth = vps[i].min_depth;
        vp[i].MaxDepth = vps[i].max_depth;
    }
    ID3D11DeviceContext_RSSetViewports(g_ctx, n, vp);
}

static void d_output_restore(RhiOutputState *s)
{
    ID3D11RenderTargetView *rtv = (ID3D11RenderTargetView *)s->color;
    ID3D11DepthStencilView *dsv = (ID3D11DepthStencilView *)s->depth;

    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &rtv, dsv);
    if (s->viewport_count)
        d_set_viewports(s->viewport_count, s->viewports);
    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (dsv) ID3D11DepthStencilView_Release(dsv);
    s->color = s->depth = NULL;
}

static int d_output_color_is(const RhiOutputState *s, const RhiView *v)
{
    return v && s->color == (void *)v->v;
}

static void d_set_scissor(const RhiRect *r)
{
    D3D11_RECT rc;
    rc.left = r->left;
    rc.top = r->top;
    rc.right = r->right;
    rc.bottom = r->bottom;
    ID3D11DeviceContext_RSSetScissorRects(g_ctx, 1, &rc);
}

static void d_set_topology(uint32_t t)
{
    ID3D11DeviceContext_IASetPrimitiveTopology(g_ctx, (D3D11_PRIMITIVE_TOPOLOGY)t);
}

static void d_set_vertex_layout(RhiVertexLayout *l)
{
    ID3D11DeviceContext_IASetInputLayout(g_ctx, l ? l->il : NULL);
}

static void d_set_vertex_buffer(uint32_t slot, RhiBuffer *b, uint32_t stride, uint32_t offset)
{
    ID3D11Buffer *nb = b ? b->b : NULL;
    UINT st = stride, off = offset;
    ID3D11DeviceContext_IASetVertexBuffers(g_ctx, slot, 1, &nb, &st, &off);
}

static void d_set_index_buffer(RhiBuffer *b, uint32_t bits, uint32_t offset)
{
    ID3D11DeviceContext_IASetIndexBuffer(g_ctx, b ? b->b : NULL,
                                         bits == 32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
                                         offset);
}

static void d_set_shader(uint32_t stage, RhiShader *s)
{
    if (stage == RHI_STAGE_VERTEX)
        ID3D11DeviceContext_VSSetShader(g_ctx, s ? (ID3D11VertexShader *)s->sh : NULL, NULL, 0);
    else
        ID3D11DeviceContext_PSSetShader(g_ctx, s ? (ID3D11PixelShader *)s->sh : NULL, NULL, 0);
}

static void d_set_uniform_buffers(uint32_t stage, uint32_t slot, uint32_t n, RhiBuffer *const *b)
{
    ID3D11Buffer *nb[14];
    uint32_t i;

    if (n > 14) n = 14;
    for (i = 0; i < n; i++)
        nb[i] = b[i] ? b[i]->b : NULL;
    if (stage == RHI_STAGE_VERTEX)
        ID3D11DeviceContext_VSSetConstantBuffers(g_ctx, slot, n, nb);
    else
        ID3D11DeviceContext_PSSetConstantBuffers(g_ctx, slot, n, nb);
}

static void d_set_textures(uint32_t slot, uint32_t n, RhiView *const *v)
{
    ID3D11ShaderResourceView *srv[16];
    uint32_t i;

    if (n > 16) n = 16;
    for (i = 0; i < n; i++)
        srv[i] = v[i] ? (ID3D11ShaderResourceView *)v[i]->v : NULL;
    ID3D11DeviceContext_PSSetShaderResources(g_ctx, slot, n, srv);
}

static void d_set_samplers(uint32_t slot, uint32_t n, RhiSampler *const *s)
{
    ID3D11SamplerState *ns[16];
    uint32_t i;

    if (n > 16) n = 16;
    for (i = 0; i < n; i++)
        ns[i] = s[i] ? s[i]->s : NULL;
    ID3D11DeviceContext_PSSetSamplers(g_ctx, slot, n, ns);
}

static void d_set_blend_state(RhiBlendState *s, const float *factor, uint32_t mask)
{
    ID3D11DeviceContext_OMSetBlendState(g_ctx, s ? s->s : NULL, factor, mask);
}

static void d_set_depth_state(RhiDepthState *s, uint32_t ref)
{
    ID3D11DeviceContext_OMSetDepthStencilState(g_ctx, s ? s->s : NULL, ref);
}

static void d_set_raster_state(RhiRasterState *s)
{
    ID3D11DeviceContext_RSSetState(g_ctx, s ? s->s : NULL);
}

static void d_draw(uint32_t n, uint32_t first)
{
    ID3D11DeviceContext_Draw(g_ctx, n, first);
}

static void d_draw_indexed(uint32_t n, uint32_t first, int32_t base)
{
    ID3D11DeviceContext_DrawIndexed(g_ctx, n, first, base);
}

static void d_clear_color(RhiView *v, const float *rgba)
{
    if (v)
        ID3D11DeviceContext_ClearRenderTargetView(g_ctx, (ID3D11RenderTargetView *)v->v, rgba);
}

static void d_clear_depth(RhiView *v, uint32_t flags, float z, uint8_t s)
{
    if (v)
        ID3D11DeviceContext_ClearDepthStencilView(g_ctx, (ID3D11DepthStencilView *)v->v, flags, z, s);
}

const RhiBackend rhi_d3d11_backend = {
    "d3d11",
    d_device_create, d_device_destroy, d_device_ready, d_swapchain_view, d_swapchain_resize,
    d_present, d_swapchain_readback, d_swapchain_set_vrr,
    d_buffer_create, d_buffer_destroy, d_buffer_map, d_buffer_unmap, d_buffer_update,
    d_image_create, d_image_retain, d_image_destroy, d_image_get_desc, d_image_update, d_image_readback,
    d_view_create, d_view_destroy, d_view_image,
    d_sample_count_supported,
    d_shader_create, d_shader_destroy, d_vertex_layout_create, d_vertex_layout_destroy,
    d_blend_state_create, d_depth_state_create, d_raster_state_create, d_sampler_create,
    d_blend_state_destroy, d_depth_state_destroy, d_raster_state_destroy, d_sampler_destroy,
    d_set_render_target, d_output_save, d_output_restore, d_output_color_is, d_set_viewports, d_set_scissor,
    d_set_topology, d_set_vertex_layout, d_set_vertex_buffer, d_set_index_buffer, d_set_shader,
    d_set_uniform_buffers, d_set_textures, d_set_samplers, d_set_blend_state, d_set_depth_state,
    d_set_raster_state, d_draw, d_draw_indexed, d_clear_color, d_clear_depth,
};

#endif /* _WIN32 */
