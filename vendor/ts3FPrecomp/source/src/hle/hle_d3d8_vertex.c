/*
 * hle_d3d8_vertex.c -- vertex buffer draws and vertex shader constants,
 * forwarded to the shadow device.
 *
 * Part of shadow mode (hle_d3d8.c, RECOMP_HLE_D3D8=shadow). Every replacement
 * runs the title's own body first.
 *
 * A vertex buffer is a guest X_D3DVertexBuffer (Common, Data, Lock): Data is
 * physical, so the vertices are read at guest 0x80000000 + Data, like texels
 * (Cxbx-Reloaded, GetDataFromXboxResource). Rather than mirror buffers on the
 * host, each draw hands the vertices it needs to the host's UP draws, the same
 * path DrawVerticesUP takes (hle_d3d8.c).
 *
 * DrawIndexedVertices takes a pointer to 16-bit indices and adds the base
 * vertex index set with SetIndices (Cxbx-Reloaded, Direct3D9.cpp). Burnout 2's
 * XDK symbols do not name SetIndices, but every DrawIndexedVertices calls
 * CDevice_SetStateVB with it (7,188 of each in a profiled minute): the lifted
 * DrawIndexedVertices pushes the device's base index, and SetStateVB multiplies
 * its argument by the stride and adds the buffer's Data when it sets the
 * vertex array offsets. So the base is captured there. A title whose symbols
 * do not name that function -- or name only the stdcall CDevice_SetStateVB_8,
 * `this` on the stack -- never updates it; the first indexed buffer draw
 * without it says so once, and draws with base 0.
 *
 * The host binds one stream, stream 0. A vertex program that reads another
 * one -- Outrun 2's road takes v13 from stream 1 -- has those registers copied
 * in beside each stream 0 vertex (shadow_expand_vertices in hle_d3d8.c),
 * which finds them through hle_d3d8_stream_vertices below at the first
 * vertex each buffer draw passes along. FVF draws use only stream 0.
 *
 * Vertex shader constants arrive through fastcall setters: register in ecx,
 * data in edx, and for the NotInline pair a count of floats on the stack.
 * The register is already 0..191, the host's range; Cxbx-Reloaded subtracts
 * 96 on the way in only because its shared setter adds it back.
 *
 * XDK 3925 has only that shared setter, D3DDevice_SetVertexShaderConstant,
 * and it is the exception to the line above: its register is -96-based and
 * the XDK biases it itself, so the replacement at the bottom of this file
 * adds 96 to reach the same range the fastcall ones already arrive in.
 */
#include "platform/xbox_winnt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hle.h"

#ifdef _WIN32
#include "d3d8_xbox.h"
#include "d3d8_vsh.h"
#include "hle_d3d8_record.h"

/* From hle_d3d8.c. */
IDirect3DDevice8 *hle_d3d8_shadow_device(void);
void hle_d3d8_shadow_draw(uint32_t xpt, uint32_t count, const void *verts,
                          uint32_t stride, int from_buffer);
void hle_d3d8_shadow_draw_indexed(uint32_t xpt, uint32_t count, const uint16_t *idx,
                                  const void *verts, uint32_t stride, int from_buffer);
void hle_d3d8_shadow_set_first_vertex(uint32_t first);

#define CONTIG_BASE 0x80000000u
#define CONTIG_SIZE (64u * 1024u * 1024u)    /* kernel.h XBOX_CONTIG_SIZE */
#define MAX_CONSTANT_REGISTERS 192

static uint32_t g_stream0_vb;        /* guest X_D3DVertexBuffer */
static uint32_t g_stream0_stride;
static uint32_t g_stream_vb[16], g_stream_stride[16];   /* every stream, 0 included */
static uint32_t g_base_vertex;
static int      g_base_vertex_seen;  /* CDevice_SetStateVB has run */
static int      g_in_notinline;      /* inside SetVertexShaderConstantNotInline */
static unsigned long g_skip_no_stream, g_skip_range;

/* Vertex arrays the title programs itself, through BeginPush and EndPush.
 *
 * NV097_SET_VERTEX_DATA_ARRAY_FORMAT (0x1760 + 4i) gives attribute i its
 * format and its own stride, NV097_SET_VERTEX_DATA_ARRAY_OFFSET (0x1720 + 4i)
 * its own physical address. The XDK writes both from its stream and
 * declaration state in CDevice_SetStateVB. A title may push its own instead
 * and then call DrawVertices or DrawIndexedVertices: TimeSplitters: Future
 * Perfect draws its models that way, every attribute in an array of its own,
 * after selecting a declaration that matches. Stream 0 then describes
 * whatever the last XDK-path draw used, and reading the draw's vertices from
 * it gave a 24-byte vertex buffer to 28- to 36-byte layouts -- every model in
 * the 2401 intro cutscene skipped as "stride", or drawn as shards.
 *
 * BeginPush runs CDevice_SetStateVB first, so the XDK's own arrays are
 * written before the title's and the title's are what the GPU uses until the
 * XDK has reason to write its own again: a new declaration
 * (hle_d3d8_push_arrays_off, from the vertex shader selection in hle_d3d8.c)
 * or a new stream. Until then each draw is gathered from the pushed arrays
 * (push_gather) into one vertex laid out as the selected declaration says. */
static uint32_t g_push_fmt[16], g_push_off[16];
static uint32_t g_push_known;          /* attributes pushed since the last reset */
static int      g_push_active;
static unsigned long g_push_draws, g_push_failed;

uint32_t hle_d3d8_shadow_program_object(void);   /* hle_d3d8.c */
static int guest_readable(uint32_t va, uint64_t bytes);

void hle_d3d8_push_arrays_off(void)
{
    g_push_active = 0;
    g_push_known = 0;
}

/* Bytes of one X_D3DVSDT element: the low nibble is the type, the high one
 * the component count (FLOAT 2, D3DCOLOR 0, NORMSHORT 1, PBYTE 4, SHORT 5,
 * NORMPACKED3 6). 0 for a format this cannot size. */
static UINT vsdt_bytes(uint32_t format)
{
    UINT count = (format >> 4) & 0xFu;

    if (format == 0x72u)                 /* FLOAT2H: three floats */
        return 12u;
    switch (format & 0xFu) {
    case 2:  return 4u * count;
    case 0:
    case 4:  return count;
    case 1:
    case 5:  return 2u * count;
    case 6:  return 4u;
    default: return 0u;
    }
}

/* The commands between BeginPush's pointer and EndPush's: only the vertex
 * array formats and offsets are kept. Stops at anything that is not a method
 * header (a jump has no business inside one push). */
static void push_scan(uint32_t start, uint32_t end)
{
    uint32_t va = start, pushed = 0;

    while (va + 4u <= end) {
        uint32_t w = HLE_MEM32(va);
        uint32_t count, method, i;
        int noninc;

        va += 4u;
        if ((w & 0xFFFF0003u) == 0x00020000u || (w & 3u) == 2u)
            continue;                    /* return, call */
        if ((w & 0x00030003u) != 0u || (w & 0xE0000000u) == 0x20000000u)
            break;                       /* a jump, or not a command */
        count = (w >> 18) & 0x7FFu;
        method = w & 0x1FFCu;
        noninc = (w & 0xE0000000u) == 0x40000000u;
        for (i = 0; i < count && va + 4u <= end; i++, va += 4u) {
            uint32_t m = noninc ? method : method + i * 4u;
            uint32_t value = HLE_MEM32(va);

            if (m >= 0x1720u && m < 0x1760u) {
                g_push_off[(m - 0x1720u) / 4u] = value;
                pushed |= 1u << ((m - 0x1720u) / 4u);
            } else if (m >= 0x1760u && m < 0x17A0u) {
                g_push_fmt[(m - 0x1760u) / 4u] = value;
                pushed |= 1u << ((m - 0x1760u) / 4u);
            }
        }
    }
    if (pushed) {
        static int said;
        g_push_known |= pushed;
        g_push_active = 1;
        if (!said++)
            fprintf(stderr, "[HLE-D3D8] the title pushes its own vertex arrays (BeginPush, "
                    "attributes 0x%04X); its draws are gathered from them until the next "
                    "declaration or stream\n", pushed);
    }
}

/* hle_d3d8.c's D3DDevice_EndPush hands every push here: the commands from
 * where the device's push pointer still points (what BeginPush handed out;
 * BeginStateBig does not advance it) to where EndPush takes it back. */
void hle_d3d8_push_arrays_scan(uint32_t start, uint32_t end)
{
    if (hle_d3d8_shadow_device() && start && end > start && end - start <= 0x10000u &&
        guest_readable(start, end - start))
        push_scan(start, end);
}

/* The vertices the GPU fetches for indices first .. first + vertices - 1 when
 * the title's pushed arrays are in force: every register of the selected
 * declaration read from its own array (address + index * stride) and laid
 * out at its declared offset, so the rest of the draw path sees an ordinary
 * stream 0 vertex. A register no push described stays zero. NULL (counted)
 * when there is no declaration to lay them out by. */
static uint8_t *push_gather(uint32_t first, uint32_t vertices, uint32_t *stride)
{
    uint32_t object = hle_d3d8_shadow_program_object(), extent = 0, i, v;
    uint8_t *out;

    if (!object || !vertices || !guest_readable(object, 20u + 16u * 16u))
        return NULL;
    for (i = 0; i < 16u; i++) {
        uint32_t attr = object + 20u + i * 16u, format = HLE_MEM32(attr + 8u);
        UINT size;

        if (format <= 0x02u)
            continue;
        if (!(size = vsdt_bytes(format)))
            return NULL;
        if (HLE_MEM32(attr + 4u) + size > extent)
            extent = HLE_MEM32(attr + 4u) + size;
    }
    if (!extent || extent > 256u)
        return NULL;
    out = calloc((size_t)vertices, extent);
    if (!out)
        return NULL;
    for (i = 0; i < 16u; i++) {
        uint32_t attr = object + 20u + i * 16u, format = HLE_MEM32(attr + 8u);
        uint32_t offset = HLE_MEM32(attr + 4u), pushed = g_push_fmt[i];
        uint32_t pstride = pushed >> 8, va = (g_push_off[i] & 0x7FFFFFFFu) | CONTIG_BASE;
        UINT size, psize;

        if (format <= 0x02u || !(g_push_known & (1u << i)) || (pushed & 0xFFu) <= 0x02u)
            continue;
        size = vsdt_bytes(format);
        psize = vsdt_bytes(pushed & 0xFFu);
        if (psize && psize < size)
            size = psize;
        if ((pushed & 0xFFu) != format) {
            static int said;
            if (said++ < 4)
                fprintf(stderr, "[HLE-D3D8] pushed vertex array %u is format 0x%02X, the "
                        "declaration says 0x%02X; read as the declaration\n",
                        i, pushed & 0xFFu, format);
        }
        if (!guest_readable(va, (uint64_t)(first + vertices - 1u) * pstride + size)) {
            g_push_failed++;
            continue;
        }
        for (v = 0; v < vertices; v++)
            memcpy(out + (size_t)v * extent + offset,
                   HLE_PTR(va + (first + v) * pstride), size);
    }
    *stride = extent;
    return out;
}

/* Title RAM or the contiguous window. */
static int guest_readable(uint32_t va, uint64_t bytes)
{
    if (va >= 0x00010000u && (uint64_t)va + bytes <= g_xbox_total_ram)
        return 1;
    return va >= CONTIG_BASE && (uint64_t)va + bytes <= (uint64_t)CONTIG_BASE + CONTIG_SIZE;
}

/* Host pointer to vertex `first` of the stream 0 buffer, for `vertices`
 * vertices, or NULL (counted) if there is no buffer or it would read outside
 * the contiguous window. */
static const void *stream0_vertices(uint32_t first, uint32_t vertices)
{
    uint32_t data, va;
    uint64_t start, bytes;

    if (!g_stream0_vb || !g_stream0_stride || !guest_readable(g_stream0_vb, 12u)) {
        g_skip_no_stream++;
        return NULL;
    }
    data = HLE_MEM32(g_stream0_vb + 4u);
    /* Where the title's own D3DVertexBuffer_Lock2 hands out the vertices. */
    va = data | CONTIG_BASE;
    start = (uint64_t)(va - CONTIG_BASE) + (uint64_t)first * g_stream0_stride;
    bytes = (uint64_t)vertices * g_stream0_stride;
    if (!data || start + bytes > CONTIG_SIZE) {
        g_skip_range++;
        return NULL;
    }
    return HLE_PTR(CONTIG_BASE + (uint32_t)start);
}

/* Host pointer to vertex `first` of any stream's buffer, for `vertices`
 * vertices, with its stride; NULL if there is none or it would read outside
 * the contiguous window. For the registers a program reads from streams other
 * than 0, found at the same index as the stream 0 vertex they go with. */
const void *hle_d3d8_stream_vertices(uint32_t stream, uint32_t first, uint32_t vertices,
                                     uint32_t *stride)
{
    uint32_t vb, data;
    uint64_t start, bytes;

    if (stream >= 16u)
        return NULL;
    vb = g_stream_vb[stream];
    *stride = g_stream_stride[stream];
    if (!vb || !*stride || !guest_readable(vb, 12u))
        return NULL;
    data = HLE_MEM32(vb + 4u);
    start = (uint64_t)((data | CONTIG_BASE) - CONTIG_BASE) + (uint64_t)first * *stride;
    bytes = (uint64_t)vertices * *stride;
    if (!data || start + bytes > CONTIG_SIZE)
        return NULL;
    return HLE_PTR(CONTIG_BASE + (uint32_t)start);
}

static void report(void)
{
    static DWORD last;
    DWORD now = GetTickCount();

    if (!last) {
        last = now;
    } else if (now - last >= 5000) {
        fprintf(stderr, "[HLE-D3D8] shadow buffers: skipped %lu with no stream 0 buffer, "
                "%lu out of range; %lu drawn from title-pushed arrays (%lu not)\n",
                g_skip_no_stream, g_skip_range, g_push_draws, g_push_failed);
        last = now;
    }
}
#endif /* _WIN32 */

HLE_ORIGINAL(D3DDevice_SetStreamSource);
HLE_ORIGINAL(CDevice_SetStateVB);
HLE_ORIGINAL(D3DDevice_DrawVertices);
HLE_ORIGINAL(D3DDevice_DrawIndexedVertices);
/* The one generic setter, on XDKs that predate the specialised forms. */
HLE_ORIGINAL(D3DDevice_SetVertexShaderConstant);
HLE_ORIGINAL(D3DDevice_SetVertexShaderConstant1);
HLE_ORIGINAL(D3DDevice_SetVertexShaderConstant4);
HLE_ORIGINAL(D3DDevice_SetVertexShaderConstantNotInline);
HLE_ORIGINAL(D3DDevice_SetVertexShaderConstantNotInlineFast);

static void first_call(int *seen, const char *name, uint32_t arg)
{
    if (!*seen) {
        *seen = 1;
        fprintf(stderr, "[HLE] %s(0x%X) replaced by name\n", name, arg);
        fflush(stderr);
    }
}

static int original_missing(void (*fn)(void), const char *name)
{
    if (fn)
        return 0;
    fprintf(stderr, "[HLE] %s: original body missing -- regenerate the lift\n", name);
    return 1;
}

/* void D3DDevice_SetStreamSource(UINT StreamNumber,
 *     D3DVertexBuffer *pStreamData, UINT Stride)                            */
HLE_EXPORT(D3DDevice_SetStreamSource)
{
    static int seen;
    uint32_t stream = HLE_ARG(0);
#ifdef _WIN32
    uint32_t vb = HLE_ARG(1), stride = HLE_ARG(2);
#endif

    first_call(&seen, "D3DDevice_SetStreamSource", stream);
    if (original_missing(hle_original_D3DDevice_SetStreamSource, "D3DDevice_SetStreamSource"))
        HLE_RETURN(0x80004005u);
    HLE_CALL_ORIGINAL(D3DDevice_SetStreamSource);
#ifdef _WIN32
    hle_d3d8_push_arrays_off();          /* the XDK writes its own arrays again */
    if (stream == 0u) {
        g_stream0_vb = vb;
        g_stream0_stride = stride;
    }
    if (stream < 16u) {
        g_stream_vb[stream] = vb;
        g_stream_stride[stream] = stride;
    }
#endif
}

/* void CDevice::SetStateVB(DWORD BaseVertexIndex) -- thiscall, D3D internal,
 * called by every DrawIndexedVertices. */
HLE_EXPORT(CDevice_SetStateVB)
{
    static int seen;
    uint32_t base = HLE_ARG(0);

    first_call(&seen, "CDevice_SetStateVB", base);
    if (original_missing(hle_original_CDevice_SetStateVB, "CDevice_SetStateVB"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(CDevice_SetStateVB);
#ifdef _WIN32
    g_base_vertex = base;
    g_base_vertex_seen = 1;
#endif
}

/* void D3DDevice_DrawVertices(D3DPRIMITIVETYPE PrimitiveType,
 *     UINT StartVertex, UINT VertexCount)                                   */
HLE_EXPORT(D3DDevice_DrawVertices)
{
    static int seen;
    uint32_t xpt = HLE_ARG(0);
#ifdef _WIN32
    uint32_t start = HLE_ARG(1), count = HLE_ARG(2);
#endif

    first_call(&seen, "D3DDevice_DrawVertices", xpt);
    if (original_missing(hle_original_D3DDevice_DrawVertices, "D3DDevice_DrawVertices"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_DrawVertices);
#ifdef _WIN32
    if (hle_d3d8_shadow_device() && count && g_push_active) {
        uint32_t stride;
        uint8_t *gathered = push_gather(start, count, &stride);
        if (gathered) {
            hle_d3d8_shadow_draw(xpt, count, gathered, stride, 0);
            free(gathered);
            g_push_draws++;
        } else {
            g_push_failed++;
        }
        report();
    } else if (hle_d3d8_shadow_device() && count) {
        const void *verts = stream0_vertices(start, count);
        if (verts) {
            hle_d3d8_shadow_set_first_vertex(start);
            hle_d3d8_shadow_draw(xpt, count, verts, g_stream0_stride, 1);
        }
        report();
    }
#endif
}

/* void D3DDevice_DrawIndexedVertices(D3DPRIMITIVETYPE PrimitiveType,
 *     UINT VertexCount, const WORD *pIndexData)                             */
HLE_EXPORT(D3DDevice_DrawIndexedVertices)
{
    static int seen;
    uint32_t xpt = HLE_ARG(0);
#ifdef _WIN32
    uint32_t count = HLE_ARG(1), index_va = HLE_ARG(2);
#endif

    first_call(&seen, "D3DDevice_DrawIndexedVertices", xpt);
    if (original_missing(hle_original_D3DDevice_DrawIndexedVertices,
                         "D3DDevice_DrawIndexedVertices"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_DrawIndexedVertices);
#ifdef _WIN32
    if (hle_d3d8_shadow_device() && count && index_va &&
        guest_readable(index_va, (uint64_t)count * 2u)) {
        const uint16_t *idx = (const uint16_t *)HLE_PTR(index_va);
        uint32_t i, vertices = 0;
        const void *verts;

        for (i = 0; i < count; i++)
            if ((uint32_t)idx[i] + 1u > vertices)
                vertices = (uint32_t)idx[i] + 1u;
        if (g_push_active) {
            /* The pushed arrays carry no base vertex: the XDK applies one by
             * rewriting its own offsets, which it has not done. Only the
             * range the indices use is gathered, rebased to zero. */
            uint32_t lo = 0xFFFFu, stride;
            uint16_t *rebased = malloc((size_t)count * sizeof *rebased);
            uint8_t *gathered = NULL;

            for (i = 0; i < count; i++)
                if (idx[i] < lo)
                    lo = idx[i];
            if (rebased)
                gathered = push_gather(lo, vertices - lo, &stride);
            if (gathered) {
                for (i = 0; i < count; i++)
                    rebased[i] = (uint16_t)(idx[i] - lo);
                hle_d3d8_shadow_draw_indexed(xpt, count, rebased, gathered, stride, 0);
                g_push_draws++;
            } else {
                g_push_failed++;
            }
            free(gathered);
            free(rebased);
            report();
            return;
        }
        if (!g_base_vertex_seen) {
            static int said;
            if (!said++)
                fprintf(stderr, "[HLE-D3D8] shadow buffers: CDevice_SetStateVB never "
                        "ran; indexed buffer draws use base vertex 0\n");
        }
        verts = stream0_vertices(g_base_vertex, vertices);
        if (verts) {
            hle_d3d8_shadow_set_first_vertex(g_base_vertex);
            hle_d3d8_shadow_draw_indexed(xpt, count, idx, verts, g_stream0_stride, 1);
        }
        report();
    }
#endif
}

#ifdef _WIN32
static void forward_constants(uint32_t reg, uint32_t data, uint32_t count)
{
    if (!hle_d3d8_shadow_device() || !data || !count || reg >= MAX_CONSTANT_REGISTERS)
        return;
    if (count > MAX_CONSTANT_REGISTERS - reg)
        count = MAX_CONSTANT_REGISTERS - reg;
    if (!guest_readable(data, (uint64_t)count * 16u))
        return;
    host_vsh_set_constant((int)reg, (const float *)HLE_PTR(data), (int)count);
}
#endif

/* void __fastcall D3DDevice_SetVertexShaderConstant1(int Register,
 *     const void *pConstantData) -- one register.                           */
HLE_EXPORT(D3DDevice_SetVertexShaderConstant1)
{
    static int seen;
    uint32_t reg = g_ecx;
#ifdef _WIN32
    uint32_t data = g_edx;
#endif

    first_call(&seen, "D3DDevice_SetVertexShaderConstant1", reg);
    if (original_missing(hle_original_D3DDevice_SetVertexShaderConstant1,
                         "D3DDevice_SetVertexShaderConstant1"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_SetVertexShaderConstant1);
#ifdef _WIN32
    forward_constants(reg, data, 1u);
#endif
}

/* void __fastcall D3DDevice_SetVertexShaderConstant1Fast(int Register,
 *     const void *pConstantData) -- one register, without the checks.
 *
 * Some builds have only this form of the single-register setter, beside
 * NotInlineFast: XGRA, Doom 3 and Breakdown name no other, and Outrun 2 has
 * both. Unreplaced, every constant a title set this way stayed zero on the
 * host, and a program that transforms its position by one drew nothing --
 * XGRA's movie quad reached the host every frame and came out black. */
HLE_ORIGINAL(D3DDevice_SetVertexShaderConstant1Fast);
HLE_EXPORT(D3DDevice_SetVertexShaderConstant1Fast)
{
    static int seen;
    uint32_t reg = g_ecx;
#ifdef _WIN32
    uint32_t data = g_edx;
#endif

    first_call(&seen, "D3DDevice_SetVertexShaderConstant1Fast", reg);
    if (original_missing(hle_original_D3DDevice_SetVertexShaderConstant1Fast,
                         "D3DDevice_SetVertexShaderConstant1Fast"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_SetVertexShaderConstant1Fast);
#ifdef _WIN32
    forward_constants(reg, data, 1u);
#endif
}

/* void __fastcall D3DDevice_SetVertexShaderConstant4(int Register,
 *     const void *pConstantData) -- four registers.                         */
HLE_EXPORT(D3DDevice_SetVertexShaderConstant4)
{
    static int seen;
    uint32_t reg = g_ecx;
#ifdef _WIN32
    uint32_t data = g_edx;
#endif

    first_call(&seen, "D3DDevice_SetVertexShaderConstant4", reg);
    if (original_missing(hle_original_D3DDevice_SetVertexShaderConstant4,
                         "D3DDevice_SetVertexShaderConstant4"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_SetVertexShaderConstant4);
#ifdef _WIN32
    forward_constants(reg, data, 4u);
#endif
}

/* void __fastcall D3DDevice_SetVertexShaderConstantNotInline(int Register,
 *     const void *pConstantData, DWORD ConstantCount) -- ConstantCount counts
 * floats (Cxbx-Reloaded divides it by 4). A count that is not a multiple of
 * four drops the last, partial register here; the XDK passes whole ones.
 *
 * Burnout 2's NotInline calls NotInlineFast (through its thunk), so the inner
 * call does not forward and this one does, once. */
HLE_EXPORT(D3DDevice_SetVertexShaderConstantNotInline)
{
    static int seen;
    uint32_t reg = g_ecx;
#ifdef _WIN32
    uint32_t data = g_edx, floats = HLE_ARG(0);
#endif

    first_call(&seen, "D3DDevice_SetVertexShaderConstantNotInline", reg);
    if (original_missing(hle_original_D3DDevice_SetVertexShaderConstantNotInline,
                         "D3DDevice_SetVertexShaderConstantNotInline"))
        HLE_RETURN(0u);
#ifdef _WIN32
    g_in_notinline++;
#endif
    HLE_CALL_ORIGINAL(D3DDevice_SetVertexShaderConstantNotInline);
#ifdef _WIN32
    g_in_notinline--;
    forward_constants(reg, data, floats / 4u);
#endif
}

/* The same, the XDK's faster variant.                                        */
HLE_EXPORT(D3DDevice_SetVertexShaderConstantNotInlineFast)
{
    static int seen;
    uint32_t reg = g_ecx;
#ifdef _WIN32
    uint32_t data = g_edx, floats = HLE_ARG(0);
#endif

    first_call(&seen, "D3DDevice_SetVertexShaderConstantNotInlineFast", reg);
    if (original_missing(hle_original_D3DDevice_SetVertexShaderConstantNotInlineFast,
                         "D3DDevice_SetVertexShaderConstantNotInlineFast"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_SetVertexShaderConstantNotInlineFast);
#ifdef _WIN32
    if (!g_in_notinline)
        forward_constants(reg, data, floats / 4u);
#endif
}

/* void __stdcall D3DDevice_SetVertexShaderConstant(INT Register,
 *     const void *pConstantData, DWORD ConstantCount)
 *
 * The one generic setter, on an XDK that predates the specialised forms.
 * Max Payne is XDK 3925: its D3D8 exports this and none of Constant1,
 * Constant4, NotInline or NotInlineFast, so a title there sets every vertex
 * shader constant through a function nothing replaced, and the host saw none
 * of them.
 *
 * ConstantCount counts registers here, not floats -- it is the plain D3D8
 * signature. NotInline's float count is the odd one out, not this.
 *
 * Register is -96-based and biased here, which the specialised forms are not.
 * 3925 starts `mov edx,[ebp+8]; add edx,0x60`, applying the bias itself, while
 * 4721's Constant4 writes its ecx straight into the push buffer and indexes
 * its shadow array with it unchanged. host_vsh_set_constant wants the same
 * 0-based index the specialised forms hand it, so the bias has to be added
 * here or every constant lands 96 registers low -- silently, because the
 * range check below would still pass for most of them.
 *
 * The arguments are logged for the first few calls: this has not yet been
 * seen to run (Max Payne reaches no vertex shader in the frames it renders),
 * so the log is the evidence that the reading above is right.
 */
#define XBOX_VSH_CONSTANT_BIAS 96u

HLE_EXPORT(D3DDevice_SetVertexShaderConstant)
{
    static int seen;
    uint32_t reg   = HLE_ARG(0) + XBOX_VSH_CONSTANT_BIAS;
    uint32_t data  = HLE_ARG(1);
    uint32_t count = HLE_ARG(2);

    first_call(&seen, "D3DDevice_SetVertexShaderConstant", reg);
    {
        static int notes;
        if (notes < 4) {
            notes++;
            fprintf(stderr, "[HLE-D3D8] SetVertexShaderConstant(reg=%d, data=0x%08X,"
                    " count=%u) -> host register %u; ecx=0x%08X edx=0x%08X\n",
                    (int)HLE_ARG(0), data, count, reg, g_ecx, g_edx);
            fflush(stderr);
        }
    }
    if (original_missing(hle_original_D3DDevice_SetVertexShaderConstant,
                         "D3DDevice_SetVertexShaderConstant"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_SetVertexShaderConstant);
#ifdef _WIN32
    forward_constants(reg, data, count);
#endif
}
