/**
 * D3D8 Render State -> host state objects
 *
 * Converts D3D8 render state values into rhi.h state objects:
 *   - Blend state (alpha blending, color write mask)
 *   - Depth-stencil state (z-test, z-write, stencil)
 *   - Rasterizer state (cull mode, fill mode)
 *   - Sampler state (texture filtering, addressing)
 *
 * State objects are cached and recreated only when dirty.
 */

#include "d3d8_internal.h"
#include "d3d8_display.h"
#include <string.h>
#include <stdio.h>

/* ================================================================
 * Cached state objects
 * ================================================================ */

static RhiBlendState  *g_blend_state = NULL;
static RhiDepthState  *g_ds_state = NULL;
static RhiRasterState *g_raster_state = NULL;
static RhiSampler     *g_sampler_states[4] = { NULL, NULL, NULL, NULL };

/* Last known render state identity for dirty detection */
static DWORD g_last_blend_hash = 0;
static DWORD g_last_raster_hash = 0;
static RhiDepthDesc g_last_ds_desc;

/* ================================================================
 * D3D8 -> host enum translation (rhi.h keeps the D3D11 numbering)
 * ================================================================ */

static uint32_t d3d8_to_rhi_blend(DWORD d3d8blend)
{
    switch (d3d8blend) {
    case D3DBLEND_ZERO:         return RHI_BLEND_ZERO;
    case D3DBLEND_ONE:          return RHI_BLEND_ONE;
    case D3DBLEND_SRCCOLOR:     return RHI_BLEND_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:  return RHI_BLEND_INV_SRC_COLOR;
    case D3DBLEND_SRCALPHA:     return RHI_BLEND_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:  return RHI_BLEND_INV_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:    return RHI_BLEND_DEST_ALPHA;
    case D3DBLEND_INVDESTALPHA: return RHI_BLEND_INV_DEST_ALPHA;
    case D3DBLEND_DESTCOLOR:    return RHI_BLEND_DEST_COLOR;
    case D3DBLEND_INVDESTCOLOR: return RHI_BLEND_INV_DEST_COLOR;
    case D3DBLEND_SRCALPHASAT:  return RHI_BLEND_SRC_ALPHA_SAT;
    default:                    return RHI_BLEND_ONE;
    }
}

/* D3D11 forbids color factors in the alpha channel. Use their alpha components. */
static uint32_t blend_alpha_factor(uint32_t blend)
{
    switch (blend) {
    case RHI_BLEND_SRC_COLOR:      return RHI_BLEND_SRC_ALPHA;
    case RHI_BLEND_INV_SRC_COLOR:  return RHI_BLEND_INV_SRC_ALPHA;
    case RHI_BLEND_DEST_COLOR:     return RHI_BLEND_DEST_ALPHA;
    case RHI_BLEND_INV_DEST_COLOR: return RHI_BLEND_INV_DEST_ALPHA;
    default:                       return blend;
    }
}

static uint32_t d3d8_to_rhi_cmp(DWORD d3d8cmp)
{
    switch (d3d8cmp) {
    case D3DCMP_NEVER:        return RHI_CMP_NEVER;
    case D3DCMP_LESS:         return RHI_CMP_LESS;
    case D3DCMP_EQUAL:        return RHI_CMP_EQUAL;
    case D3DCMP_LESSEQUAL:    return RHI_CMP_LESS_EQUAL;
    case D3DCMP_GREATER:      return RHI_CMP_GREATER;
    case D3DCMP_NOTEQUAL:     return RHI_CMP_NOT_EQUAL;
    case D3DCMP_GREATEREQUAL: return RHI_CMP_GREATER_EQUAL;
    case D3DCMP_ALWAYS:       return RHI_CMP_ALWAYS;
    default:                  return RHI_CMP_LESS_EQUAL;
    }
}

static uint32_t d3d8_to_rhi_stencilop(DWORD op)
{
    switch (op) {
    case 1: return RHI_STENCIL_KEEP;
    case 2: return RHI_STENCIL_ZERO;
    case 3: return RHI_STENCIL_REPLACE;
    case 4: return RHI_STENCIL_INCR_SAT;
    case 5: return RHI_STENCIL_DECR_SAT;
    case 6: return RHI_STENCIL_INVERT;
    case 7: return RHI_STENCIL_INCR;
    case 8: return RHI_STENCIL_DECR;
    default: return RHI_STENCIL_KEEP;
    }
}

static uint32_t d3d8_to_rhi_blendop(DWORD op)
{
    switch (op) {
    case 1: return RHI_BLEND_OP_ADD;
    case 2: return RHI_BLEND_OP_SUBTRACT;
    case 3: return RHI_BLEND_OP_REV_SUBTRACT;
    case 4: return RHI_BLEND_OP_MIN;
    case 5: return RHI_BLEND_OP_MAX;
    default: return RHI_BLEND_OP_ADD;
    }
}

/* Simple hash of relevant render state values for dirty detection */
static DWORD hash_blend_states(const DWORD *rs)
{
    return rs[D3DRS_ALPHABLENDENABLE] ^
           (rs[D3DRS_SRCBLEND] << 4) ^
           (rs[D3DRS_DESTBLEND] << 8) ^
           (rs[D3DRS_BLENDOP] << 12) ^
           (rs[D3DRS_COLORWRITEENABLE] << 16);
}

static DWORD hash_raster_states(const DWORD *rs, BOOL scissor)
{
    return rs[D3DRS_CULLMODE] ^
           (rs[D3DRS_FILLMODE] << 4) ^
           (scissor ? 0x100u : 0u);
}

/* ================================================================
 * State object creation
 * ================================================================ */

static void update_blend_state(const DWORD *rs)
{
    DWORD hash = hash_blend_states(rs);
    RhiBlendDesc bd;

    if (hash == g_last_blend_hash && g_blend_state) return;
    g_last_blend_hash = hash;

    rhi_blend_state_destroy(g_blend_state);
    g_blend_state = NULL;

    memset(&bd, 0, sizeof(bd));
    bd.enable = rs[D3DRS_ALPHABLENDENABLE] ? 1 : 0;
    bd.src = d3d8_to_rhi_blend(rs[D3DRS_SRCBLEND]);
    bd.dst = d3d8_to_rhi_blend(rs[D3DRS_DESTBLEND]);
    bd.op = d3d8_to_rhi_blendop(rs[D3DRS_BLENDOP] ? rs[D3DRS_BLENDOP] : 1);
    bd.src_alpha = blend_alpha_factor(bd.src);
    bd.dst_alpha = blend_alpha_factor(bd.dst);
    bd.op_alpha = bd.op;
    bd.write_mask = rs[D3DRS_COLORWRITEENABLE] & 0x0F;

    g_blend_state = rhi_blend_state_create(&bd);
    if (!g_blend_state)
        fprintf(stderr, "D3D8: CreateBlendState failed\n");
}

static void update_depth_stencil_state(const DWORD *rs)
{
    RhiDepthDesc dsd;

    memset(&dsd, 0, sizeof(dsd));
    dsd.depth_enable = rs[D3DRS_ZENABLE] ? 1 : 0;
    dsd.depth_write = rs[D3DRS_ZWRITEENABLE] ? 1 : 0;
    dsd.depth_func = d3d8_to_rhi_cmp(rs[D3DRS_ZFUNC]);

    dsd.stencil_enable = rs[D3DRS_STENCILENABLE] ? 1 : 0;
    dsd.stencil_read_mask = (uint8_t)(rs[D3DRS_STENCILMASK] & 0xFF);
    dsd.stencil_write_mask = (uint8_t)(rs[D3DRS_STENCILWRITEMASK] & 0xFF);

    dsd.front.func = d3d8_to_rhi_cmp(rs[D3DRS_STENCILFUNC]);
    dsd.front.fail = d3d8_to_rhi_stencilop(rs[D3DRS_STENCILFAIL]);
    dsd.front.depth_fail = d3d8_to_rhi_stencilop(rs[D3DRS_STENCILZFAIL]);
    dsd.front.pass = d3d8_to_rhi_stencilop(rs[D3DRS_STENCILPASS]);
    dsd.back = dsd.front;

    /* Zeroed padding makes the complete descriptor comparable; the reference
     * is bound separately. */
    if (g_ds_state && memcmp(&dsd, &g_last_ds_desc, sizeof(dsd)) == 0) return;
    rhi_depth_state_destroy(g_ds_state);
    g_ds_state = NULL;

    g_ds_state = rhi_depth_state_create(&dsd);
    if (!g_ds_state)
        fprintf(stderr, "D3D8: CreateDepthStencilState failed\n");
    else
        memcpy(&g_last_ds_desc, &dsd, sizeof(dsd));
}

static void update_rasterizer_state(const DWORD *rs, BOOL scissor)
{
    DWORD hash = hash_raster_states(rs, scissor);
    RhiRasterDesc rd;

    if (hash == g_last_raster_hash && g_raster_state) return;
    g_last_raster_hash = hash;

    rhi_raster_state_destroy(g_raster_state);
    g_raster_state = NULL;

    memset(&rd, 0, sizeof(rd));

    switch (rs[D3DRS_FILLMODE]) {
    case D3DFILL_POINT:     rd.fill = RHI_FILL_WIREFRAME; break;  /* D3D11 has no point fill */
    case D3DFILL_WIREFRAME: rd.fill = RHI_FILL_WIREFRAME; break;
    default:                rd.fill = RHI_FILL_SOLID; break;
    }

    /* The one winding inversion, and it belongs to D3D8's semantics, not to
     * any API: D3D8 CW culls what the host convention calls the front face.
     * A backend that flips Y pays for that in its own front-face setting,
     * never here (docs/technical/vulkan-backend.md, section 4.4). */
    switch (rs[D3DRS_CULLMODE]) {
    case D3DCULL_NONE: rd.cull = RHI_CULL_NONE; break;
    case D3DCULL_CW:   rd.cull = RHI_CULL_FRONT; break;  /* D3D8 CW = cull front in D3D11 convention */
    case D3DCULL_CCW:  rd.cull = RHI_CULL_BACK; break;
    default:           rd.cull = RHI_CULL_BACK; break;
    }

    rd.front_ccw = 0;
    rd.depth_clip = 1;
    rd.scissor = scissor ? 1 : 0;
    rd.multisample = 0;
    rd.antialiased_lines = 0;

    g_raster_state = rhi_raster_state_create(&rd);
    if (!g_raster_state)
        fprintf(stderr, "D3D8: CreateRasterizerState failed\n");
}

/* ================================================================
 * Sampler state
 * ================================================================ */

static uint32_t d3d8_to_rhi_address(DWORD mode)
{
    switch (mode) {
    case D3DTADDRESS_WRAP:       return RHI_ADDRESS_WRAP;
    case D3DTADDRESS_MIRROR:     return RHI_ADDRESS_MIRROR;
    case D3DTADDRESS_CLAMP:      return RHI_ADDRESS_CLAMP;
    case D3DTADDRESS_BORDER:     return RHI_ADDRESS_BORDER;
    case D3DTADDRESS_MIRRORONCE: return RHI_ADDRESS_MIRROR_ONCE;
    default:                     return RHI_ADDRESS_WRAP;
    }
}

static uint32_t d3d8_to_rhi_filter(DWORD mag, DWORD min, DWORD mip)
{
    /* Simplified filter mapping */
    BOOL mag_linear = (mag == D3DTEXF_LINEAR || mag == D3DTEXF_ANISOTROPIC);
    BOOL min_linear = (min == D3DTEXF_LINEAR || min == D3DTEXF_ANISOTROPIC);
    BOOL mip_linear = (mip == D3DTEXF_LINEAR);

    if (mag == D3DTEXF_ANISOTROPIC || min == D3DTEXF_ANISOTROPIC)
        return RHI_FILTER_ANISOTROPIC;

    if (min_linear && mag_linear && mip_linear)
        return RHI_FILTER_LINEAR;
    if (min_linear && mag_linear)
        return RHI_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    if (min_linear)
        return RHI_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    if (mag_linear)
        return RHI_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;

    return RHI_FILTER_POINT;
}

/* Sampler objects, kept by descriptor. Every draw applies all four stages,
 * and a title's stages mostly keep their filter and addressing from one
 * draw to the next, so a stage whose descriptor has not changed keeps its
 * object and is not re-bound; a descriptor seen before gets its object
 * back. Creating a sampler takes the device lock, and this used to be four
 * creates and four releases per draw. The cache owns the objects. A bound
 * stage only points at one, so an entry replaced while a stage holds it
 * makes that stage forget what it holds and bind afresh on its next draw:
 * no backend may keep drawing with a sampler that has been destroyed. */
#define SAMPLER_CACHE_SIZE 32

typedef struct SamplerCacheEntry {
    RhiSamplerDesc desc;
    RhiSampler    *state;
} SamplerCacheEntry;

static SamplerCacheEntry g_sampler_cache[SAMPLER_CACHE_SIZE];
static int               g_sampler_cache_count;
static int               g_sampler_cache_next;       /* replacement cursor */
static RhiSamplerDesc    g_sampler_bound_desc[4];    /* what each stage holds */

static RhiSampler *sampler_cache_get(const RhiSamplerDesc *sd)
{
    SamplerCacheEntry *e;
    int i;

    for (i = 0; i < g_sampler_cache_count; i++)
        if (memcmp(&g_sampler_cache[i].desc, sd, sizeof(*sd)) == 0)
            return g_sampler_cache[i].state;

    if (g_sampler_cache_count < SAMPLER_CACHE_SIZE) {
        e = &g_sampler_cache[g_sampler_cache_count++];
    } else {
        e = &g_sampler_cache[g_sampler_cache_next];
        g_sampler_cache_next = (g_sampler_cache_next + 1) % SAMPLER_CACHE_SIZE;
        if (e->state) {
            for (i = 0; i < 4; i++)
                if (g_sampler_states[i] == e->state)
                    g_sampler_states[i] = NULL;
            rhi_sampler_destroy(e->state);
            e->state = NULL;
        }
    }
    e->state = rhi_sampler_create(sd);
    if (!e->state)
        fprintf(stderr, "D3D8: CreateSamplerState failed\n");
    memcpy(&e->desc, sd, sizeof(*sd));
    return e->state;
}

static void sampler_cache_shutdown(void)
{
    int i;
    for (i = 0; i < g_sampler_cache_count; i++) {
        rhi_sampler_destroy(g_sampler_cache[i].state);
        g_sampler_cache[i].state = NULL;
    }
    g_sampler_cache_count = 0;
    g_sampler_cache_next = 0;
    memset(g_sampler_bound_desc, 0, sizeof(g_sampler_bound_desc));
}

void d3d8_states_apply_sampler(DWORD stage)
{
    const DWORD *tss;
    RhiSamplerDesc sd;
    RhiSampler *state;

    if (stage >= 4) return;
    tss = d3d8_GetTSS(stage);
    if (!tss) return;

    memset(&sd, 0, sizeof(sd));
    sd.filter = d3d8_to_rhi_filter(
        tss[D3DTSS_MAGFILTER],
        tss[D3DTSS_MINFILTER],
        tss[D3DTSS_MIPFILTER]);
    sd.address_u = d3d8_to_rhi_address(tss[D3DTSS_ADDRESSU] ? tss[D3DTSS_ADDRESSU] : D3DTADDRESS_WRAP);
    sd.address_v = d3d8_to_rhi_address(tss[D3DTSS_ADDRESSV] ? tss[D3DTSS_ADDRESSV] : D3DTADDRESS_WRAP);
    sd.address_w = RHI_ADDRESS_WRAP;
    sd.max_anisotropy = tss[D3DTSS_MAXANISOTROPY] ? tss[D3DTSS_MAXANISOTROPY] : 1;

    /* Forced anisotropy, and only where the title already minifies
     * linearly. A stage filtering by nearest texel is almost always the
     * 2D layer -- fonts, HUD art, anything authored to land on exact
     * pixels -- and smoothing that blurs it for no gain. Textures on a
     * floor or a wall seen at a glancing angle are what this is for, and
     * they are the ones asking for a linear filter. */
    {
        UINT forced = d3d8_display_policy()->anisotropy;

        if (forced > 1 &&
            (sd.filter == RHI_FILTER_LINEAR ||
             sd.filter == RHI_FILTER_MIN_MAG_LINEAR_MIP_POINT ||
             sd.filter == RHI_FILTER_MIN_LINEAR_MAG_MIP_POINT ||
             sd.filter == RHI_FILTER_ANISOTROPIC)) {
            sd.filter = RHI_FILTER_ANISOTROPIC;
            if (sd.max_anisotropy < forced)
                sd.max_anisotropy = forced;
        }
    }
    sd.compare = RHI_CMP_NEVER;
    sd.max_lod = 3.402823466e+38f;          /* D3D11_FLOAT32_MAX */

    /* Unchanged since it was bound: nothing to do. */
    if (g_sampler_states[stage] &&
        memcmp(&sd, &g_sampler_bound_desc[stage], sizeof(sd)) == 0)
        return;

    state = sampler_cache_get(&sd);
    if (!state) return;

    g_sampler_states[stage] = state;
    memcpy(&g_sampler_bound_desc[stage], &sd, sizeof(sd));
    rhi_set_samplers(stage, 1, &g_sampler_states[stage]);
}

/* ================================================================
 * Apply all states before draw call
 * ================================================================ */

HRESULT d3d8_states_init(void)
{
    /* States are created on first apply */
    return S_OK;
}

void d3d8_states_shutdown(void)
{
    int i;
    rhi_blend_state_destroy(g_blend_state);   g_blend_state = NULL;
    rhi_depth_state_destroy(g_ds_state);      g_ds_state = NULL;
    rhi_raster_state_destroy(g_raster_state); g_raster_state = NULL;
    for (i = 0; i < 4; i++)
        g_sampler_states[i] = NULL;
    sampler_cache_shutdown();
    g_last_blend_hash = 0;
    g_last_raster_hash = 0;
}

void d3d8_states_apply(void)
{
    const DWORD *rs = d3d8_GetRenderStates();
    float blend_factor[4] = { 1, 1, 1, 1 };
    static RhiRect last_scissor;
    static BOOL last_scissor_on;
    RhiRect scissor;
    BOOL scissor_on;

    if (!rs || !rhi_device_ready()) return;

    scissor_on = d3d8_GetScissor(&scissor);
    update_blend_state(rs);
    update_depth_stencil_state(rs);
    update_rasterizer_state(rs, scissor_on);

    if (g_blend_state)
        rhi_set_blend_state(g_blend_state, blend_factor, 0xFFFFFFFF);
    if (g_ds_state)
        rhi_set_depth_state(g_ds_state, rs[D3DRS_STENCILREF]);
    if (g_raster_state)
        rhi_set_raster_state(g_raster_state);
    /* The rectangle only matters while the rasterizer state has scissoring
     * on, and is re-sent only when it changes. */
    if (scissor_on && (!last_scissor_on || memcmp(&scissor, &last_scissor, sizeof scissor) != 0)) {
        rhi_set_scissor(&scissor);
        last_scissor = scissor;
    }
    last_scissor_on = scissor_on;

    /* Apply samplers for all 4 texture stages */
    {
        DWORD s;
        for (s = 0; s < 4; s++)
            d3d8_states_apply_sampler(s);
    }
}
