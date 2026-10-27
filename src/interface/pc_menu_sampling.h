#ifndef TSFP_PC_MENU_SAMPLING_H
#define TSFP_PC_MENU_SAMPLING_H
/* Catmull-Rom reconstruction of the original texels. Only magnification uses
 * this path; mipmapped/minified samples and point-filtered stages stay native.
 * Clamp to the four central texels to suppress ringing at glyph/atlas edges.
 * SampleLevel at texel centres preserves the sampler's wrap/clamp modes. */
#include "graphics/menu_sampling_shader.h"

/* Backend-independent eligibility; adapters only extract texture metadata.
 * A mode of two reconstructs A8 glyph coverage in the scoped row painter. */
static inline float pc_menu_sampling_mode(int menu, int linear, int texture2d,
                                         unsigned usage, int screen_copy, int alpha8)
{
    if(!menu || !linear || !texture2d || usage || screen_copy) return 0.f;
    return menu==2 && alpha8 ? 2.f : 1.f;
}

/* Called by the Windows RHI renderer, after the host has bound its textures.
 * tex_scale.z is spare per-stage constant space in both pixel shader paths. */
#ifdef TSFP_PC_DISPLAY
#include "pc_display.h"
static float pc_menu_filter_stage(unsigned stage)
{
    IDirect3DBaseTexture8 *base=d3d8_GetStageTexture(stage);
    const DWORD *tss=d3d8_GetTSS(stage);
    D3D8Texture *texture;
    if(!base || !tss || IDirect3DBaseTexture8_GetType(base)!=D3DRTYPE_TEXTURE) return 0.f;
    texture=(D3D8Texture *)base;
    D3DFORMAT format=d3d8_base_format(base);
    return pc_menu_sampling_mode(pc_display_menu_filter(),
        tss[D3DTSS_MAGFILTER]==D3DTEXF_LINEAR,1,texture->usage,texture->screen_copy,
        format==D3DFMT_A8 || format==D3DFMT_LIN_A8);
}
#endif
#endif
