/*
 * d3d8_capture.h -- the D3D8 frame capture container: one frame of the calls
 * shadow mode made on the HOST renderer, plus every byte they read.
 *
 * Why this exists: reaching an interesting frame in shadow mode
 * (hle_d3d8.c, RECOMP_HLE_D3D8=shadow) means running the title for minutes
 * and driving menus, and what the renderer does is then never quite the same
 * twice. A capture is the same frame every time, replays in seconds from
 * src/replay with no game running, and so turns a change to the shader
 * translation into an image to look at rather than another play-through.
 *
 * Why the host boundary (version 2). Version 1 recorded the title's Xbox
 * values, and the replay tool repeated shadow mode's Xbox-to-host conversion.
 * That conversion kept growing -- vertex declarations, NORMPACKED3 unpacking,
 * the screen-space undo and its constants, the per-draw viewport choice,
 * pixel shader forwarding -- and every piece the replay did not copy made a
 * replayed frame differ from the live one. Now the capture holds exactly what
 * src/hle handed to src/d3d, after all conversion, and replay is a player
 * with no Xbox knowledge: it calls the same host functions with the same
 * arguments. A replay therefore shows what the host renderer does with the
 * frame. It cannot be used to re-examine the Xbox-to-host conversion; that
 * needs a live run.
 *
 * This file is the container only: no Direct3D, no Windows, no guest memory.
 * It builds on every platform so the round-trip test (tests/d3d8_capture)
 * runs in the Linux CI job as well as the Windows one. The writer's driver is
 * hle_d3d8_record.c, Windows-only like the rest of shadow mode; the reader's
 * is src/replay.
 *
 * ------------------------------------------------------------------ layout
 *
 * Little-endian throughout, and no packing pragmas: every field of every
 * payload struct below is a 4-byte uint32_t, int32_t or float, so each struct
 * is the same size and shape on MSVC x64 and gcc x86-64. Host enum values
 * (D3DRENDERSTATETYPE, D3DFORMAT, DXGI_FORMAT, ...) are stored as uint32_t,
 * which is their size on both. Guest and hosts are all little-endian
 * (CLAUDE.md), so nothing is byte-swapped. A capture is NOT a long-term
 * archive format, and the version is checked exactly, not ranged.
 *
 *   D3D8CapHeader                      (32 bytes, at offset 0)
 *   then chunk_count chunks:
 *     uint32_t type;                   D3D8CAP_* below
 *     uint32_t bytes;                  payload length, NOT counting these 8
 *     uint8_t  payload[bytes];
 *     uint8_t  pad[];                  to the next 4-byte boundary
 *
 * A capture has two parts, in this order:
 *   1. a snapshot of the host's state at the frame boundary, read back from
 *      src/d3d itself rather than from src/hle's change caches, and ended by
 *      one D3D8CAP_FRAME_START chunk;
 *   2. every host call shadow mode made until the next Swap, in call order.
 * The snapshot uses the same chunk types as the calls, so replay has one path
 * for both. Its order is fixed by the writer: programs and their
 * declarations, constants, screen-space, input current values, combiner
 * token, vertex shader,
 * textures, render target, transforms, viewport, render states, texture
 * stage states. The
 * stage states come after the textures because the host's SetTexture
 * rewrites D3DTSS_COLOROP (d3d8_device.c, dev_SetTexture).
 *
 * Handles. Vertex program handles are the host's own (d3d8_vsh.c, slot +
 * 0x10000). Replay creates its own programs and maps each recorded handle to
 * the one it got. Textures are numbered by the capture: a host texture object
 * gets an id the first time the capture needs its contents, and
 * D3D8CAP_TEXTURE_RELEASE retires the id when shadow mode releases the
 * object, so an object reallocated at the same address gets a new id.
 *
 * Not in the format:
 *   - push-buffer traffic. A title that fills its own push buffer through
 *     BeginPush (Burnout 2 does, at two call sites -- see hle_d3d8.c)
 *     bypasses every replacement, so shadow mode never draws those, and
 *     neither does a replay.
 *   - host state src/hle never sets: lights, material, stream sources,
 *     the device's own pixel shader handle. A replay leaves them at the
 *     device's defaults, as the live run does. (Stage palettes are recorded
 *     since version 7: src/hle sets them to expand P8 textures.)
 *   - what a render target held before the frame. Its TEXTURE chunk carries
 *     the host's system-memory copy, which rendering never writes, so a
 *     target drawn in an earlier frame and only sampled in this one replays
 *     as zeros.
 *   - volume textures, which shadow mode does not create. One bound on a
 *     stage would be recorded as nothing bound.
 *   - the contents of a cube a title renders into (they live on the GPU and
 *     are drawn again on replay). A cube filled from memory has its texels
 *     recorded (CUBE_LEVEL, version 7).
 *   - anything time-varying: no timestamps, no frame pacing.
 */
#ifndef XBOXRECOMP_D3D8_CAPTURE_H
#define XBOXRECOMP_D3D8_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* "XBRD3D8", NUL-terminated, so `head -c8` on a capture is readable. */
#define D3D8CAP_MAGIC        "XBRD3D8"
#define D3D8CAP_MAGIC_BYTES  8

/* Bumped on any change to a payload struct or chunk meaning. The reader
 * refuses anything else rather than guessing: captures are cheap to retake.
 * Version 1 was the title-level format; its chunk numbers mean different
 * things here. Version 3 added render targets and input current values,
 * version 4 cube textures and the face a render target names, version 6
 * fixed-function lights and the material, version 7 the texels of cubes
 * filled from memory and the stage palettes P8 textures are expanded with. */
#define D3D8CAP_VERSION      7u
/* The oldest version the reader still accepts. A bump that only adds chunk
 * kinds leaves an older file a valid newer one that happens not to contain
 * them (version 6 added three; version 5 one), so it leaves this alone. A
 * bump that changes a payload struct or what a chunk means must raise this
 * to itself -- version 4 changed what a render target names, so nothing
 * before 5 is readable. Captures stopped being cheap to retake once they
 * became the regression corpus for a second renderer. */
#define D3D8CAP_VERSION_MIN_READ 5u

/* The conventional extension. .gitignore has it: a capture contains the
 * title's own textures and vertices, so it is game content and must never be
 * committed (CLAUDE.md, Legal). */
#define D3D8CAP_EXTENSION    ".d3dcap"

enum {
    D3D8CAP_END                 = 0,  /* optional terminator; readers stop at EOF too */
    D3D8CAP_FRAME_START         = 1,  /* no payload: the snapshot ends here */
    D3D8CAP_CLEAR               = 2,  /* D3D8CapClear + D3D8CapRect[rect_count] */
    D3D8CAP_RENDER_STATE        = 3,  /* D3D8CapRenderState */
    D3D8CAP_TEXTURE_STAGE_STATE = 4,  /* D3D8CapStageState */
    D3D8CAP_TRANSFORM           = 5,  /* D3D8CapTransform */
    D3D8CAP_VIEWPORT            = 6,  /* D3D8CapViewport */
    D3D8CAP_SET_TEXTURE         = 7,  /* D3D8CapSetTexture */
    D3D8CAP_SET_VERTEX_SHADER   = 8,  /* D3D8CapSetVertexShader */
    D3D8CAP_DRAW_UP             = 9,  /* D3D8CapDrawUp + vertices */
    D3D8CAP_DRAW_INDEXED_UP     = 10, /* D3D8CapDrawIndexedUp + indices + vertices */
    D3D8CAP_TEXTURE             = 11, /* D3D8CapTexture + D3D8CapLevel[levels] + bytes */
    D3D8CAP_TEXTURE_LEVEL       = 12, /* D3D8CapTextureLevel + bytes */
    D3D8CAP_TEXTURE_RELEASE     = 13, /* D3D8CapTextureId */
    D3D8CAP_VS_CREATE           = 14, /* D3D8CapVsCreate + uint32_t[insn_count * 4] */
    D3D8CAP_VS_DELETE           = 15, /* D3D8CapVsHandle */
    D3D8CAP_VS_DECLARATION      = 16, /* D3D8CapVsDeclaration + D3D8CapVsInput[count] */
    D3D8CAP_VS_CONSTANTS        = 17, /* D3D8CapVsConstants + float[count * 4] */
    D3D8CAP_VS_SCREENSPACE      = 18, /* D3D8CapVsScreenspace */
    D3D8CAP_PS_TOKEN            = 19, /* D3D8CapPsToken */
    D3D8CAP_DEPTH_SURFACE       = 20, /* D3D8CapDepthSurface */
    D3D8CAP_SET_RENDER_TARGET   = 21, /* D3D8CapSetRenderTarget */
    D3D8CAP_VS_VERTEX_DATA      = 22, /* D3D8CapVsVertexData */
    D3D8CAP_CUBE_TEXTURE        = 23, /* D3D8CapCubeTexture */
    D3D8CAP_SCISSORS            = 24, /* D3D8CapScissors (version 5) */
    D3D8CAP_MATERIAL            = 25, /* D3D8CapMaterial (version 6) */
    D3D8CAP_LIGHT               = 26, /* D3D8CapLight */
    D3D8CAP_LIGHT_ENABLE        = 27, /* D3D8CapLightEnable */
    D3D8CAP_TWOD_PLACEMENT      = 28, /* D3D8CapTwoDPlacement (version 6) */
    D3D8CAP_CUBE_LEVEL          = 29, /* D3D8CapCubeLevel + bytes (version 7) */
    D3D8CAP_PALETTE             = 30, /* D3D8CapPalette (version 7) */
    D3D8CAP_CHUNK_KINDS         = 31  /* one past the last, for per-kind counters */
};

typedef struct {
    char     magic[D3D8CAP_MAGIC_BYTES];  /* D3D8CAP_MAGIC */
    uint32_t version;                     /* D3D8CAP_VERSION */
    uint32_t header_bytes;                /* sizeof(D3D8CapHeader); chunks start here */
    uint32_t frame;                       /* which swap this was, counting from 1 */
    uint32_t width, height;               /* the host device's back buffer */
    uint32_t chunk_count;                 /* content chunks, excluding the END
                                           * terminator; patched in at close,
                                           * and 0 in a capture whose run died
                                           * before it closed */
} D3D8CapHeader;

/* IDirect3DDevice8::Clear, as called. flags are the host's D3DCLEAR_*, after
 * shadow mode's Xbox mapping. */
typedef struct {
    uint32_t rect_count, flags, color;
    float    z;
    uint32_t stencil;
} D3D8CapClear;
typedef struct { int32_t x1, y1, x2, y2; } D3D8CapRect;     /* D3DRECT */

typedef struct { uint32_t state, value; } D3D8CapRenderState;
typedef struct { uint32_t stage, type, value; } D3D8CapStageState;

/* state is the host D3DTRANSFORMSTATETYPE (VIEW 2, PROJECTION 3, TEXTURE0-3
 * 16-19, WORLD-WORLD3 256-259). */
typedef struct { uint32_t state; float m[16]; } D3D8CapTransform;

typedef struct { uint32_t x, y, width, height; float min_z, max_z; } D3D8CapViewport;

/* The Xbox scissor as the host holds it: how many rectangles the title set,
 * whether they were exclusive, and the first rectangle (the only one the
 * host applies). count 0 is "off". */
typedef struct { uint32_t count, exclusive; D3D8CapRect rect; } D3D8CapScissors;

/* D3DMATERIAL8 and D3DLIGHT8 as words: 17 floats, and a D3DLIGHTTYPE then
 * 25 floats. The same layout on the Xbox and the PC. */
typedef struct { uint32_t words[17]; } D3D8CapMaterial;
typedef struct { uint32_t index; uint32_t words[26]; } D3D8CapLight;
typedef struct { uint32_t index, enable; } D3D8CapLightEnable;
/* xbox_D3D8SetTwoDPlacement: where the next screen-space draws go in
 * widescreen (XBOX_D3D8_2D_*), and the title's tag for the call site. */
typedef struct { uint32_t placement, tag; } D3D8CapTwoDPlacement;

/* texture_id 0 is SetTexture(stage, NULL). */
typedef struct { uint32_t stage, texture_id; } D3D8CapSetTexture;

/* The argument to the host's SetVertexShader: an FVF code, or a program
 * handle (d3d8_vsh_is_programmable), which replay maps to its own. */
typedef struct { uint32_t handle; } D3D8CapSetVertexShader;

/* DrawPrimitiveUP. prim_type is the host D3DPRIMITIVETYPE; vertex_bytes is
 * what the host read: d3d8_up_vertices_read() vertices of `stride`. */
typedef struct { uint32_t prim_type, prim_count, stride, vertex_bytes; } D3D8CapDrawUp;

/* DrawIndexedPrimitiveUP. index_format is the host D3DFORMAT (INDEX16 101 or
 * INDEX32 102); index_bytes covers d3d8_up_indices_read() indices, and
 * vertex_bytes is num_vertices * stride, the range the host uploads. */
typedef struct {
    uint32_t prim_type, min_index, num_vertices, prim_count;
    uint32_t index_format, index_bytes, stride, vertex_bytes;
} D3D8CapDrawIndexedUp;

/* A host texture's creation and full contents: CreateTexture with these
 * arguments (pool D3DPOOL_MANAGED, the only one src/hle uses), then every
 * level filled. format is the D3DFORMAT the host was given -- an Xbox format
 * code, which src/d3d takes directly. The level bytes are the host's
 * system-memory copy (d3d8_texture_level), rows of `pitch` bytes, so replay
 * writes them back through LockRect unchanged. A TEXTURE chunk for an id that
 * replay already holds replaces that texture's contents. */
typedef struct {
    uint32_t id, format, width, height, levels, usage;
} D3D8CapTexture;
typedef struct { uint32_t pitch, rows, bytes; } D3D8CapLevel;

/* One level filled again: shadow mode locked and unlocked a texture the
 * capture already holds (hle_d3d8_texture.c re-uploads a texture whose guest
 * texels changed). */
typedef struct { uint32_t id, level, pitch, rows, bytes; } D3D8CapTextureLevel;

typedef struct { uint32_t id; } D3D8CapTextureId;

/* d3d8_vsh_create_shader; handle is what it returned. */
typedef struct { uint32_t handle, insn_count; } D3D8CapVsCreate;
typedef struct { uint32_t handle; } D3D8CapVsHandle;

/* d3d8_vsh_set_declaration. The host's D3D8VshInput is { int reg;
 * DXGI_FORMAT format; UINT offset; } (d3d8_vsh.h), copied field by field. */
typedef struct { uint32_t handle, count; } D3D8CapVsDeclaration;
typedef struct { int32_t reg; uint32_t dxgi_format, offset; } D3D8CapVsInput;

/* d3d8_vsh_set_constant; first_reg is 0..191. */
typedef struct { uint32_t first_reg, count; } D3D8CapVsConstants;

/* d3d8_vsh_set_screenspace when enabled is 1. enabled 0 appears only in a
 * snapshot taken before anything turned it on. */
typedef struct { uint32_t enabled; float scale[4], offset[4]; } D3D8CapVsScreenspace;

/* d3d8_vsh_set_vertex_data: input register reg (0-15)'s current value. The
 * snapshot carries all 16. */
typedef struct { uint32_t reg; float value[4]; } D3D8CapVsVertexData;

/* d3d8_combiners_set_pixel_shader. */
typedef struct { uint32_t token; } D3D8CapPsToken;

/* CreateDepthStencilSurface with these arguments (no multisampling). Ids are
 * numbered by the capture, separately from texture ids, from 1, the first
 * time a SET_RENDER_TARGET names the surface. */
typedef struct { uint32_t id, width, height, format; } D3D8CapDepthSurface;

/* A cube texture's creation: CreateCubeTexture with these arguments. The
 * contents follow as CUBE_LEVEL chunks for a cube filled from memory; a cube
 * the title renders into has none -- what it draws there is drawn again on
 * replay. */
typedef struct { uint32_t id, format, edge, levels, usage; } D3D8CapCubeTexture;

/* One face and level of a cube's texels, as the host holds them (the
 * system-memory copy, in the Xbox packing its upload takes; d3d8_cube_level).
 * Written after the CUBE_TEXTURE chunk of a cube that is not a render target
 * -- a normalisation or environment cube the title keeps in memory -- and
 * again when such a cube is refilled. Replay writes the bytes back through
 * LockRect, which uploads them the way the live run did. */
typedef struct { uint32_t id, face, level, pitch, rows, bytes; } D3D8CapCubeLevel;

/* SetPalette(stage, entries): the 256 D3DCOLOR entries a P8 texture bound to
 * that stage is expanded through when it is uploaded. The snapshot carries
 * all four stages, before the textures. */
typedef struct { uint32_t stage; uint32_t entries[256]; } D3D8CapPalette;

/* SetRenderTarget. texture_id 0 is the back buffer; otherwise level `level`
 * of that texture, which was created with D3DUSAGE_RENDERTARGET. `face` is
 * 0 for a 2D texture and the cube face (0-5) when the id names a cube.
 * depth_id 0 is no depth, D3D8CAP_DEPTH_DEVICE the device's own depth
 * buffer, and anything else a DEPTH_SURFACE chunk's id. */
#define D3D8CAP_DEPTH_DEVICE 0xFFFFFFFFu
typedef struct { uint32_t texture_id, level, face, depth_id; } D3D8CapSetRenderTarget;

/* A short name for a chunk type ("draw_up"), or "unknown", for logs. */
const char *d3d8cap_chunk_name(uint32_t type);

/* ------------------------------------------------------------------ writer */

typedef struct D3D8CapWriter D3D8CapWriter;

/* Creates the file and reserves its header. Returns NULL if it cannot be
 * opened, so a mistyped RECOMP_D3D8_CAPTURE path disables capture rather
 * than stopping the run. */
D3D8CapWriter *d3d8cap_create(const char *path, uint32_t frame,
                              uint32_t width, uint32_t height);

/* One chunk, from up to three pieces, so a header and its payload go out
 * without being concatenated into a temporary buffer first. Any piece may be
 * NULL/0. Returns 0 on success, -1 once the stream has failed; a writer that
 * has failed stays failed and every later call is a no-op. */
int d3d8cap_chunk(D3D8CapWriter *w, uint32_t type,
                  const void *p0, size_t n0,
                  const void *p1, size_t n1,
                  const void *p2, size_t n2);

/* Writes the terminator, patches chunk_count into the header and closes.
 * Returns 0 if the whole capture was written, -1 otherwise; the writer is
 * freed either way. */
int d3d8cap_close(D3D8CapWriter *w);

/* How many chunks have been written so far (for the run log). */
uint32_t d3d8cap_chunk_count(const D3D8CapWriter *w);

/* ------------------------------------------------------------------ reader */

typedef struct {
    uint32_t    type;
    uint32_t    bytes;
    const void *data;    /* into the reader's buffer; valid until close */
} D3D8CapChunk;

typedef struct D3D8CapReader D3D8CapReader;

/* Reads the whole capture into memory -- a frame is tens of megabytes at
 * most, and replay walks it repeatedly. On failure returns NULL and writes a
 * reason into err (never truncated to nothing; err may be NULL). A capture of
 * any other version, including version 1, is refused. */
D3D8CapReader *d3d8cap_open(const char *path, char *err, size_t err_bytes);

const D3D8CapHeader *d3d8cap_header(const D3D8CapReader *r);

/* Next chunk, or 0 at the end. Every chunk is bounds-checked against the file
 * -- a capture from a run that crashed mid-frame is truncated, which must
 * stop the walk rather than read past the buffer. */
int d3d8cap_next(D3D8CapReader *r, D3D8CapChunk *out);

/* Back to the first chunk, for replaying the same capture again. */
void d3d8cap_rewind(D3D8CapReader *r);

void d3d8cap_close_read(D3D8CapReader *r);

/* Payload accessor: the bytes that follow a fixed-size payload header inside
 * a chunk, bounds-checked. Returns NULL if the chunk is too short to hold
 * `head` bytes plus `want` bytes, which is how replay rejects a malformed
 * chunk without trusting its own counts. */
const void *d3d8cap_tail(const D3D8CapChunk *c, size_t head, size_t want);

#ifdef __cplusplus
}
#endif

#endif /* XBOXRECOMP_D3D8_CAPTURE_H */
