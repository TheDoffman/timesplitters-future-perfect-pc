/*
 * d3d8_capture -- round-trip the frame capture container (format version 6).
 *
 * Writes a synthetic host-level capture -- a snapshot and a frame, using every
 * chunk kind -- reads it back, and asserts every field and every payload byte
 * survived. Also checks that a truncated capture stops the walk rather than
 * reading past its buffer, that a file which is not a capture is refused, and
 * that a version 1 capture is refused.
 *
 * The data is invented. No game data is used or needed, which is the point:
 * a capture taken from a title is game content and must never be committed
 * (CLAUDE.md, Legal), so the only captures in the tree are the ones this test
 * builds at run time and deletes.
 *
 * The frame is built to be worth replaying (see --write below): a textured
 * fixed-function quad on the left, and on the right a quad drawn by an NV2A
 * vertex program with a declaration, whose colour comes from a NORMPACKED3
 * normal expanded exactly as shadow mode expands it, under the screen-space
 * undo and a register combiner token. Every value is a host value, as shadow
 * mode hands them to src/d3d; the numbers written out below are the host's
 * enumerations from src/d3d/d3d8_xbox.h, repeated here because this test
 * builds without any Direct3D header.
 *
 * What it covers is the container. Whether a real capture describes its frame
 * correctly needs a running title.
 *
 * No Direct3D and no Windows: this runs in the Linux CI job too.
 */
#include "d3d8_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks;
static int g_failures;

static void check(int cond, const char *what)
{
    g_checks++;
    if (!cond) {
        g_failures++;
        printf("FAIL: %s\n", what);
    }
}

#define CAP_PATH "d3d8_capture_roundtrip.tmp"

/* Host enumerations (src/d3d/d3d8_xbox.h). */
enum {
    RS_ZENABLE = 7, RS_ALPHABLENDENABLE = 27, RS_CULLMODE = 22, RS_LIGHTING = 137,
    RS_PSFINALCOMBINERINPUTSABCD = 208, RS_PSFINALCOMBINERINPUTSEFG = 209,
    RS_PSCOMBINERCOUNT = 234,
    CULL_NONE = 1,
    TSS_COLOROP = 1, TSS_COLORARG1 = 2, TSS_COLORARG2 = 3,
    TSS_ALPHAOP = 4, TSS_ALPHAARG1 = 5,
    TOP_DISABLE = 1, TOP_SELECTARG1 = 2, TOP_MODULATE = 4,
    TA_DIFFUSE = 0, TA_TEXTURE = 2,
    TS_VIEW = 2, TS_PROJECTION = 3, TS_WORLD = 256,
    PT_TRIANGLELIST = 4, PT_TRIANGLESTRIP = 5,
    FMT_INDEX16 = 101,
    FMT_LIN_A8R8G8B8 = 0x12,
    CLEAR_TARGET = 1, CLEAR_ZBUFFER = 2,
    DXGI_R32G32B32_FLOAT = 6,
    FVF_XYZRHW_DIFFUSE_TEX1 = 0x004 | 0x040 | 0x100
};

#define PROGRAM   0x10000u      /* d3d8_vsh.c hands out slot + 0x10000 */
#define SCRATCH   0x10001u      /* created and deleted inside the frame */
#define CLEAR_COLOR 0xFF203060u
#define TARGET_COLOR 0xFF00FF00u
#define USAGE_RENDERTARGET 0x1u
#define FMT_D24S8 0x2Au

/* ---------------------------------------------------------------- microcode
 *
 * Two NV2A instructions: MOV oPos, v0 and MOV oD0, v2. Bit offsets are from
 * docs/technical/nv2a-vertex-program-encoding.md, as tests/nv2a_vsh_hlsl uses
 * them. */
enum {
    F_A_SWZ_W = 32, F_A_SWZ_Z = 34, F_A_SWZ_Y = 36, F_A_SWZ_X = 38,
    F_INPUT = 41, F_MAC = 53,
    F_A_MUX = 90, F_FINAL = 96, F_OUT_MUX = 98, F_OUT_ADDRESS = 99,
    F_OUT_ORB = 107, F_OUT_O_MASK = 108
};

static void set_field(uint32_t *insn, int start, int size, uint32_t value)
{
    int word = start / 32, bit = start % 32;
    uint32_t mask = ((1u << size) - 1u) << bit;

    insn[word] = (insn[word] & ~mask) | ((value << bit) & mask);
}

static void mov_input_to_output(uint32_t *w, uint32_t input, uint32_t out_addr, int final)
{
    memset(w, 0, 4 * sizeof *w);
    set_field(w, F_A_SWZ_X, 2, 0);
    set_field(w, F_A_SWZ_Y, 2, 1);
    set_field(w, F_A_SWZ_Z, 2, 2);
    set_field(w, F_A_SWZ_W, 2, 3);
    set_field(w, F_MAC, 4, 1);                  /* MOV */
    set_field(w, F_A_MUX, 2, 2);                /* A from the input bank */
    set_field(w, F_INPUT, 4, input);
    set_field(w, F_OUT_MUX, 1, 0);              /* the MAC drives the output */
    set_field(w, F_OUT_ORB, 1, 1);              /* an output register */
    set_field(w, F_OUT_ADDRESS, 8, out_addr);   /* 0 oPos, 3 oD0 */
    set_field(w, F_OUT_O_MASK, 4, 0xF);
    set_field(w, F_FINAL, 1, (uint32_t)final);
}

/* ----------------------------------------------------------------- vertices */

/* NORMPACKED3: x 11 bits, y 11 bits, z 10 bits, signed, over 1023, 1023 and
 * 511 (Cxbx-Reloaded's vertex conversion). The same arithmetic as
 * hle_d3d8.c's shadow_expand_vertices, so the host vertex below is what shadow
 * mode would hand over for this packed one. */
static void unpack_normal(uint32_t bits, float n[3])
{
    n[0] = (float)((int32_t)(bits << 21) >> 21) / 1023.0f;
    n[1] = (float)((int32_t)(bits << 10) >> 21) / 1023.0f;
    n[2] = (float)((int32_t)bits >> 22) / 511.0f;
}

/* x = +1023 (1.0), y = +511 (about 0.5), z = 0: an orange oD0. */
#define PACKED_NORMAL (0x3FFu | (511u << 11))

/* The Xbox vertex was position (12) + packed normal (4). The host vertex has
 * the unpacked normal in front: 12 + 16 = 28 bytes, with the program's
 * declaration moved up by 12 to match. */
#define PROGRAM_STRIDE 28u

static void program_vertex(uint8_t *out, float x, float y)
{
    float normal[3], pos[3];
    uint32_t packed = PACKED_NORMAL;

    unpack_normal(packed, normal);
    pos[0] = x;
    pos[1] = y;
    pos[2] = 0.5f;
    memcpy(out, normal, 12);
    memcpy(out + 12, pos, 12);
    memcpy(out + 24, &packed, 4);
}

/* XYZRHW + DIFFUSE + TEX1: 16 + 4 + 8. */
#define FVF_STRIDE 28u

static void fvf_vertex(uint8_t *out, float x, float y, float u, float v)
{
    float xyzw[4];
    float uv[2];
    uint32_t white = 0xFFFFFFFFu;

    xyzw[0] = x;
    xyzw[1] = y;
    xyzw[2] = 0.5f;
    xyzw[3] = 1.0f;
    uv[0] = u;
    uv[1] = v;
    memcpy(out, xyzw, 16);
    memcpy(out + 16, &white, 4);
    memcpy(out + 20, uv, 8);
}

static uint8_t g_fvf_quad[4 * FVF_STRIDE];
static uint8_t g_program_quad[4 * PROGRAM_STRIDE];
static const uint16_t QUAD_INDICES[6] = { 0, 1, 2, 0, 2, 3 };

/* A 4x4 LIN_A8R8G8B8 texture with 3 levels, back to back as the host keeps
 * it: red/green checker, then 2x2 and 1x1. Stored B, G, R, A. */
static uint8_t g_texels[16 * 4 + 4 * 4 + 1 * 4];
static uint8_t g_refill[16 * 4];

static void build_data(void)
{
    uint32_t x, y;

    fvf_vertex(g_fvf_quad + 0 * FVF_STRIDE,  40.0f,  40.0f, 0.0f, 0.0f);
    fvf_vertex(g_fvf_quad + 1 * FVF_STRIDE, 280.0f,  40.0f, 1.0f, 0.0f);
    fvf_vertex(g_fvf_quad + 2 * FVF_STRIDE,  40.0f, 280.0f, 0.0f, 1.0f);
    fvf_vertex(g_fvf_quad + 3 * FVF_STRIDE, 280.0f, 280.0f, 1.0f, 1.0f);

    program_vertex(g_program_quad + 0 * PROGRAM_STRIDE, 360.0f,  40.0f);
    program_vertex(g_program_quad + 1 * PROGRAM_STRIDE, 600.0f,  40.0f);
    program_vertex(g_program_quad + 2 * PROGRAM_STRIDE, 600.0f, 280.0f);
    program_vertex(g_program_quad + 3 * PROGRAM_STRIDE, 360.0f, 280.0f);

    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++) {
            uint8_t *p = g_texels + (y * 4 + x) * 4;
            int red = ((x ^ y) & 1) == 0;
            p[0] = 0x00;
            p[1] = red ? 0x00 : 0xFF;
            p[2] = red ? 0xFF : 0x00;
            p[3] = 0xFF;
        }
    memset(g_texels + 64, 0x80, 16);
    memset(g_texels + 80, 0xFF, 4);
    /* The refill writes the same checker back: it exercises TEXTURE_LEVEL
     * without changing what a replay draws. */
    memcpy(g_refill, g_texels, sizeof g_refill);
}

/* ------------------------------------------------------------------ writer */

static void w_render_state(D3D8CapWriter *w, uint32_t state, uint32_t value)
{
    D3D8CapRenderState c;

    c.state = state;
    c.value = value;
    d3d8cap_chunk(w, D3D8CAP_RENDER_STATE, &c, sizeof c, NULL, 0, NULL, 0);
}

static void w_stage_state(D3D8CapWriter *w, uint32_t stage, uint32_t type, uint32_t value)
{
    D3D8CapStageState c;

    c.stage = stage;
    c.type  = type;
    c.value = value;
    d3d8cap_chunk(w, D3D8CAP_TEXTURE_STAGE_STATE, &c, sizeof c, NULL, 0, NULL, 0);
}

static void w_u32(D3D8CapWriter *w, uint32_t type, uint32_t value)
{
    d3d8cap_chunk(w, type, &value, sizeof value, NULL, 0, NULL, 0);
}

static void w_set_texture(D3D8CapWriter *w, uint32_t stage, uint32_t id)
{
    D3D8CapSetTexture c;

    c.stage = stage;
    c.texture_id = id;
    d3d8cap_chunk(w, D3D8CAP_SET_TEXTURE, &c, sizeof c, NULL, 0, NULL, 0);
}

static void w_vs_create(D3D8CapWriter *w, uint32_t handle, const uint32_t *code,
                        uint32_t insns)
{
    D3D8CapVsCreate c;

    c.handle = handle;
    c.insn_count = insns;
    d3d8cap_chunk(w, D3D8CAP_VS_CREATE, &c, sizeof c, code, insns * 16u, NULL, 0);
}

static uint32_t g_microcode[2 * 4];
static const D3D8CapVsInput DECL[2] = {
    { 0, DXGI_R32G32B32_FLOAT, 12 },    /* v0 position, behind the normal */
    { 2, DXGI_R32G32B32_FLOAT, 0 },     /* v2 the unpacked normal */
};
static const float CONST58[2 * 4] = {
    320.0f, -240.0f, 1.0f, 1.0f,  320.0f, 240.0f, 0.0f, 0.0f
};
static const D3D8CapLevel LEVELS[3] = { { 16, 4, 64 }, { 8, 2, 16 }, { 4, 1, 4 } };
static const float IDENTITY[16] = {
    1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1
};

/* How many chunks write_capture emits, for the read-back and truncation
 * checks. */
#define SNAPSHOT_CHUNKS 27
#define FRAME_CHUNKS    22

static int write_capture(const char *path)
{
    D3D8CapWriter *w = d3d8cap_create(path, 7, 640, 480);
    D3D8CapVsDeclaration decl = { PROGRAM, 2 };
    D3D8CapVsConstants consts = { 58, 2 };
    D3D8CapVsScreenspace screen = { 1, { 320.0f, -240.0f, 1.0f, 1.0f },
                                       { 320.0f, 240.0f, 0.0f, 0.0f } };
    D3D8CapTexture tex = { 1, FMT_LIN_A8R8G8B8, 4, 4, 3, 0 };
    D3D8CapTexture scratch_tex = { 2, FMT_LIN_A8R8G8B8, 1, 1, 1, 0 };
    D3D8CapLevel scratch_level = { 4, 1, 4 };
    static const uint8_t scratch_texel[4] = { 1, 2, 3, 4 };
    D3D8CapTextureLevel refill = { 1, 0, 16, 4, 64 };
    D3D8CapTexture target_tex = { 3, FMT_LIN_A8R8G8B8, 1, 1, 1, USAGE_RENDERTARGET };
    D3D8CapDepthSurface depth = { 1, 1, 1, FMT_D24S8 };
    D3D8CapSetRenderTarget to_target = { 3, 0, 0, 1 };
    D3D8CapSetRenderTarget to_back = { 0, 0, 0, 0 };
    D3D8CapSetRenderTarget to_face = { 4, 0, 2, 1 };   /* cube face 2 */
    D3D8CapCubeTexture cube = { 4, FMT_LIN_A8R8G8B8, 1, 1, USAGE_RENDERTARGET };
    D3D8CapClear face_clear = { 0, CLEAR_TARGET, 0xFF0000FFu, 1.0f, 0 };
    D3D8CapVsVertexData vdata = { 3, { 0.25f, 0.5f, 0.75f, 1.0f } };
    D3D8CapClear target_clear = { 0, CLEAR_TARGET | CLEAR_ZBUFFER, TARGET_COLOR, 1.0f, 0 };
    D3D8CapTransform xf;
    D3D8CapViewport vp = { 0, 0, 640, 480, 0.0f, 1.0f };
    D3D8CapClear clear = { 0, CLEAR_TARGET | CLEAR_ZBUFFER, CLEAR_COLOR, 1.0f, 0 };
    D3D8CapDrawUp up = { PT_TRIANGLESTRIP, 2, FVF_STRIDE, sizeof g_fvf_quad };
    D3D8CapDrawIndexedUp iup = {
        PT_TRIANGLELIST, 0, 4, 2, FMT_INDEX16, sizeof QUAD_INDICES,
        PROGRAM_STRIDE, sizeof g_program_quad
    };
    static const uint32_t transforms[3] = { TS_VIEW, TS_PROJECTION, TS_WORLD };
    uint32_t i;

    if (!w) {
        printf("FAIL: could not create %s\n", path);
        return -1;
    }
    build_data();
    mov_input_to_output(g_microcode + 0, 0, 0, 0);     /* MOV oPos, v0 */
    mov_input_to_output(g_microcode + 4, 2, 3, 1);     /* MOV oD0, v2  */

    /* ---- snapshot: 27 chunks, in the writer's documented order, less the
     * sixteen input current values the real writer puts after the
     * screen-space chunk (the frame below carries one) */
    w_vs_create(w, PROGRAM, g_microcode, 2);                                 /* 1 */
    d3d8cap_chunk(w, D3D8CAP_VS_DECLARATION, &decl, sizeof decl,
                  DECL, sizeof DECL, NULL, 0);                               /* 2 */
    d3d8cap_chunk(w, D3D8CAP_VS_CONSTANTS, &consts, sizeof consts,
                  CONST58, sizeof CONST58, NULL, 0);                         /* 3 */
    d3d8cap_chunk(w, D3D8CAP_VS_SCREENSPACE, &screen, sizeof screen,
                  NULL, 0, NULL, 0);                                         /* 4 */
    w_u32(w, D3D8CAP_PS_TOKEN, 0);                                           /* 5 */
    w_u32(w, D3D8CAP_SET_VERTEX_SHADER, FVF_XYZRHW_DIFFUSE_TEX1);            /* 6 */
    d3d8cap_chunk(w, D3D8CAP_TEXTURE, &tex, sizeof tex, LEVELS, sizeof LEVELS,
                  g_texels, sizeof g_texels);                                /* 7 */
    w_set_texture(w, 0, 1);                                                  /* 8 */
    for (i = 1; i < 4; i++)
        w_set_texture(w, i, 0);                                              /* 9-11 */
    d3d8cap_chunk(w, D3D8CAP_SET_RENDER_TARGET, &to_back, sizeof to_back,
                  NULL, 0, NULL, 0);                                         /* 12 */
    memcpy(xf.m, IDENTITY, sizeof xf.m);
    for (i = 0; i < 3; i++) {
        xf.state = transforms[i];
        d3d8cap_chunk(w, D3D8CAP_TRANSFORM, &xf, sizeof xf, NULL, 0, NULL, 0); /* 13-15 */
    }
    d3d8cap_chunk(w, D3D8CAP_VIEWPORT, &vp, sizeof vp, NULL, 0, NULL, 0);   /* 16 */
    w_render_state(w, RS_ZENABLE, 0);                                        /* 17 */
    w_render_state(w, RS_CULLMODE, CULL_NONE);                               /* 18 */
    w_render_state(w, RS_LIGHTING, 0);                                       /* 19 */
    w_render_state(w, RS_ALPHABLENDENABLE, 0);                               /* 20 */
    /* One combiner stage writing nothing; the final combiner's D is the
     * diffuse colour (register 4) and G its alpha (0x14: register 4, alpha
     * replicate), which is how the program draw gets its colour
     * (d3d8_combiners.c, from_render_states: first input in the top byte). */
    w_render_state(w, RS_PSCOMBINERCOUNT, 1);                                /* 21 */
    w_render_state(w, RS_PSFINALCOMBINERINPUTSABCD, 0x00000004u);            /* 22 */
    w_render_state(w, RS_PSFINALCOMBINERINPUTSEFG, 0x00001400u);             /* 23 */
    w_stage_state(w, 0, TSS_COLOROP, TOP_MODULATE);                          /* 24 */
    w_stage_state(w, 0, TSS_COLORARG1, TA_TEXTURE);                          /* 25 */
    w_stage_state(w, 0, TSS_COLORARG2, TA_DIFFUSE);                          /* 26 */
    d3d8cap_chunk(w, D3D8CAP_FRAME_START, NULL, 0, NULL, 0, NULL, 0);        /* 27 */

    /* ---- frame: 22 chunks. The offscreen passes (2-9) clear a 1x1 target
     * green and one cube face blue, then go back to the back buffer, so a
     * replay that forgot either switch shows that colour instead of the two
     * quads. */
    d3d8cap_chunk(w, D3D8CAP_CLEAR, &clear, sizeof clear, NULL, 0, NULL, 0); /* 1 */
    d3d8cap_chunk(w, D3D8CAP_TEXTURE, &target_tex, sizeof target_tex,
                  &scratch_level, sizeof scratch_level,
                  scratch_texel, sizeof scratch_texel);                      /* 2 */
    d3d8cap_chunk(w, D3D8CAP_DEPTH_SURFACE, &depth, sizeof depth,
                  NULL, 0, NULL, 0);                                         /* 3 */
    d3d8cap_chunk(w, D3D8CAP_SET_RENDER_TARGET, &to_target, sizeof to_target,
                  NULL, 0, NULL, 0);                                         /* 4 */
    d3d8cap_chunk(w, D3D8CAP_CLEAR, &target_clear, sizeof target_clear,
                  NULL, 0, NULL, 0);                                         /* 5 */
    d3d8cap_chunk(w, D3D8CAP_CUBE_TEXTURE, &cube, sizeof cube,
                  NULL, 0, NULL, 0);                                         /* 6 */
    d3d8cap_chunk(w, D3D8CAP_SET_RENDER_TARGET, &to_face, sizeof to_face,
                  NULL, 0, NULL, 0);                                         /* 7 */
    d3d8cap_chunk(w, D3D8CAP_CLEAR, &face_clear, sizeof face_clear,
                  NULL, 0, NULL, 0);                                         /* 8 */
    d3d8cap_chunk(w, D3D8CAP_SET_RENDER_TARGET, &to_back, sizeof to_back,
                  NULL, 0, NULL, 0);                                         /* 9 */
    w_stage_state(w, 0, TSS_ALPHAOP, TOP_SELECTARG1);                        /* 10 */
    d3d8cap_chunk(w, D3D8CAP_DRAW_UP, &up, sizeof up,
                  g_fvf_quad, sizeof g_fvf_quad, NULL, 0);                   /* 8 */
    d3d8cap_chunk(w, D3D8CAP_TEXTURE_LEVEL, &refill, sizeof refill,
                  g_refill, sizeof g_refill, NULL, 0);                       /* 9 */
    d3d8cap_chunk(w, D3D8CAP_TEXTURE, &scratch_tex, sizeof scratch_tex,
                  &scratch_level, sizeof scratch_level,
                  scratch_texel, sizeof scratch_texel);                      /* 10 */
    w_u32(w, D3D8CAP_TEXTURE_RELEASE, 2);                                    /* 11 */
    w_vs_create(w, SCRATCH, g_microcode, 2);                                 /* 12 */
    w_u32(w, D3D8CAP_VS_DELETE, SCRATCH);                                    /* 13 */
    /* v3's current value; the program reads only v0 and v2, so it changes
     * nothing a replay draws. */
    d3d8cap_chunk(w, D3D8CAP_VS_VERTEX_DATA, &vdata, sizeof vdata,
                  NULL, 0, NULL, 0);                                         /* 14 */
    w_u32(w, D3D8CAP_PS_TOKEN, 1);                                           /* 15 */
    w_u32(w, D3D8CAP_SET_VERTEX_SHADER, PROGRAM);                            /* 16 */
    d3d8cap_chunk(w, D3D8CAP_DRAW_INDEXED_UP, &iup, sizeof iup,
                  QUAD_INDICES, sizeof QUAD_INDICES,
                  g_program_quad, sizeof g_program_quad);                    /* 17 */
    w_u32(w, D3D8CAP_PS_TOKEN, 0);                                           /* 18 */
    w_u32(w, D3D8CAP_SET_VERTEX_SHADER, FVF_XYZRHW_DIFFUSE_TEX1);            /* 19 */

    if (d3d8cap_chunk_count(w) != SNAPSHOT_CHUNKS + FRAME_CHUNKS)
        printf("FAIL: wrote %u chunks, expected %d\n", d3d8cap_chunk_count(w),
               SNAPSHOT_CHUNKS + FRAME_CHUNKS);
    return d3d8cap_close(w);
}

/* ------------------------------------------------------------------ reader */

static int next_of(D3D8CapReader *r, D3D8CapChunk *c, uint32_t type, const char *what)
{
    int ok = d3d8cap_next(r, c) && c->type == type;

    check(ok, what);
    return ok;
}

static void read_capture(void)
{
    char err[128];
    D3D8CapReader *r = d3d8cap_open(CAP_PATH, err, sizeof err);
    const D3D8CapHeader *h;
    D3D8CapChunk c;
    int n = 0, i;

    check(r != NULL, "the capture reopens");
    if (!r) {
        printf("  reason: %s\n", err);
        return;
    }
    h = d3d8cap_header(r);
    check(h->version == 7 && D3D8CAP_VERSION == 7, "the version is 7");
    check(h->frame == 7 && h->width == 640 && h->height == 480, "header fields");
    check(h->chunk_count == SNAPSHOT_CHUNKS + FRAME_CHUNKS, "chunk_count is patched in");

    if (next_of(r, &c, D3D8CAP_VS_CREATE, "vs_create first")) {
        const D3D8CapVsCreate *p = c.data;
        const uint32_t *code = d3d8cap_tail(&c, sizeof *p, 2 * 16);
        uint32_t w3;

        check(p->handle == PROGRAM && p->insn_count == 2, "vs_create fields");
        check(code && !memcmp(code, g_microcode, sizeof g_microcode), "microcode bytes");
        /* The final bit is bit 96, the low bit of word 3 of the last one. */
        w3 = code ? code[7] : 0;
        check((w3 & 1u) == 1u && code && (code[3] & 1u) == 0u, "only the last insn is final");
        check(d3d8cap_tail(&c, sizeof *p, 2 * 16 + 1) == NULL, "tail refuses a read past the chunk");
    }
    if (next_of(r, &c, D3D8CAP_VS_DECLARATION, "vs_declaration")) {
        const D3D8CapVsDeclaration *p = c.data;
        const D3D8CapVsInput *in = d3d8cap_tail(&c, sizeof *p, 2 * sizeof *in);

        check(p->handle == PROGRAM && p->count == 2, "declaration fields");
        check(in && in[0].reg == 0 && in[0].offset == 12 && in[1].reg == 2 &&
              in[1].offset == 0 && in[1].dxgi_format == DXGI_R32G32B32_FLOAT,
              "declaration inputs");
    }
    if (next_of(r, &c, D3D8CAP_VS_CONSTANTS, "vs_constants")) {
        const D3D8CapVsConstants *p = c.data;
        const float *f = d3d8cap_tail(&c, sizeof *p, sizeof CONST58);

        check(p->first_reg == 58 && p->count == 2 && f &&
              !memcmp(f, CONST58, sizeof CONST58), "constants");
    }
    if (next_of(r, &c, D3D8CAP_VS_SCREENSPACE, "vs_screenspace")) {
        const D3D8CapVsScreenspace *p = c.data;

        check(p->enabled == 1 && p->scale[0] == 320.0f && p->scale[1] == -240.0f &&
              p->offset[1] == 240.0f, "screenspace fields");
    }
    if (next_of(r, &c, D3D8CAP_PS_TOKEN, "ps_token"))
        check(((const D3D8CapPsToken *)c.data)->token == 0, "token 0 in the snapshot");
    if (next_of(r, &c, D3D8CAP_SET_VERTEX_SHADER, "set_vertex_shader"))
        check(((const D3D8CapSetVertexShader *)c.data)->handle == FVF_XYZRHW_DIFFUSE_TEX1,
              "FVF handle");
    if (next_of(r, &c, D3D8CAP_TEXTURE, "texture")) {
        const D3D8CapTexture *t = c.data;
        const D3D8CapLevel *l = d3d8cap_tail(&c, sizeof *t, sizeof LEVELS);
        const uint8_t *b = d3d8cap_tail(&c, sizeof *t + sizeof LEVELS, sizeof g_texels);

        check(t->id == 1 && t->format == FMT_LIN_A8R8G8B8 && t->width == 4 &&
              t->height == 4 && t->levels == 3, "texture fields");
        check(l && !memcmp(l, LEVELS, sizeof LEVELS), "level table");
        check(b && !memcmp(b, g_texels, sizeof g_texels), "texel bytes");
        check(c.bytes == sizeof *t + sizeof LEVELS + sizeof g_texels, "texture length");
    }
    for (i = 0; i < 4; i++)
        if (next_of(r, &c, D3D8CAP_SET_TEXTURE, "set_texture per stage")) {
            const D3D8CapSetTexture *p = c.data;
            check(p->stage == (uint32_t)i && p->texture_id == (i ? 0u : 1u), "stage bindings");
        }
    if (next_of(r, &c, D3D8CAP_SET_RENDER_TARGET, "render target in the snapshot")) {
        const D3D8CapSetRenderTarget *p = c.data;
        check(c.bytes == sizeof *p && p->texture_id == 0 && p->level == 0 &&
              p->depth_id == 0, "the snapshot names the back buffer");
    }
    for (i = 0; i < 3; i++)
        if (next_of(r, &c, D3D8CAP_TRANSFORM, "transform")) {
            const D3D8CapTransform *p = c.data;
            check(!memcmp(p->m, IDENTITY, sizeof IDENTITY), "transform matrix");
            check(p->state == (i == 0 ? TS_VIEW : i == 1 ? TS_PROJECTION : TS_WORLD),
                  "host transform numbering");
        }
    if (next_of(r, &c, D3D8CAP_VIEWPORT, "viewport")) {
        const D3D8CapViewport *p = c.data;
        check(p->width == 640 && p->height == 480 && p->max_z == 1.0f, "viewport fields");
    }
    for (i = 0; i < 7; i++)
        if (next_of(r, &c, D3D8CAP_RENDER_STATE, "render states"))
            check(c.bytes == sizeof(D3D8CapRenderState), "render state length");
    for (i = 0; i < 3; i++)
        if (next_of(r, &c, D3D8CAP_TEXTURE_STAGE_STATE, "stage states"))
            check(((const D3D8CapStageState *)c.data)->stage == 0, "stage state stage");
    if (next_of(r, &c, D3D8CAP_FRAME_START, "frame_start ends the snapshot"))
        check(c.bytes == 0 && c.data == NULL, "frame_start has no payload");

    /* The frame. */
    if (next_of(r, &c, D3D8CAP_CLEAR, "clear")) {
        const D3D8CapClear *p = c.data;
        check(p->rect_count == 0 && p->flags == 3 && p->color == CLEAR_COLOR &&
              p->z == 1.0f, "clear fields");
    }
    if (next_of(r, &c, D3D8CAP_TEXTURE, "render target texture")) {
        const D3D8CapTexture *t = c.data;
        check(t->id == 3 && t->usage == USAGE_RENDERTARGET && t->levels == 1,
              "render target texture fields");
    }
    if (next_of(r, &c, D3D8CAP_DEPTH_SURFACE, "depth_surface")) {
        const D3D8CapDepthSurface *p = c.data;
        check(c.bytes == sizeof *p && p->id == 1 && p->width == 1 && p->height == 1 &&
              p->format == FMT_D24S8, "depth surface fields");
    }
    if (next_of(r, &c, D3D8CAP_SET_RENDER_TARGET, "set_render_target")) {
        const D3D8CapSetRenderTarget *p = c.data;
        check(p->texture_id == 3 && p->level == 0 && p->face == 0 && p->depth_id == 1,
              "offscreen target fields");
    }
    if (next_of(r, &c, D3D8CAP_CLEAR, "offscreen clear"))
        check(((const D3D8CapClear *)c.data)->color == TARGET_COLOR, "offscreen clear colour");
    if (next_of(r, &c, D3D8CAP_CUBE_TEXTURE, "cube_texture")) {
        const D3D8CapCubeTexture *p = c.data;
        check(c.bytes == sizeof *p && p->id == 4 && p->edge == 1 && p->levels == 1 &&
              p->usage == USAGE_RENDERTARGET, "cube texture fields");
    }
    if (next_of(r, &c, D3D8CAP_SET_RENDER_TARGET, "cube face target")) {
        const D3D8CapSetRenderTarget *p = c.data;
        check(p->texture_id == 4 && p->face == 2 && p->depth_id == 1,
              "cube face fields");
    }
    next_of(r, &c, D3D8CAP_CLEAR, "cube face clear");
    if (next_of(r, &c, D3D8CAP_SET_RENDER_TARGET, "back to the back buffer")) {
        const D3D8CapSetRenderTarget *p = c.data;
        check(p->texture_id == 0 && p->depth_id == 0, "back buffer fields");
    }
    next_of(r, &c, D3D8CAP_TEXTURE_STAGE_STATE, "in-frame stage state");
    if (next_of(r, &c, D3D8CAP_DRAW_UP, "draw_up")) {
        const D3D8CapDrawUp *d = c.data;
        const uint8_t *v = d3d8cap_tail(&c, sizeof *d, d->vertex_bytes);
        check(d->prim_type == PT_TRIANGLESTRIP && d->prim_count == 2 &&
              d->stride == FVF_STRIDE && d->vertex_bytes == 4 * FVF_STRIDE, "draw_up fields");
        check(v && !memcmp(v, g_fvf_quad, sizeof g_fvf_quad), "draw_up vertex bytes");
    }
    if (next_of(r, &c, D3D8CAP_TEXTURE_LEVEL, "texture_level")) {
        const D3D8CapTextureLevel *t = c.data;
        const uint8_t *b = d3d8cap_tail(&c, sizeof *t, t->bytes);
        check(t->id == 1 && t->level == 0 && t->bytes == 64 && b &&
              !memcmp(b, g_refill, 64), "texture_level");
    }
    next_of(r, &c, D3D8CAP_TEXTURE, "scratch texture");
    if (next_of(r, &c, D3D8CAP_TEXTURE_RELEASE, "texture_release"))
        check(((const D3D8CapTextureId *)c.data)->id == 2, "released id");
    if (next_of(r, &c, D3D8CAP_VS_CREATE, "scratch program"))
        check(((const D3D8CapVsCreate *)c.data)->handle == SCRATCH, "scratch handle");
    if (next_of(r, &c, D3D8CAP_VS_DELETE, "vs_delete"))
        check(((const D3D8CapVsHandle *)c.data)->handle == SCRATCH, "deleted handle");
    if (next_of(r, &c, D3D8CAP_VS_VERTEX_DATA, "vs_vertex_data")) {
        const D3D8CapVsVertexData *p = c.data;
        check(c.bytes == sizeof *p && p->reg == 3 && p->value[0] == 0.25f &&
              p->value[2] == 0.75f && p->value[3] == 1.0f, "vertex data fields");
    }
    if (next_of(r, &c, D3D8CAP_PS_TOKEN, "ps_token in frame"))
        check(((const D3D8CapPsToken *)c.data)->token == 1, "token 1");
    if (next_of(r, &c, D3D8CAP_SET_VERTEX_SHADER, "program select"))
        check(((const D3D8CapSetVertexShader *)c.data)->handle == PROGRAM, "program handle");
    if (next_of(r, &c, D3D8CAP_DRAW_INDEXED_UP, "draw_indexed_up")) {
        const D3D8CapDrawIndexedUp *d = c.data;
        const uint16_t *idx = d3d8cap_tail(&c, sizeof *d, d->index_bytes);
        const uint8_t *v = d3d8cap_tail(&c, sizeof *d + d->index_bytes, d->vertex_bytes);
        float normal[3];

        check(d->prim_type == PT_TRIANGLELIST && d->num_vertices == 4 &&
              d->prim_count == 2 && d->index_format == FMT_INDEX16 &&
              d->stride == PROGRAM_STRIDE, "draw_indexed_up fields");
        check(idx && !memcmp(idx, QUAD_INDICES, sizeof QUAD_INDICES), "index bytes");
        check(v && !memcmp(v, g_program_quad, sizeof g_program_quad), "program vertex bytes");
        /* The expanded normal in front of each vertex is the packed one
         * behind it, unpacked. */
        if (v) {
            uint32_t packed;
            float want[3];

            memcpy(normal, v + 2 * PROGRAM_STRIDE, 12);
            memcpy(&packed, v + 2 * PROGRAM_STRIDE + 24, 4);
            unpack_normal(packed, want);
            check(!memcmp(normal, want, 12) && want[0] == 1.0f && want[2] == 0.0f &&
                  want[1] > 0.49f && want[1] < 0.51f, "NORMPACKED3 expansion");
        }
    }
    next_of(r, &c, D3D8CAP_PS_TOKEN, "token back to 0");
    next_of(r, &c, D3D8CAP_SET_VERTEX_SHADER, "FVF back");

    check(!d3d8cap_next(r, &c), "the END terminator stops the walk");

    d3d8cap_rewind(r);
    while (d3d8cap_next(r, &c)) {
        check(c.type < D3D8CAP_CHUNK_KINDS &&
              strcmp(d3d8cap_chunk_name(c.type), "unknown") != 0, "every kind has a name");
        n++;
    }
    check(n == SNAPSHOT_CHUNKS + FRAME_CHUNKS, "rewind walks the same chunks again");
    check(strcmp(d3d8cap_chunk_name(999), "unknown") == 0, "an unknown kind is named so");
    d3d8cap_close_read(r);
}

static long file_bytes(const char *path, unsigned char **out)
{
    FILE *f = fopen(path, "rb");
    long size;

    *out = NULL;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out = malloc((size_t)size);
    if (!*out || fread(*out, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(*out);
        *out = NULL;
        return -1;
    }
    fclose(f);
    return size;
}

static void write_bytes(const char *path, const unsigned char *b, size_t n)
{
    FILE *f = fopen(path, "wb");

    if (f) {
        fwrite(b, 1, n, f);
        fclose(f);
    }
}

static void truncated_capture(void)
{
    const char *path = "d3d8_capture_truncated.tmp";
    unsigned char *bytes;
    long size = file_bytes(CAP_PATH, &bytes);
    char err[128];
    D3D8CapReader *r;
    D3D8CapChunk c;
    int seen = 0;

    check(size > 64, "the round-trip capture can be read for truncation");
    if (size <= 64)
        return;

    /* Cut it off part-way through the payload of the indexed draw, the last
     * large chunk: END (8), two 12-byte chunks, then inside the draw. */
    write_bytes(path, bytes, (size_t)size - 60u);
    free(bytes);

    r = d3d8cap_open(path, err, sizeof err);
    check(r != NULL, "a truncated capture still opens");
    if (r) {
        while (d3d8cap_next(r, &c))
            seen++;
        check(seen == SNAPSHOT_CHUNKS + FRAME_CHUNKS - 3,
              "the walk stops at the last whole chunk");
        d3d8cap_close_read(r);
    }
    remove(path);
}

static void rejects_version_1(void)
{
    const char *path = "d3d8_capture_v1.tmp";
    unsigned char *bytes;
    long size = file_bytes(CAP_PATH, &bytes);
    char err[128];
    D3D8CapReader *r;
    uint32_t v1 = 1;

    if (size < (long)sizeof(D3D8CapHeader)) {
        check(0, "the round-trip capture can be read for the version check");
        free(bytes);
        return;
    }
    memcpy(bytes + offsetof(D3D8CapHeader, version), &v1, sizeof v1);
    write_bytes(path, bytes, (size_t)size);
    free(bytes);

    r = d3d8cap_open(path, err, sizeof err);
    check(r == NULL, "a version 1 capture is refused");
    check(r != NULL || strstr(err, "version") != NULL, "and the reason names the version");
    if (r)
        d3d8cap_close_read(r);
    remove(path);
}

/* The round-trip capture relabelled as another version: does it open? */
static int opens_as_version(uint32_t version)
{
    const char *path = "d3d8_capture_vN.tmp";
    unsigned char *bytes;
    long size = file_bytes(CAP_PATH, &bytes);
    char err[128];
    D3D8CapReader *r;

    if (size < (long)sizeof(D3D8CapHeader)) {
        free(bytes);
        return -1;
    }
    memcpy(bytes + offsetof(D3D8CapHeader, version), &version, sizeof version);
    write_bytes(path, bytes, (size_t)size);
    free(bytes);
    r = d3d8cap_open(path, err, sizeof err);
    if (r)
        d3d8cap_close_read(r);
    remove(path);
    return r != NULL;
}

/* Version 6 only added chunk kinds, so a version 5 file is a version 6 file
 * without lights and must still open: the saved captures are the second
 * renderer's regression corpus. Version 4 changed a payload, and a build
 * cannot read a version newer than itself. */
static void version_range(void)
{
    check(D3D8CAP_VERSION_MIN_READ == 5, "the oldest readable version is 5");
    check(opens_as_version(5) == 1, "a version 5 capture opens");
    check(opens_as_version(4) == 0, "a version 4 capture is refused");
    check(opens_as_version(D3D8CAP_VERSION + 1) == 0, "a newer version is refused");
}

static void rejects_junk(void)
{
    const char *path = "d3d8_capture_junk.tmp";
    char err[128];
    D3D8CapReader *r;

    write_bytes(path, (const unsigned char *)"not a capture at all, but long enough to be one", 48);
    r = d3d8cap_open(path, err, sizeof err);
    check(r == NULL, "a file that is not a capture is refused");
    if (r)
        d3d8cap_close_read(r);
    remove(path);
}

int main(int argc, char **argv)
{
    /* --write <path> keeps the synthetic capture instead of testing it, so
     * the replay tool can be smoke-tested end to end without game files:
     *
     *   d3d8_capture_test --write synthetic.d3dcap
     *   d3d8_replay synthetic.d3dcap --out synthetic --loops 3 --dump-every
     *
     * The image should show, on a dark blue clear, a red/green checker on the
     * left (fixed function, texture) and an orange square on the right
     * (vertex program, declaration, NORMPACKED3 normal, screen-space undo,
     * combiners), and every loop's image should be byte-identical. */
    if (argc == 3 && strcmp(argv[1], "--write") == 0) {
        if (write_capture(argv[2]) != 0) {
            printf("could not write %s\n", argv[2]);
            return 1;
        }
        printf("wrote synthetic capture %s\n", argv[2]);
        return 0;
    }

    if (write_capture(CAP_PATH) == 0) {
        read_capture();
        truncated_capture();
        rejects_version_1();
        version_range();
    } else {
        check(0, "the synthetic capture is written");
    }
    rejects_junk();
    remove(CAP_PATH);

    printf("d3d8_capture: %d checks, %d failed\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
