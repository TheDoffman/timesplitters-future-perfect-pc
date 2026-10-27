/**
 * D3D8 Compatibility Layer - Internal Header
 *
 * Shared types and declarations for the D3D8 implementation. The host GPU
 * is reached only through rhi.h; nothing here names a graphics API.
 * Not part of the public API - only included by d3d8_*.c files.
 */

#ifndef BURNOUT3_D3D8_INTERNAL_H
#define BURNOUT3_D3D8_INTERNAL_H

#define COBJMACROS
#include "d3d8_xbox.h"

/* Portable: D3D8 device accessor used by recompiled code on both backends
 * (d3d8_device.c on Windows, d3d8_gl.c on POSIX). */
IDirect3DDevice8 *d3d8_GetDevice(void);

#if defined(_WIN32)
/* === Everything below is the rhi.h renderer. The POSIX d3d8_compat
 * library (d3d8_gl.c) implements its own internal state and does not
 * need any of these declarations. === */

#include "rhi.h"

/* ================================================================
 * Device accessors (implemented in d3d8_device.c)
 * ================================================================ */

IDirect3DDevice8    *d3d8_GetDevice(void);
/* The host back buffer's size, which is what clip space maps onto. */
UINT                 d3d8_GetBackBufferWidth(void);
UINT                 d3d8_GetBackBufferHeight(void);
HWND                 d3d8_GetHWND(void);
UINT                 d3d8_GetBackbufferWidth(void);
/* The guest's presentation size (d3d8_device.c); the back buffer's when unset. */
UINT                 d3d8_GetGuestWidth(void);
/* The finished frame as a shader input: the offscreen scene target when
 * the host renders larger than the guest, the swap chain's back buffer
 * when it does not. NULL before the device exists. */
/* The scene as rhi.h objects: the image the title draws into, a view to
 * sample it, and the render-target view "the back buffer" means. */
RhiImage *d3d8_GetSceneImage(void);
RhiView  *d3d8_GetSceneView(void);
RhiView  *d3d8_GetDefaultTargetView(void);
/* Tell the device whether the next draw is positioned in screen
 * coordinates of the title's own making rather than through its
 * projection. Only matters in widescreen, where the two need different
 * horizontal treatment. */
void d3d8_SetTwoDSqueeze(BOOL on);

/* Runtime shader compiles, timed (d3d8_compile.c). kind: 0 combiner pixel
 * shader, 1 fixed-function pixel shader, 2 vertex program. Reported every
 * few seconds with the count and the milliseconds they took, because a
 * compile inside a frame is a stall the frame rate shows and the profile
 * attributes to D3DCOMPILER_47. */
long long d3d8_compile_clock(void);
void      d3d8_compile_note(int kind, long long started);
/* Every backend calls this with each HLSL source before compiling it
 * (d3d8_compile.c): RECOMP_D3D8_HLSL_DIR=<dir> writes each distinct one. */
void      d3d8_hlsl_note(const char *src, size_t len, const char *name,
                         const RhiMacro *macros, const char *entry, const char *target);
BOOL d3d8_GetTwoDSqueeze(void);
/* Whether a screen-space draw spans the widescreen picture rather than
 * being squeezed to 4:3 with the HUD: a backdrop or a fade, by its own
 * extent under the scissor (and, with RECOMP_WIDESCREEN_2D=centre, only a
 * whole-screen pass). */
BOOL d3d8_draw_escapes_squeeze(const void *vertices, UINT stride, UINT count);
/* Once per draw after the shader path: place a screen-space draw in
 * widescreen, by the title's placement (xbox_D3D8SetTwoDPlacement) or else
 * by d3d8_draw_escapes_squeeze. */
void d3d8_place_2d_draw(const void *vertices, UINT stride, UINT count);
/* The scissor rectangle to draw with, if one is on (xbox_D3D8SetScissors). */
BOOL                 d3d8_GetScissor(RhiRect *out);
UINT                 d3d8_GetGuestHeight(void);
UINT                 d3d8_GetBackbufferHeight(void);

/* Current render state array accessor */
const DWORD         *d3d8_GetRenderStates(void);
const DWORD         *d3d8_GetTSS(DWORD stage);

/* Base texture currently bound to a texture stage (2D/cube/volume),
 * or NULL if nothing is bound. */
IDirect3DBaseTexture8 *d3d8_GetStageTexture(DWORD stage);

/* Transform accessors */
const D3DMATRIX     *d3d8_GetTransform(D3DTRANSFORMSTATETYPE type);

/* Lighting accessors (d3d8_device.c) */
const D3DLIGHT8     *d3d8_GetLight(DWORD index);
BOOL                 d3d8_GetLightEnable(DWORD index);
const D3DMATERIAL8  *d3d8_GetMaterial(void);
UINT                 d3d8_GetNumLights(void);
DWORD                d3d8_GetCurrentFVF(void);

/* How many vertices DrawPrimitiveUP reads from the caller, and how many
 * indices DrawIndexedPrimitiveUP reads, for a primitive type and count; 0 for
 * a type the draw refuses. Kept beside the draws (d3d8_device.c) so frame
 * capture (src/hle/hle_d3d8_record.c) copies exactly the bytes the host read. */
UINT                 d3d8_up_vertices_read(D3DPRIMITIVETYPE type, UINT prims);
UINT                 d3d8_up_indices_read(D3DPRIMITIVETYPE type, UINT prims);

/* A 2D texture as the host holds it, for frame capture. Both return FALSE
 * for anything that is not a D3D8Texture (cube, volume, surface). The level
 * bytes are the texture's own system-memory copy, in the Xbox packing the
 * upload path takes (d3d8_resources.c, tex_LockRect); valid until the
 * texture is released or locked for writing. */
typedef struct D3D8TextureInfo {
    D3DFORMAT format;
    UINT      width, height, levels;
    DWORD     usage;
} D3D8TextureInfo;
BOOL d3d8_texture_info(IDirect3DBaseTexture8 *texture, D3D8TextureInfo *info);

/* A cube texture's shape, for frame capture. TRUE only for a cube, so this
 * doubles as the type test against d3d8_texture_info. */
typedef struct D3D8CubeInfo {
    D3DFORMAT format;
    UINT      edge, levels;
    DWORD     usage;
} D3D8CubeInfo;
BOOL d3d8_cube_info(IDirect3DBaseTexture8 *texture, D3D8CubeInfo *info);
BOOL d3d8_texture_level(IDirect3DBaseTexture8 *texture, UINT level,
                        const BYTE **bits, UINT *pitch, UINT *rows);
/* One face and level of a cube's system-memory copy, the bytes its LockRect
 * hands out and its upload reads (Xbox packing). FALSE for anything that is
 * not a cube. For a cube the title renders into the copy is never written,
 * since those texels live on the GPU. */
BOOL d3d8_cube_level(IDirect3DBaseTexture8 *texture, UINT face, UINT level,
                     const BYTE **bits, UINT *pitch, UINT *rows);

/* ================================================================
 * Resource wrapper structures
 * ================================================================ */

typedef struct D3D8VertexBuffer {
    IDirect3DVertexBuffer8  iface;      /* COM interface (must be first) */
    LONG                    ref_count;
    RhiBuffer              *buffer;
    UINT                    size;
    DWORD                   fvf;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* System memory for Lock */
    BOOL                    locked;
    BOOL                    dirty;
} D3D8VertexBuffer;

typedef struct D3D8IndexBuffer {
    IDirect3DIndexBuffer8   iface;
    LONG                    ref_count;
    RhiBuffer              *buffer;
    UINT                    size;
    D3DFORMAT               format;     /* INDEX16 or INDEX32 */
    DWORD                   usage;
    BYTE                   *sys_mem;
    BOOL                    locked;
    BOOL                    dirty;
} D3D8IndexBuffer;

typedef struct D3D8Texture {
    IDirect3DTexture8       iface;
    LONG                    ref_count;
    RhiImage               *image;
    RhiView                *srv;
    UINT                    width;
    UINT                    height;
    UINT                    levels;
    D3DFORMAT               d3d8_format;
    RhiFormat               host_format;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* All mip levels, back to back (level 0 first) */
    UINT                    pitch;      /* Row pitch of level 0 */
    BOOL                    locked;
    BOOL                    dirty;
    UINT                    palette;    /* Palette index (texture stage) this P8 texture bakes */
    BOOL                    screen_copy; /* the frame was copied into it (d3d8_screencopy.c) */
} D3D8Texture;

typedef struct D3D8Surface {
    IDirect3DSurface8       iface;
    LONG                    ref_count;
    RhiImage               *image;      /* held; often a parent texture's */
    RhiView                *rtv;
    RhiView                *dsv;
    UINT                    width;
    UINT                    height;
    D3DFORMAT               format;
    D3DPOOL                 pool;
    DWORD                   usage;

    /* Subresource of `image` this surface aliases (array*MipLevels+mip).
     * 0 for a standalone offscreen surface. */
    UINT                    subresource;

    /* Multisample state (requested Xbox type + resolved host count). */
    D3DMULTISAMPLE_TYPE     multsample_type;
    UINT                    sample_count;

    /* Surface LockRect: the locked region, read back into CPU memory
     * (rhi_image_readback) and written back on unlock unless read-only. */
    BYTE                   *locked_bits;
    INT                     locked_pitch;
    UINT                    lock_x;
    UINT                    lock_y;
    UINT                    lock_w;
    UINT                    lock_h;
    BOOL                    locked;
    BOOL                    lock_readonly;

    /* P8 (palettized) surfaces: raw index data lives in the parent
     * texture's sys_mem. LockRect returns the indices directly instead
     * of the palette-expanded BGRA that the host image holds. */
    BOOL                    palettized;
    const BYTE             *palette_sys;  /* raw level data (1 byte/texel) */
    UINT                    palette_pitch;
    UINT                    palette_index; /* stage palette baked into it */
} D3D8Surface;

/* 2D texture. See tex_* implementation.
 *
 * D3D8CubeTexture/D3D8VolumeTexture intentionally mirror the field
 * layout of D3D8Texture up to and including the `srv` member (a
 * "layout overlay"), so dev_SetTexture can fetch the SRV of any
 * bound base texture through the D3D8Texture offset. */
typedef struct D3D8CubeTexture {
    IDirect3DCubeTexture8   iface;
    LONG                    ref_count;
    RhiImage               *image;
    RhiView                *srv;
    UINT                    width;      /* edge length */
    UINT                    height;     /* == width (cube faces are square) */
    UINT                    levels;
    D3DFORMAT               d3d8_format;
    RhiFormat               host_format;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* 6 faces * mip chain per face */
    UINT                    pitch;      /* row pitch of face level 0 */
    BOOL                    locked;
    BOOL                    dirty;
    UINT                    palette;    /* Palette index (texture stage) this P8 texture bakes */
} D3D8CubeTexture;

/* 3D texture. The leading fields mirror D3D8CubeTexture
 * up to `srv` (layout overlay) when read through the D3D8Texture prefix.
 * Note `depth` is placed AFTER `levels` so width/height/levels stay at the
 * same offsets as the 2D types. */
typedef struct D3D8VolumeTexture {
    IDirect3DVolumeTexture8 iface;
    LONG                    ref_count;
    RhiImage               *image;
    RhiView                *srv;
    UINT                    width;
    UINT                    height;
    UINT                    levels;
    UINT                    depth;
    D3DFORMAT               d3d8_format;
    RhiFormat               host_format;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* all levels, back to back */
    UINT                    pitch;      /* row pitch of level 0 */
    BOOL                    locked;
    BOOL                    dirty;
    UINT                    palette;    /* Palette index this P8 texture bakes */
} D3D8VolumeTexture;

/* A single volume level handed out by GetVolumeLevel(). */
typedef struct D3D8Volume {
    IDirect3DVolume8        iface;
    LONG                    ref_count;
    IDirect3DVolumeTexture8 *parent;    /* owning texture (AddRef'd) */
    UINT                    level;
} D3D8Volume;

/* ================================================================
 * Format conversion (d3d8_resources.c)
 * ================================================================ */

/* The host format an Xbox format is held in: an RhiFormat, which is the
 * DXGI number (so the name is historical). */
RhiFormat   d3d8_to_dxgi_format(D3DFORMAT fmt);
UINT        d3d8_format_bpp(D3DFORMAT fmt);
BOOL        d3d8_format_is_compressed(D3DFORMAT fmt);
UINT        d3d8_row_pitch(D3DFORMAT fmt, UINT width);

/* Is the Xbox format a depth/stencil format? */
BOOL d3d8_format_is_depth(D3DFORMAT fmt);
/* d3d8_device.c: draws go to the screen, not a render-target texture. */
BOOL d3d8_target_is_screen(void);

/* bpp of the data actually uploaded to D3D11 (post-conversion). */
UINT d3d8_upload_bpp(D3DFORMAT fmt);

/* Does this format need software conversion at upload time? */
BOOL d3d8_format_has_conversion(D3DFORMAT fmt);

/* Is this a palettized (P8) format? */
BOOL d3d8_format_is_palettized(D3DFORMAT fmt);

/* Convert one linear (unswizzled) row of pixels from the Xbox layout
 * to the layout expected by the D3D11 texture. dst holds
 * d3d8_upload_bpp()/8 * width bytes per row. src is the raw
 * d3d8_format_bpp()/8 * width pitch. `palette` is the palette index
 * (texture stage) P8 texels are expanded through. */
void d3d8_convert_linear_pixels(D3DFORMAT fmt, UINT width, UINT height,
                                const BYTE *src, BYTE *dst, UINT palette);

/* Surface implementation (d3d8_resources.c). The surface takes its own
 * reference to `image`. */
IDirect3DSurface8 *d3d8_surface_create(RhiImage *image,
                                       UINT mip_slice,
                                       UINT array_slice,
                                       UINT width, UINT height,
                                       D3DFORMAT fmt, D3DPOOL pool,
                                       DWORD usage,
                                       D3DMULTISAMPLE_TYPE multsample_type,
                                       const BYTE *raw_level_data,
                                       UINT palette_index);

HRESULT d3d8_CreateImageSurfaceImpl(UINT Width, UINT Height, D3DFORMAT Format,
                                    IDirect3DSurface8 **ppSurface);

/* Fetch the sampled view of any bound base texture (2D, cube or
 * volume). All three implementations keep it at the same offset as
 * D3D8Texture, and the image likewise. */
RhiView  *d3d8_base_srv(IDirect3DBaseTexture8 *texture);
RhiImage *d3d8_base_resource(IDirect3DBaseTexture8 *texture);

/* Read the D3DFORMAT of any base texture. */
D3DFORMAT d3d8_base_format(IDirect3DBaseTexture8 *texture);
BOOL d3d8_format_is_linear(D3DFORMAT fmt);
BOOL d3d8_base_size(IDirect3DBaseTexture8 *texture, UINT *width, UINT *height);
void      d3d8_base_set_palette(IDirect3DBaseTexture8 *texture, UINT palette);

/* Re-upload every level of a P8 (palettized) texture through its
 * current stage palette. No-op for non-palettized textures. */
void d3d8_refresh_palette(IDirect3DBaseTexture8 *texture);

/* Cube/volume texture implementations (d3d8_resources.c) */
HRESULT d3d8_CreateCubeTextureImpl(UINT EdgeLength, UINT Levels, DWORD Usage,
                                   D3DFORMAT Format, IDirect3DCubeTexture8 **ppTex);
HRESULT d3d8_CreateVolumeTextureImpl(UINT Width, UINT Height, UINT Depth,
                                     UINT Levels, DWORD Usage, D3DFORMAT Format,
                                     IDirect3DVolumeTexture8 **ppTex);

/* ================================================================
 * Palette management (d3d8_device.c)
 * ================================================================ */

#define D3D8_MAX_PALETTES 4
void            d3d8_SetPalette(DWORD stage, const DWORD *entries);
const DWORD    *d3d8_GetPalette(DWORD stage);

/* Resource creation (d3d8_resources.c) */
HRESULT d3d8_CreateVertexBufferImpl(UINT Length, DWORD Usage, DWORD FVF, IDirect3DVertexBuffer8 **ppVB);
HRESULT d3d8_CreateIndexBufferImpl(UINT Length, DWORD Usage, D3DFORMAT Format, IDirect3DIndexBuffer8 **ppIB);
HRESULT d3d8_CreateTextureImpl(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, IDirect3DTexture8 **ppTex);

/* ================================================================
 * Shader management (d3d8_shaders.c)
 * ================================================================ */

HRESULT d3d8_shaders_init(void);
void    d3d8_shaders_shutdown(void);

/* Select programmable/FVF vertex state and refresh fixed-function pixel state. */
void    d3d8_shaders_prepare_draw(DWORD handle);

/* ================================================================
 * NV2A Register Combiner pixel shaders (d3d8_combiners.c)
 * ================================================================ */

#include "d3d8_combiners.h"

/* ================================================================
 * NV2A Programmable Vertex Shaders (d3d8_vsh.c)
 * ================================================================ */

#include "d3d8_vsh.h"

/* ================================================================
 * Render state management (d3d8_states.c)
 * ================================================================ */

HRESULT d3d8_states_init(void);
void    d3d8_states_shutdown(void);

/* Apply current D3D8 render states as D3D11 state objects */
void    d3d8_states_apply(void);

/* Create sampler state from TSS and apply to slot */
void    d3d8_states_apply_sampler(DWORD stage);

#endif /* _WIN32 -- end of D3D11 backend section */

#endif /* BURNOUT3_D3D8_INTERNAL_H */
