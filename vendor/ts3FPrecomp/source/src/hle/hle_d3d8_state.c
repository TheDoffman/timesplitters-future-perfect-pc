/*
 * hle_d3d8_state.c -- the title's render and texture stage states, forwarded
 * to the shadow device before each draw.
 *
 * Part of shadow mode (hle_d3d8.c, RECOMP_HLE_D3D8=shadow).
 *
 * Xbox D3D8 inlines most SetRenderState and SetTextureStageState calls: the
 * title writes the value into D3D8's own state arrays and nothing is called
 * that a replacement could see. So, as Cxbx-Reloaded does (RenderStates.cpp,
 * TextureStates.cpp), the arrays themselves are read when a draw reaches the
 * host, and every state that changed since the last draw is converted and set.
 * The arrays are found by name through the title's XDK symbols:
 *   D3D_g_RenderState           render states, from X_D3DRS_PSALPHAINPUTS0;
 *   D3D_g_DeferredRenderState   the same array from X_D3DRS_FOGENABLE;
 *   D3D_g_ComplexRenderState    the same array from X_D3DRS_PSTEXTUREMODES;
 *   D3D_g_DeferredTextureState  4 stages x 32 texture stage states.
 *
 * Which slot holds which state changed until XDK 4627 (Cxbx-Reloaded,
 * DxbxRenderStateInfo in XbConvert.cpp). States are numbered here as in the
 * final layout: deferred states start at 92 and complex states at 136, and
 * the one state removed along the way, D3DRS_MULTISAMPLETYPE (154), leaves
 * every later state one slot down. Two older layouts are known as well (see
 * enum rs_layout). The distances between the named arrays say which layout a
 * title has; for any other, nothing is forwarded and the run log says why.
 * Texture stage states use the order from XDK 4039 on, except in the oldest
 * layout.
 *
 * One gap in "the title writes the arrays": the XDK's own
 * D3DDevice_SetRenderState_Simple only writes the push buffer. Burnout 2
 * reaches it through SetRenderStateNotInline, which stores the value after;
 * a title that calls it directly would leave the simple states (57-91) stale
 * here, and Cxbx-Reloaded replaces that function to store them.
 *
 * Values are Xbox (OpenGL-style) enumerations; the host takes the PC's
 * (src/d3d/d3d8_xbox.h and the converters in d3d8_states.c). The tables
 * below are Cxbx-Reloaded's EmuXB2PC_* conversions (XbConvert.h). A value
 * with no host equivalent is replaced by the nearest one and logged once.
 *
 * Pixel shader states (0-56, and PSTextureModes at 136) are forwarded to the
 * host's register combiners; RECOMP_HLE_D3D8_PS=0 turns that off and leaves
 * the host on its fixed-function pixel path. The setup comes from the shader
 * object D3DDevice_SetPixelShader selects, not from the render states.
 *
 * Forwarded but not yet used by the host: SHADEMODE, DITHERENABLE,
 * COLORVERTEX, NORMALIZENORMALS, RESULTARG, BUMPENVMAT*, MIPMAPLODBIAS,
 * MAXMIPLEVEL, BORDERCOLOR. Not forwarded: WRAP0-3, VERTEXBLEND, LOCALVIEWER, ZBIAS,
 * EDGEANTIALIAS, BLENDCOLOR, the back-face material and two-sided lighting
 * states, point sprite, material source and multisample states, and the
 * texture stage states ADDRESSW, TEXTURETRANSFORMFLAGS, BUMPENVLSCALE/LOFFSET,
 * COLORKEYOP, COLORSIGN, ALPHAKILL, COLORKEYCOLOR, COLORARG0 (its host number
 * is ALPHAKILL's) and ALPHAARG0.
 */
#include "platform/xbox_winnt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hle.h"

HLE_IMPORT_VAR(D3D_g_RenderState);
HLE_IMPORT_VAR(D3D_g_DeferredRenderState);
HLE_IMPORT_VAR(D3D_g_ComplexRenderState);
HLE_IMPORT_VAR(D3D_g_DeferredTextureState);

#ifdef _WIN32
#include "d3d8_xbox.h"
#include "hle_d3d8_record.h"

#define XRS_COUNT        167         /* X_D3DRS_DONOTCULLUNCOMPRESSED + 1 */
#define XRS_REMOVED      154         /* X_D3DRS_MULTISAMPLETYPE */
#define XRS_FOGENABLE    92
#define XRS_COMPLEX      136
#define STAGES           4
#define STAGE_SIZE       32

static int      g_state_ready = -1;  /* -1 not checked, 0 unusable, 1 ready */
/* Which of the known render-state layouts the title's arrays have.
 *
 * LAYOUT_PRE4034: XDK builds before 4034 (3424-3944; Halo and Max Payne are
 * 3925). XbSymbolDatabase's per-version table (DxbxRenderStateInfo, in
 * third_party/XbSymbolDatabase/src/lib/manual_d3d8__ltcg.h) says which states
 * such a build lacks: DEPTHCLIPCONTROL (4432), STIPPLEENABLE and the eight
 * simple and eight deferred unused slots (4627), SWAPFILTER (4034),
 * PRESENTATIONINTERVAL (4627), MULTISAMPLEMODE and MULTISAMPLERENDERTARGETMODE
 * (4034) and SAMPLEALPHA (4627); MULTISAMPLETYPE is still there (removed in
 * 4034). That puts the deferred block at slot 82 and the complex block at 116,
 * which is exactly where such a title's XDK symbols put them, and it is the
 * enumeration Halo's own debug information records. The texture stage states
 * of those builds come in an older order too: the eight operation and argument
 * states, RESULTARG and TEXTURETRANSFORMFLAGS first (0-9), then ADDRESSU to
 * ALPHAKILL (10-21); from 22 on the order is the same.
 *
 * LAYOUT_4034: XDK 4034 to 4431 (Hunter: The Reckoning is 4361). From the
 * same table: SWAPFILTER, MULTISAMPLEMODE and MULTISAMPLERENDERTARGETMODE
 * arrive in 4034 and MULTISAMPLETYPE leaves; DEPTHCLIPCONTROL (4432) and
 * STIPPLEENABLE, the unused slots, PRESENTATIONINTERVAL and SAMPLEALPHA
 * (4627) are not there yet. That puts the deferred block at 82 and the complex
 * block at 117, where Hunter's XDK symbols put them (and the database's own
 * check of its table against the arrays it found passes for 4361). The texture
 * stage states are in the 4039+ order already: Cxbx-Reloaded reorders them
 * only for D3D8 3948 and older, and Hunter's stage arrays read as valid
 * values in that order (see the note with this change).
 *
 * LAYOUT_4627: the final layout, deferred at 92 and complex at 136. */
enum rs_layout { LAYOUT_4627, LAYOUT_PRE4034, LAYOUT_4034 };
static enum rs_layout g_layout = LAYOUT_4627;
static uint32_t g_prev_rs[XRS_COUNT];
static uint32_t g_prev_tss[STAGES][STAGE_SIZE];
static int      g_prev_valid;

/* Each value is logged once. Texture operations are converted on every draw,
 * so logging each occurrence would bury every other value under one. */
static void unmapped(const char *what, uint32_t value)
{
    static struct { const char *what; uint32_t value; } seen[32];
    static int nseen;
    int i;

    for (i = 0; i < nseen; i++)
        if (seen[i].value == value && strcmp(seen[i].what, what) == 0)
            return;
    if (nseen < (int)(sizeof seen / sizeof seen[0])) {
        seen[nseen].what = what;
        seen[nseen].value = value;
        nseen++;
        fprintf(stderr, "[HLE-D3D8] shadow state: %s value 0x%X has no host "
                "equivalent, nearest used\n", what, value);
    }
}

static int state_ready(void)
{
    uint32_t rs = hle_var_D3D_g_RenderState;

    if (g_state_ready >= 0)
        return g_state_ready;
    g_state_ready = 0;
    /* Both block starts are needed: XDKs before 4627 added the simple and
     * deferred unused slots separately, so the deferred start alone does not
     * pin where the complex states are. */
    if (!rs || !hle_var_D3D_g_DeferredRenderState || !hle_var_D3D_g_ComplexRenderState ||
        !hle_var_D3D_g_DeferredTextureState) {
        fprintf(stderr, "[HLE-D3D8] shadow states off: this title's XDK symbols do not "
                "name all of D3D_g_RenderState, D3D_g_DeferredRenderState, "
                "D3D_g_ComplexRenderState and D3D_g_DeferredTextureState\n");
        return 0;
    }
    if (hle_var_D3D_g_DeferredRenderState - rs == 4 * 82 &&
        hle_var_D3D_g_ComplexRenderState - rs == 4 * 116) {
        g_layout = LAYOUT_PRE4034;
    } else if (hle_var_D3D_g_DeferredRenderState - rs == 4 * 82 &&
               hle_var_D3D_g_ComplexRenderState - rs == 4 * 117) {
        g_layout = LAYOUT_4034;
    } else if (hle_var_D3D_g_DeferredRenderState - rs != 4 * XRS_FOGENABLE ||
        hle_var_D3D_g_ComplexRenderState - rs != 4 * XRS_COMPLEX) {
        fprintf(stderr, "[HLE-D3D8] shadow states off: deferred states at slot %d and "
                "complex at %d, where the XDK 4627+ layout has %d and %d, the "
                "4034-4431 one 82 and 117 and the pre-4034 one 82 and 116\n",
                (int)(hle_var_D3D_g_DeferredRenderState - rs) / 4,
                (int)(hle_var_D3D_g_ComplexRenderState - rs) / 4,
                XRS_FOGENABLE, XRS_COMPLEX);
        return 0;
    }
    fprintf(stderr, "[HLE-D3D8] shadow states: render states at 0x%08X, texture "
            "stage states at 0x%08X (%s layout)\n",
            rs, hle_var_D3D_g_DeferredTextureState,
            g_layout == LAYOUT_PRE4034 ? "pre-4034 XDK" :
            g_layout == LAYOUT_4034 ? "XDK 4034-4431" : "XDK 4627+");
    g_state_ready = 1;
    return 1;
}

/* Where a state (numbered as in the 4627+ enumeration, MULTISAMPLETYPE
 * included) sits in this title's array, or -1 when its XDK has no such state.
 * See g_layout for the older layouts. */
static int rs_slot(uint32_t state)
{
    if (g_layout == LAYOUT_4627)
        return state > XRS_REMOVED ? (int)state - 1 : (int)state;
    if (g_layout == LAYOUT_4034) {
        if (state <= 81)  return (int)state;      /* pixel shader and simple */
        if (state <= 91)  return -1;              /* DEPTHCLIPCONTROL..unused */
        if (state <= 126) return (int)state - 10; /* FOGENABLE..SWAPFILTER */
        if (state <= 135) return -1;              /* PRESENTATIONINTERVAL..unused */
        if (state <= 153) return (int)state - 19; /* PSTEXTUREMODES..MULTISAMPLEMASK */
        if (state == 154) return -1;              /* MULTISAMPLETYPE */
        if (state <= 158) return (int)state - 20; /* MULTISAMPLEMODE..LINEWIDTH */
        if (state == 159) return -1;              /* SAMPLEALPHA */
        if (state <  XRS_COUNT) return (int)state - 21; /* DXT1NOISEENABLE.. */
        return -1;
    }
    if (state <= 81)  return (int)state;          /* pixel shader and simple */
    if (state <= 91)  return -1;                  /* DEPTHCLIPCONTROL..unused */
    if (state <= 125) return (int)state - 10;     /* FOGENABLE..PATCHSEGMENTS */
    if (state <= 135) return -1;                  /* SWAPFILTER..unused */
    if (state <= 154) return (int)state - 20;     /* PSTEXTUREMODES..MULTISAMPLETYPE */
    if (state <= 156) return -1;                  /* MULTISAMPLEMODE, ..RTMODE */
    if (state <= 158) return (int)state - 22;     /* SHADOWFUNC, LINEWIDTH */
    if (state == 159) return -1;                  /* SAMPLEALPHA */
    if (state <  XRS_COUNT) return (int)state - 23; /* DXT1NOISEENABLE.. */
    return -1;
}

static uint32_t guest_rs(uint32_t state)
{
    int slot = rs_slot(state);
    return slot < 0 ? 0u : HLE_MEM32(hle_var_D3D_g_RenderState + 4u * (uint32_t)slot);
}

/* A texture stage state's slot (4039+ numbering, as g_ts_map has them) in
 * this title's per-stage array. */
static uint32_t ts_slot(uint32_t x)
{
    if (g_layout != LAYOUT_PRE4034)
        return x;
    if (x <= 11) return x + 10;     /* ADDRESSU..ALPHAKILL */
    if (x <= 21) return x - 12;     /* COLOROP..TEXTURETRANSFORMFLAGS */
    return x;
}

/* ------------------------------------------------------------ conversions */

static DWORD cmp_to_host(uint32_t v)
{
    if (v >= 0x200 && v <= 0x207)
        return D3DCMP_NEVER + (v - 0x200);   /* NEVER..ALWAYS, same order */
    unmapped("D3DCMPFUNC", v);
    return D3DCMP_ALWAYS;
}

static DWORD blend_to_host(uint32_t v)
{
    if (v == 0)
        return D3DBLEND_ZERO;
    if (v == 1)
        return D3DBLEND_ONE;
    if (v >= 0x300 && v <= 0x308)
        return D3DBLEND_SRCCOLOR + (v - 0x300);  /* SRCCOLOR..SRCALPHASAT */
    unmapped("D3DBLEND", v);                 /* the constant-colour factors */
    return D3DBLEND_ONE;
}

static DWORD blendop_to_host(uint32_t v)
{
    switch (v) {                             /* d3d8_to_d3d11_blendop: 1..5 */
    case 0x8006: return 1;                   /* ADD */
    case 0x800A: return 2;                   /* SUBTRACT */
    case 0x800B: return 3;                   /* REVSUBTRACT */
    case 0x8007: return 4;                   /* MIN */
    case 0x8008: return 5;                   /* MAX */
    case 0xF006: unmapped("D3DBLENDOP", v); return 1;   /* ADDSIGNED */
    case 0xF005: unmapped("D3DBLENDOP", v); return 3;   /* REVSUBTRACTSIGNED */
    }
    unmapped("D3DBLENDOP", v);
    return 1;
}

static DWORD stencilop_to_host(uint32_t v)
{
    switch (v) {                             /* d3d8_to_d3d11_stencilop: 1..8 */
    case 0x1E00: return 1;                   /* KEEP */
    case 0x0000: return 2;                   /* ZERO */
    case 0x1E01: return 3;                   /* REPLACE */
    case 0x1E02: return 4;                   /* INCRSAT */
    case 0x1E03: return 5;                   /* DECRSAT */
    case 0x150A: return 6;                   /* INVERT */
    case 0x8507: return 7;                   /* INCR */
    case 0x8508: return 8;                   /* DECR */
    }
    unmapped("D3DSTENCILOP", v);
    return 1;
}

static DWORD colorwrite_to_host(uint32_t v)
{
    /* Xbox keeps a byte per channel, A R G B from the top; the host takes
     * the PC's RED 1, GREEN 2, BLUE 4, ALPHA 8. */
    return ((v & 0x00010000) ? 1u : 0u) | ((v & 0x00000100) ? 2u : 0u) |
           ((v & 0x00000001) ? 4u : 0u) | ((v & 0x01000000) ? 8u : 0u);
}

static DWORD cull_to_host(uint32_t v)
{
    switch (v) {
    case 0:     return D3DCULL_NONE;
    case 0x900: return D3DCULL_CW;
    case 0x901: return D3DCULL_CCW;
    }
    unmapped("D3DCULL", v);
    return D3DCULL_NONE;
}

static DWORD fill_to_host(uint32_t v)
{
    if (v >= 0x1B00 && v <= 0x1B02)
        return D3DFILL_POINT + (v - 0x1B00);
    unmapped("D3DFILLMODE", v);
    return D3DFILL_SOLID;
}

static DWORD shade_to_host(uint32_t v)
{
    if (v == 0x1D00)
        return 1;                            /* D3DSHADE_FLAT */
    if (v == 0x1D01)
        return 2;                            /* D3DSHADE_GOURAUD */
    unmapped("D3DSHADEMODE", v);
    return 2;
}

/* Xbox D3DTOP (Cxbx-Reloaded, XbD3D8Types.h, XDK 4039 on) to the host's own
 * enumeration, which numbers the blend ops differently and lacks some. */
static DWORD textureop_to_host(uint32_t v)
{
    switch (v) {
    case 1:  return D3DTOP_DISABLE;
    case 2:  return D3DTOP_SELECTARG1;
    case 3:  return D3DTOP_SELECTARG2;
    case 4:  return D3DTOP_MODULATE;
    case 5:  return D3DTOP_MODULATE2X;
    case 6:  return D3DTOP_MODULATE4X;
    case 7:  return D3DTOP_ADD;
    case 8:  return D3DTOP_ADDSIGNED;
    case 9:  return D3DTOP_ADDSIGNED2X;
    case 10: return D3DTOP_SUBTRACT;
    case 11: return D3DTOP_ADDSMOOTH;
    case 12: return D3DTOP_BLENDDIFFUSEALPHA;
    case 13: return D3DTOP_BLENDCURRENTALPHA;
    case 14: return D3DTOP_BLENDTEXTUREALPHA;
    case 15: return D3DTOP_BLENDFACTORALPHA;
    case 16: unmapped("D3DTOP BLENDTEXTUREALPHAPM", v); return D3DTOP_BLENDTEXTUREALPHA;
    case 17: return D3DTOP_PREMODULATE;
    case 22: return D3DTOP_DOTPRODUCT3;
    case 23: return D3DTOP_MULTIPLYADD;
    case 24: return D3DTOP_LERP;
    }
    unmapped("D3DTOP", v);                   /* 18-21 and the bump-map ops */
    return D3DTOP_MODULATE;
}

static DWORD address_to_host(uint32_t v)
{
    if (v >= 1 && v <= 4)                    /* WRAP, MIRROR, CLAMP, BORDER */
        return v;
    if (v == 5)                              /* CLAMPTOEDGE; host 5 is MIRRORONCE */
        return D3DTADDRESS_CLAMP;
    if (v)
        unmapped("D3DTEXTUREADDRESS", v);
    return D3DTADDRESS_WRAP;
}

static DWORD filter_to_host(uint32_t v)
{
    if (v <= 3)                              /* NONE, POINT, LINEAR, ANISOTROPIC */
        return v;
    unmapped("D3DTEXTUREFILTER", v);         /* QUINCUNX, GAUSSIANCUBIC */
    return D3DTEXF_LINEAR;
}

/* ------------------------------------------------------------------ apply */

typedef enum {
    AS_IS, CMP, BLEND, BLENDOP, STENCILOP, COLORWRITE, CULL, FILL, SHADE
} rs_kind;

static const struct {
    uint16_t xbox;
    uint16_t host;
    uint8_t  kind;
} g_rs_map[] = {
    {  57, D3DRS_ZFUNC,            CMP },
    {  58, D3DRS_ALPHAFUNC,        CMP },
    {  59, D3DRS_ALPHABLENDENABLE, AS_IS },
    {  60, D3DRS_ALPHATESTENABLE,  AS_IS },
    {  61, D3DRS_ALPHAREF,         AS_IS },
    {  62, D3DRS_SRCBLEND,         BLEND },
    {  63, D3DRS_DESTBLEND,        BLEND },
    {  64, D3DRS_ZWRITEENABLE,     AS_IS },
    {  65, D3DRS_DITHERENABLE,     AS_IS },
    {  66, D3DRS_SHADEMODE,        SHADE },
    {  67, D3DRS_COLORWRITEENABLE, COLORWRITE },
    {  68, D3DRS_STENCILZFAIL,     STENCILOP },
    {  69, D3DRS_STENCILPASS,      STENCILOP },
    {  70, D3DRS_STENCILFUNC,      CMP },
    {  71, D3DRS_STENCILREF,       AS_IS },
    {  72, D3DRS_STENCILMASK,      AS_IS },
    {  73, D3DRS_STENCILWRITEMASK, AS_IS },
    {  74, D3DRS_BLENDOP,          BLENDOP },
    {  92, D3DRS_FOGENABLE,        AS_IS },
    {  93, D3DRS_FOGTABLEMODE,     AS_IS },
    {  94, D3DRS_FOGSTART,         AS_IS },  /* float bits */
    {  95, D3DRS_FOGEND,           AS_IS },
    {  96, D3DRS_FOGDENSITY,       AS_IS },
    {  97, D3DRS_RANGEFOGENABLE,   AS_IS },
    /* The lights and the material are forwarded (hle_d3d8.c), so lighting
     * can be: it was held off while they were not, when the host would have
     * lit every vertex with none. */
    { 102, D3DRS_LIGHTING,         AS_IS },
    { 103, D3DRS_SPECULARENABLE,   AS_IS },
    { 105, D3DRS_COLORVERTEX,      AS_IS },
    { 115, D3DRS_AMBIENT,          AS_IS },
    { 138, D3DRS_FOGCOLOR,         AS_IS },
    { 139, D3DRS_FILLMODE,         FILL },
    { 142, D3DRS_NORMALIZENORMALS, AS_IS },
    { 143, D3DRS_ZENABLE,          AS_IS },
    { 144, D3DRS_STENCILENABLE,    AS_IS },
    { 145, D3DRS_STENCILFAIL,      STENCILOP },
    { 147, D3DRS_CULLMODE,         CULL },
    { 148, D3DRS_TEXTUREFACTOR,    AS_IS },
    /* XDK 5849 numbering: SHADOWFUNC sits after MULTISAMPLERENDERTARGETMODE.
     * The compare a shadow-map stage makes (d3d8_combiners.c). */
    { 157, D3DRS_SHADOWFUNC,       CMP },
};

static DWORD rs_to_host(uint8_t kind, uint32_t v)
{
    switch (kind) {
    case CMP:        return cmp_to_host(v);
    case BLEND:      return blend_to_host(v);
    case BLENDOP:    return blendop_to_host(v);
    case STENCILOP:  return stencilop_to_host(v);
    case COLORWRITE: return colorwrite_to_host(v);
    case CULL:       return cull_to_host(v);
    case FILL:       return fill_to_host(v);
    case SHADE:      return shade_to_host(v);
    }
    return v;
}

/* Xbox texture stage state slot (XDK 4039 order) to the host's. */
typedef enum { TS_AS_IS, TS_OP, TS_ADDRESS, TS_FILTER, TS_TEXCOORD } ts_kind;

static const struct {
    uint8_t xbox;
    uint8_t host;
    uint8_t kind;
} g_ts_map[] = {
    {  0, D3DTSS_ADDRESSU,      TS_ADDRESS },
    {  1, D3DTSS_ADDRESSV,      TS_ADDRESS },
    {  3, D3DTSS_MAGFILTER,     TS_FILTER },
    {  4, D3DTSS_MINFILTER,     TS_FILTER },
    {  5, D3DTSS_MIPFILTER,     TS_FILTER },
    {  6, D3DTSS_MIPMAPLODBIAS, TS_AS_IS },
    {  7, D3DTSS_MAXMIPLEVEL,   TS_AS_IS },
    {  8, D3DTSS_MAXANISOTROPY, TS_AS_IS },
    { 12, D3DTSS_COLOROP,       TS_OP },
    { 14, D3DTSS_COLORARG1,     TS_AS_IS },  /* D3DTA_* match the PC's */
    { 15, D3DTSS_COLORARG2,     TS_AS_IS },
    { 16, D3DTSS_ALPHAOP,       TS_OP },
    { 18, D3DTSS_ALPHAARG1,     TS_AS_IS },
    { 19, D3DTSS_ALPHAARG2,     TS_AS_IS },
    { 20, D3DTSS_RESULTARG,     TS_AS_IS },
    { 22, D3DTSS_BUMPENVMAT00,  TS_AS_IS },
    { 23, D3DTSS_BUMPENVMAT01,  TS_AS_IS },
    { 24, D3DTSS_BUMPENVMAT11,  TS_AS_IS },  /* Xbox has 11 before 10 */
    { 25, D3DTSS_BUMPENVMAT10,  TS_AS_IS },
    { 28, D3DTSS_TEXCOORDINDEX, TS_TEXCOORD },
    { 29, D3DTSS_BORDERCOLOR,   TS_AS_IS },
};

static DWORD ts_to_host(uint8_t kind, uint32_t v)
{
    switch (kind) {
    case TS_OP:       return textureop_to_host(v);
    case TS_ADDRESS:  return address_to_host(v);
    case TS_FILTER:   return filter_to_host(v);
    case TS_TEXCOORD: {
        /* Index in the low word, generation mode in the high word. Modes up to
         * 0x30000 are the PC's. Xbox sphere mapping is 0x50000 where the host
         * has 0x40000, and Xbox object-linear (0x40000) has no host value
         * (Cxbx-Reloaded, TextureStates.cpp). */
        uint32_t index = v & 3, mode = v & 0xFFFF0000;

        if ((v & 0xFFFF) > 3)
            unmapped("D3DTSS_TEXCOORDINDEX index", v);
        if (mode <= 0x00030000)
            return mode | index;
        if (mode == 0x00050000)
            return D3DTSS_TCI_SPHEREMAP | index;
        unmapped("D3DTSS_TCI", v);
        return index;
    }
    }
    return v;
}

/* ------------------------------------------------ pixel shader (combiners) */

/* The Xbox's pixel shader is not a program handed to D3D: the XDK writes the
 * NV2A register combiner setup into the first 57 render states, and the
 * texture modes into the first complex one (Cxbx-Reloaded, XbD3D8Types.h,
 * X_D3DRS_PS*; XbSymbolDatabase puts D3DRS_PSTextureModes at the start of
 * D3D_g_ComplexRenderState). The host renderer already turns exactly these
 * into an HLSL pixel shader (d3d8_combiners.c), under its own numbering from
 * 200, as soon as a non-zero token is set.
 *
 * Until they are forwarded every draw is coloured by the texture stage states
 * instead, which modulate by a diffuse colour the title's own pixel shader
 * ignores -- so geometry whose vertex program leaves oD0 at zero draws black,
 * which is what Burnout 2's road does.
 *
 * The two numberings run in different orders, hence the groups. Xbox
 * PSCOMPAREMODE (42) has no host state and is not forwarded;
 * PSFINALCOMBINERCONSTANT0/1 (43, 44) go to the host's 198/199, which the
 * final combiner's C0 and C1 read. */
static const struct { uint8_t xbox; uint8_t host; uint8_t count; } g_ps_map[] = {
    {  0, 200, 8 },      /* PSALPHAINPUTS0-7                   */
    {  8, 208, 2 },      /* PSFINALCOMBINERINPUTSABCD, ...EFG  */
    { 10, 235, 8 },      /* PSCONSTANT0_0-7                    */
    { 18, 243, 8 },      /* PSCONSTANT1_0-7                    */
    { 26, 226, 8 },      /* PSALPHAOUTPUTS0-7                  */
    { 34, 210, 8 },      /* PSRGBINPUTS0-7                     */
    { 45, 218, 8 },      /* PSRGBOUTPUTS0-7                    */
    { 53, 234, 1 },      /* PSCOMBINERCOUNT                    */
    { 43, 198, 2 },      /* PSFINALCOMBINERCONSTANT0, 1        */
    { 55, 252, 2 },      /* PSDOTMAPPING, PSINPUTTEXTURE       */
};

/* PSTextureModes holds one mode per texture stage, 5 bits each over 4 stages
 * (Cxbx-Reloaded, XbPixelShader.cpp: (PSTextureModes >> (i * 5)) & 0x1F,
 * checked against ~0x000FFFFF). The values are the NV2A's, 0x00 to 0x12
 * (xboxdevwiki, NV2A/Pixel Combiner).
 *
 * They go to the host as they are, marked D3D8_PSTEXTUREMODES_XBOX, and
 * d3d8_combiners.c implements them (emit_xbox_textures): the dot-product,
 * reflection and dependent-read modes take PSDOTMAPPING and PSINPUTTEXTURE,
 * forwarded beside them. This used to reduce each mode to the kind of
 * texture it samples, which turned a dot-product stage into a plain lookup
 * of whatever was bound: Halo's bumped cube-map reflections (DOTPRODUCT,
 * DOT_RFLCT_DIFF, DOT_RFLCT_SPEC) never reached its surfaces. The modes the
 * host only approximates are logged once. */
static uint32_t host_texture_modes(uint32_t xbox_modes)
{
    uint32_t i;

    for (i = 0; i < 4u; i++) {
        uint32_t mode = (xbox_modes >> (i * 5u)) & 0x1Fu;

        if (mode > 0x12u || mode == 0x05u || mode == 0x06u || mode == 0x07u ||
            mode == 0x08u || mode == 0x0Au || mode == 0x12u)
            unmapped("PS texture mode", mode);
    }
    return D3D8_PSTEXTUREMODES_XBOX | (xbox_modes & 0x000FFFFFu);
}

/* The pixel shader the title selected, as a D3DPIXELSHADERDEF.
 *
 * D3DDevice_SetPixelShader takes a handle to a shader object that carries the
 * definition at +0x0C (measured on Burnout 2, XDK 4627: the object's third
 * DWORD is a pointer to that address, and the fields below land on values a
 * frame capture independently shows -- PSCombinerCount 0x00011104,
 * PSTextureModes 0x00000001, final inputs 0x130C0300 / 0x00001C80).
 *
 * This matters because the deferred render states do NOT follow it. In one
 * captured race frame the car, the HUD, the text and the fade quad all read
 * back the same combiner state, which no title would write: the states hold
 * whichever shader last went through them, while the draws use whatever
 * SetPixelShader selected. Structure therefore comes from the definition.
 * The constants stay with the render states, which is where the title's own
 * SetPixelShaderConstant calls land at run time.
 *
 * The field order is the Xbox render state order, which is why one offset
 * table serves both paths. */
#define PSDEF_SELF_PTR    0x08
#define PSDEF_AT_HANDLE   0x0C
#define PSDEF_MODES       0xD8

static uint32_t g_ps_def;            /* guest VA of the definition, 0 = none */
static uint32_t g_ps_handle;         /* the object it came out of */
static int      g_ps_def_seen, g_ps_dirty;
/* Set once a shader object has been read successfully. From then on
 * SetPixelShader is the authority: handle 0 means no pixel shader, and the
 * host goes back to fixed function whatever the render states still hold.
 * Until then nothing here is trusted, because a title whose objects are laid
 * out differently would otherwise have every draw quietly demoted. */
static int      g_ps_def_ok;
static int      g_state_dumped;
static int g_state_dump_wanted = -1;

/* A guest heap pointer, roughly: inside the console's RAM, aligned, not a
 * small integer. HLE_MEM32 has no mapped-page check, so a handle that is a
 * token rather than a pointer would fault on the read below. */
static int plausible_va(uint32_t va)
{
    return va >= 0x10000u && va < 0x08000000u && (va & 3u) == 0u;
}

void hle_d3d8_pixel_shader_selected(uint32_t handle)
{
    uint32_t def = 0;

    /* The object carries a pointer to its definition at +0x08. A shader made
     * by CreatePixelShader (Burnout 2, XDK 5344) embeds the definition and the
     * pointer is to its own +0x0C. XDK 4721's SetPixelShaderProgram(pPSDef)
     * makes no copy: it fills a static three-word object in the device,
     * {1, 0, pPSDef}, and selects that, so the pointer leads outside the
     * object to the title's own D3DPIXELSHADERDEF. Both are the same
     * definition layout, a public XDK structure. Anything else is a layout
     * this code has not seen: say so once, and leave the combiners alone. */
    if (handle) {
        uint32_t ptr = plausible_va(handle) ? HLE_MEM32(handle + PSDEF_SELF_PTR) : 0;

        if (ptr == handle + PSDEF_AT_HANDLE) {
            def = ptr;
        } else if (plausible_va(ptr)) {
            static int said;
            def = ptr;
            if (!said++)
                fprintf(stderr, "[HLE-D3D8] shadow pixel shader: SetPixelShader(0x%08X) "
                        "points at a definition outside the object (0x%08X), the "
                        "SetPixelShaderProgram form\n", handle, ptr);
        } else {
            static int warned;

            if (!warned) {
                warned = 1;
                fprintf(stderr, "[HLE-D3D8] shadow pixel shader: SetPixelShader(0x%08X) "
                        "is not a shader object with a definition at +0x%02X or a "
                        "pointer to one; pixel shaders are left off "
                        "(RECOMP_HLE_D3D8_PS=1 forwards the render states instead)\n",
                        handle, PSDEF_AT_HANDLE);
            }
            return;
        }
    }
    if (def && !g_ps_def_seen) {
        g_ps_def_seen = 1;
        fprintf(stderr, "[HLE-D3D8] shadow pixel shader: definitions read from the "
                "shader object (+0x%02X); count 0x%08X, texture modes 0x%08X\n",
                PSDEF_AT_HANDLE, HLE_MEM32(def + 0xD4), HLE_MEM32(def + PSDEF_MODES));
    }
    g_ps_handle = handle;
    g_ps_def = def;
    g_ps_def_ok = 1;
    g_ps_dirty = 1;                  /* re-forward this shader's own states */
}

/* One Xbox pixel shader state, from the selected definition where it carries
 * it. Constants (10-25) always come from the render states. */
static uint32_t ps_state(uint32_t xbox)
{
    uint32_t off;

    if (!g_ps_def || (xbox >= 10 && xbox <= 25))
        return guest_rs(xbox);
    if (xbox <= 7)        off = 0x00 + 4 * xbox;
    else if (xbox <= 9)   off = 0x20 + 4 * (xbox - 8);
    else if (xbox <= 33)  off = 0x68 + 4 * (xbox - 26);
    else if (xbox <= 41)  off = 0x88 + 4 * (xbox - 34);
    else if (xbox <= 44)  off = 0xA8 + 4 * (xbox - 42);
    else if (xbox <= 52)  off = 0xB4 + 4 * (xbox - 45);
    else if (xbox == 53)  off = 0xD4;
    else if (xbox == 54)  off = PSDEF_MODES;      /* PSTextureModes */
    else if (xbox == 55)  off = 0xDC;
    else if (xbox == 56)  off = 0xE0;
    else                  return guest_rs(xbox);
    return HLE_MEM32(g_ps_def + off);
}

/* How many draws run the title's combiners, and how many go to the host's
 * fixed-function pixel path instead -- either because the title has no pixel
 * shader selected, or because its combiner count is zero. */
static void ps_count_draw(uint32_t token)
{
    static unsigned long with, without;
    static DWORD last;
    DWORD now;

    if (token)
        with++;
    else
        without++;
    now = GetTickCount();
    if (!last) {
        last = now;
    } else if (now - last >= 5000) {
        fprintf(stderr, "[HLE-D3D8] shadow pixel shader: %lu draws with a "
                "combiner, %lu without\n", with, without);
        fflush(stderr);
        last = now;
    }
}

/* Slots 0-56 and the complex slot 136 are outside g_rs_map, so they share
 * g_prev_rs with it without colliding -- g_prev_rs holds what was last sent
 * to the host, whichever source the value came from. */
static void forward_pixel_shader(IDirect3DDevice8 *dev)
{
    /* On by default; RECOMP_HLE_D3D8_PS=0 keeps the host's fixed-function
     * pixel path instead.
     *
     * This was opt-in while the combiner path lost content: Burnout 2's car,
     * HUD and text drew nothing with it on. Both causes are fixed -- an alpha
     * input without the alpha bit reads blue (d3d8_combiners.c), and the
     * combiner setup comes from the shader object rather than the render
     * states (above) -- and the two paths now agree on a replayed race frame.
     * With it on, the title's own shaders draw its logos, title screen,
     * loading screens, pause menu, HUD text and race. */
    static int setting = -1;
    uint32_t modes, count, token;
    size_t g;
    int i;

    if (setting < 0) {
        const char *v = getenv("RECOMP_HLE_D3D8_PS");
        setting = v ? v[0] : 0;
    }
    if (setting == '0')
        return;
    /* Without a shader object this code understands, the only source left is
     * the render state array -- and that array does not follow what the title
     * draws with, so it is opt-in (RECOMP_HLE_D3D8_PS=1) rather than the
     * default. */
    if (!g_ps_def_ok && setting != '1')
        return;

    /* The object can be deleted while selected (DeletePixelShader is not
     * replaced), and the guest allocator may hand the block to something
     * else. Re-check it rather than compiling a shader out of whatever is
     * there now. */
    if (g_ps_def && HLE_MEM32(g_ps_handle + PSDEF_SELF_PTR) != g_ps_def) {
        static int warned;

        if (!warned) {
            warned = 1;
            fprintf(stderr, "[HLE-D3D8] shadow pixel shader: the selected shader "
                    "object 0x%08X no longer holds its definition; drawing fixed "
                    "function until the title selects another\n", g_ps_handle);
        }
        g_ps_def = 0;
    }

    if (g_ps_def_ok && !g_ps_def) {
        /* The title turned its pixel shader off. */
        host_combiners_set_pixel_shader(0);
        ps_count_draw(0);
        return;
    }
    modes = ps_state(54);                         /* PSTextureModes      */
    count = ps_state(53);                         /* PSCOMBINERCOUNT     */

    if (g_ps_def) {
        /* Structure comes from the definition, constants from the render
         * states, where SetPixelShaderConstant is assumed to land. Say so
         * once if the definition's own first constant disagrees: that is the
         * signal the assumption is wrong. */
        static int checked;

        if (!checked && HLE_MEM32(g_ps_def + 0x28) != guest_rs(10)) {
            checked = 1;
            fprintf(stderr, "[HLE-D3D8] shadow pixel shader: the definition's C0 "
                    "(0x%08X) and the render state's (0x%08X) differ; constants are "
                    "taken from the render states\n",
                    HLE_MEM32(g_ps_def + 0x28), guest_rs(10));
        }
    }

    for (g = 0; g < sizeof g_ps_map / sizeof g_ps_map[0]; g++) {
        for (i = 0; i < (int)g_ps_map[g].count; i++) {
            uint32_t x = (uint32_t)g_ps_map[g].xbox + (uint32_t)i;
            uint32_t v = ps_state(x);

            if (g_prev_valid && !g_ps_dirty && g_prev_rs[x] == v)
                continue;
            g_prev_rs[x] = v;
            host_SetRenderState(dev, (D3DRENDERSTATETYPE)(g_ps_map[g].host + i), v);
        }
    }
    if (!g_prev_valid || g_ps_dirty || g_prev_rs[XRS_COMPLEX] != modes) {
        g_prev_rs[XRS_COMPLEX] = modes;
        host_SetRenderState(dev, (D3DRENDERSTATETYPE)D3DRS_PSTEXTUREMODES,
                            host_texture_modes(modes));
    }

    /* The token's low four bits are the combiner count; its texture mode bits
     * stay clear so the host reads the state set above instead
     * (d3d8_combiners.c, parse_token then from_render_states). A token of 0
     * leaves the host on its fixed-function path, which is what a title with
     * no pixel shader configured should get.
     *
     * Only a real combiner count turns the combiners on. An earlier version
     * also made a token up whenever the texture modes were non-zero, which
     * switched the combiner path on over a combiner state of all zeros: with
     * no stage sampling anything, every model came out a flat white
     * silhouette. */
    token = count & 0xFu;
    g_ps_dirty = 0;
    host_combiners_set_pixel_shader(token);

    ps_count_draw(token);

    {
        /* Each distinct setup, not just the first: the first draw of a run is
         * the boot screen, whose combiners say nothing about what a race
         * uses. */
        static uint32_t last_count = 0xFFFFFFFFu, last_modes = 0xFFFFFFFFu;
        static int lines;

        if (lines < 12 && (count != last_count || modes != last_modes)) {
            last_count = count;
            last_modes = modes;
            lines++;
            fprintf(stderr, "[HLE-D3D8] shadow pixel shader: combiner count 0x%X, "
                    "texture modes 0x%05X, dot mapping 0x%X, input texture 0x%X, "
                    "stage 0 rgb inputs 0x%08X outputs 0x%08X, final ABCD 0x%08X "
                    "EFG 0x%08X\n", count, modes, ps_state(55), ps_state(56),
                    ps_state(34), ps_state(45), ps_state(8), ps_state(9));
        }
    }
}

void hle_d3d8_shadow_apply_states(IDirect3DDevice8 *dev)
{
    size_t i;
    int s;

    if (!dev || !state_ready())
        return;

    for (i = 0; i < sizeof g_rs_map / sizeof g_rs_map[0]; i++) {
        uint32_t x = g_rs_map[i].xbox;
        uint32_t v = guest_rs(x);
        DWORD host_value;

        if (g_prev_valid && g_prev_rs[x] == v)
            continue;
        g_prev_rs[x] = v;
        host_value = rs_to_host(g_rs_map[i].kind, v);
        host_SetRenderState(dev, (D3DRENDERSTATETYPE)g_rs_map[i].host, host_value);
    }

    for (s = 0; s < STAGES; s++) {
        uint32_t base = hle_var_D3D_g_DeferredTextureState + (uint32_t)(s * STAGE_SIZE * 4);

        for (i = 0; i < sizeof g_ts_map / sizeof g_ts_map[0]; i++) {
            uint32_t x = g_ts_map[i].xbox;
            uint32_t v = HLE_MEM32(base + 4 * ts_slot(x));
            DWORD host_value;

            /* The host's SetTexture rewrites COLOROP whenever a texture is
             * bound or unbound, so the operations are set on every draw. */
            if (g_prev_valid && g_prev_tss[s][x] == v && g_ts_map[i].kind != TS_OP)
                continue;
            g_prev_tss[s][x] = v;
            host_value = ts_to_host(g_ts_map[i].kind, v);
            host_SetTextureStageState(dev, (DWORD)s,
                                      (D3DTEXTURESTAGESTATETYPE)g_ts_map[i].host,
                                      host_value);
        }
    }
    forward_pixel_shader(dev);

    /* RECOMP_HLE_D3D8_STATE_DUMP: what the title's own arrays actually hold,
     * once, on the first draw that gets this far.
     *
     * A title drawing through the host's fixed-function pixel path lives or
     * dies by these values -- the colour operation and its two arguments
     * decide every pixel -- and when the frame comes out black there is no
     * way to tell a state that was never set from one that was set to zero
     * without looking. Black issues ten thousand draws a run, all of them
     * accepted by the renderer, and produces nothing. */
    /* The lookup is cached, not just the dump.
     *
     * g_state_dumped is only set when the dump runs, so with the variable
     * unset -- which is every ordinary run -- the short circuit fell
     * through to getenv on every call, and this is called once per draw.
     * Dino Crisis 3 issues around 300,000 draws in a minute at its menus.
     * Caching only the success of a diagnostic makes the failure path the
     * expensive one, which is backwards. */
    if (g_state_dump_wanted < 0)
        g_state_dump_wanted = getenv("RECOMP_HLE_D3D8_STATE_DUMP") != NULL;
    if (!g_state_dumped && g_state_dump_wanted) {
        g_state_dumped = 1;
        fprintf(stderr, "[HLE-D3D8] state dump, guest values as forwarded:\n");
        for (i = 0; i < sizeof g_rs_map / sizeof g_rs_map[0]; i++)
            fprintf(stderr, "    rs[%2u] = 0x%08X\n",
                    g_rs_map[i].xbox, guest_rs(g_rs_map[i].xbox));
        for (s = 0; s < STAGES; s++) {
            uint32_t base = hle_var_D3D_g_DeferredTextureState
                          + (uint32_t)(s * STAGE_SIZE * 4);
            fprintf(stderr, "    stage %d:", s);
            for (i = 0; i < sizeof g_ts_map / sizeof g_ts_map[0]; i++)
                fprintf(stderr, " ts[%u]=0x%X", g_ts_map[i].xbox,
                        HLE_MEM32(base + 4 * ts_slot(g_ts_map[i].xbox)));
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
    g_prev_valid = 1;
}
#endif /* _WIN32 */

/* void __fastcall D3DDevice_SetRenderState_Simple(DWORD Method, DWORD Value)
 *
 * The XDK's inline SetRenderState for the "simple" states (57-91) writes the
 * NV2A method and value into the push buffer and nothing else; the state
 * array this file reads is written by SetRenderStateNotInline, which calls
 * this and then stores the value. A title that calls the simple form directly
 * -- TimeSplitters 2's renderer does, 1,384 times a minute, for z test, alpha
 * blend, blend factors and z write -- leaves the array stale, and every draw
 * reached the host with whatever blend and depth state the previous path
 * left. Cxbx-Reloaded replaces this function for the same reason
 * (EMUPATCH(D3DDevice_SetRenderState_Simple)).
 *
 * Run the title's own body, then store the value in the slot whose method
 * this is. The method numbers are the hardware's (NV097_SET_*) and so the
 * same in every XDK; the slots are the 4627+ layout the rest of this file
 * assumes. The table is the one SetRenderStateNotInline itself indexes,
 * read out of TimeSplitters 2's .rdata at 0x00222100. */
HLE_ORIGINAL(D3DDevice_SetRenderState_Simple);

HLE_EXPORT(D3DDevice_SetRenderState_Simple)
{
    static const struct { uint16_t method; uint8_t state; } map[] = {
        { 0x0354, 57 },  /* ZFUNC              NV097_SET_DEPTH_FUNC */
        { 0x033C, 58 },  /* ALPHAFUNC          NV097_SET_ALPHA_FUNC */
        { 0x0304, 59 },  /* ALPHABLENDENABLE   NV097_SET_BLEND_ENABLE */
        { 0x0300, 60 },  /* ALPHATESTENABLE    NV097_SET_ALPHA_TEST_ENABLE */
        { 0x0340, 61 },  /* ALPHAREF           NV097_SET_ALPHA_REF */
        { 0x0344, 62 },  /* SRCBLEND           NV097_SET_BLEND_FUNC_SFACTOR */
        { 0x0348, 63 },  /* DESTBLEND          NV097_SET_BLEND_FUNC_DFACTOR */
        { 0x035C, 64 },  /* ZWRITEENABLE       NV097_SET_DEPTH_MASK */
        { 0x0310, 65 },  /* DITHERENABLE       NV097_SET_DITHER_ENABLE */
        { 0x037C, 66 },  /* SHADEMODE          NV097_SET_SHADE_MODE */
        { 0x0358, 67 },  /* COLORWRITEENABLE   NV097_SET_COLOR_MASK */
        { 0x0374, 68 },  /* STENCILZFAIL       NV097_SET_STENCIL_OP_ZFAIL */
        { 0x0378, 69 },  /* STENCILPASS        NV097_SET_STENCIL_OP_ZPASS */
        { 0x0364, 70 },  /* STENCILFUNC        NV097_SET_STENCIL_FUNC */
        { 0x0368, 71 },  /* STENCILREF         NV097_SET_STENCIL_FUNC_REF */
        { 0x036C, 72 },  /* STENCILMASK        NV097_SET_STENCIL_FUNC_MASK */
        { 0x0360, 73 },  /* STENCILWRITEMASK   NV097_SET_STENCIL_MASK */
        { 0x0350, 74 },  /* BLENDOP            NV097_SET_BLEND_EQUATION */
        { 0x034C, 75 },  /* BLENDCOLOR         NV097_SET_BLEND_COLOR */
        { 0x09F8, 76 },  /* SWATHWIDTH         NV097_SET_SWATH_WIDTH */
        { 0x0384, 77 },  /* POLYGONOFFSETZSLOPESCALE */
        { 0x0388, 78 },  /* POLYGONOFFSETZOFFSET */
        { 0x0330, 79 },  /* POINTOFFSETENABLE */
        { 0x0334, 80 },  /* WIREFRAMEOFFSETENABLE */
        { 0x0338, 81 },  /* SOLIDOFFSETENABLE */
    };
    uint32_t method = g_ecx & 0x1FFCu, value = g_edx;
    size_t i;
    /* RECOMP_HLE_D3D8_RS_SIMPLE=0 leaves the array stale, as before this
     * replacement, to tell a wrong state apart from a wrong draw. */
    static int store = -1;

    if (hle_original_D3DDevice_SetRenderState_Simple)
        HLE_CALL_ORIGINAL(D3DDevice_SetRenderState_Simple);
    if (store < 0) {
        const char *e = getenv("RECOMP_HLE_D3D8_RS_SIMPLE");
        store = !(e && *e == '0');
    }
    if (!hle_var_D3D_g_RenderState || !store)
        return;
    for (i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (map[i].method == method) {
            HLE_MEM32(hle_var_D3D_g_RenderState + 4u * map[i].state) = value;
            return;
        }
    }
}
