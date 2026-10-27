/**
 * The renderer's HLSL, seen on its way to whichever compiler a backend uses.
 *
 * Every shader the renderer makes -- the combiner and vertex-program
 * generators, the fixed-function pair, and the overlay, screen copy, movie
 * and display-resolve passes -- is HLSL, and every backend compiles that
 * same HLSL: D3D11 with D3DCompile, a Vulkan backend with DXC to SPIR-V
 * (docs/technical/vulkan-backend.md, section 4.3). Each backend's compile
 * calls d3d8_hlsl_note first, so what is noted here is the same whichever
 * backend is running.
 *
 * RECOMP_D3D8_HLSL_DIR=<dir> writes every distinct source into that
 * directory, once each, as a file that compiles on its own: the entry point
 * and target are in its first line and any macros are written out as
 * #defines. Replaying a capture with it set collects a title's whole shader
 * set without running the title, which is the corpus for testing the
 * generators against a different compiler.
 *
 * The compile timing below (d3d8_compile_clock / d3d8_compile_note) is the
 * "[D3D8] runtime shader compiles so far" report.
 */

#include "d3d8_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FNV-1a over the source, the macros, the entry point and the target: the
 * same text under a different target is a different shader. */
static uint32_t hlsl_hash(const char *src, size_t len, const RhiMacro *macros,
                          const char *entry, const char *target)
{
    uint32_t h = 2166136261u;
    size_t i;

#define MIX(p, n) for (i = 0; i < (n); i++) { h ^= (uint8_t)(p)[i]; h *= 16777619u; }
    MIX(src, len);
    for (; macros && macros->name; macros++) {
        MIX(macros->name, strlen(macros->name) + 1);
        if (macros->value)
            MIX(macros->value, strlen(macros->value) + 1);
    }
    MIX(entry, strlen(entry) + 1);
    MIX(target, strlen(target) + 1);
#undef MIX
    return h;
}

/* Hashes already written. A title produces a few hundred distinct shaders,
 * so a full table just stops recording rather than growing. Zero marks an
 * empty slot; a source that hashes to zero is written every time, which is
 * harmless. */
#define HLSL_SEEN_SLOTS 4096u
static volatile LONG g_hlsl_seen[HLSL_SEEN_SLOTS];

static int hlsl_first_sighting(uint32_t hash)
{
    uint32_t i, slot = hash % HLSL_SEEN_SLOTS;

    if (!hash)
        return 1;
    for (i = 0; i < HLSL_SEEN_SLOTS; i++, slot = (slot + 1) % HLSL_SEEN_SLOTS) {
        LONG have = InterlockedCompareExchange(&g_hlsl_seen[slot], (LONG)hash, 0);
        if (have == 0)
            return 1;
        if (have == (LONG)hash)
            return 0;
    }
    return 0;
}

static const char *hlsl_dump_dir(void)
{
    static int checked;
    static const char *dir;

    if (!checked) {
        const char *v = getenv("RECOMP_D3D8_HLSL_DIR");
        dir = (v && *v) ? v : NULL;
        if (dir) {
            CreateDirectoryA(dir, NULL);
            fprintf(stderr, "[D3D8] writing every compiled HLSL source to %s\n", dir);
        }
        checked = 1;
    }
    return dir;
}

void d3d8_hlsl_note(const char *src, size_t len, const char *name,
                    const RhiMacro *macros, const char *entry, const char *target)
{
    const char *dir = hlsl_dump_dir();
    uint32_t hash;
    char path[MAX_PATH];
    FILE *f;

    if (!dir)
        return;
    hash = hlsl_hash(src, len, macros, entry, target);
    if (!hlsl_first_sighting(hash))
        return;
    snprintf(path, sizeof path, "%s\\%s-%s-%s-%08X.hlsl",
             dir, name ? name : "shader", entry, target, hash);
    f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "// entry=%s target=%s name=%s\n", entry, target, name ? name : "");
    for (; macros && macros->name; macros++)
        fprintf(f, "#define %s %s\n", macros->name, macros->value ? macros->value : "");
    fwrite(src, 1, len, f);
    fclose(f);
}

/* Runtime compile timing (d3d8_internal.h). */
static volatile LONG     g_compiles[3];
static volatile LONG64   g_compile_ticks[3];
static LONG64            g_compile_qpf, g_compile_last_report;

long long d3d8_compile_clock(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void d3d8_compile_note(int kind, long long started)
{
    LARGE_INTEGER now;

    if (kind < 0 || kind > 2)
        return;
    QueryPerformanceCounter(&now);
    if (!g_compile_qpf) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_compile_qpf = f.QuadPart;
        g_compile_last_report = now.QuadPart;
    }
    InterlockedIncrement(&g_compiles[kind]);
    InterlockedAdd64(&g_compile_ticks[kind], now.QuadPart - started);
    if (now.QuadPart - g_compile_last_report >= 5 * g_compile_qpf) {
        g_compile_last_report = now.QuadPart;
        fprintf(stderr, "[D3D8] runtime shader compiles so far: combiner %ld (%.0f ms), "
                "fixed-function %ld (%.0f ms), vertex program %ld (%.0f ms)\n",
                g_compiles[0], g_compile_ticks[0] * 1000.0 / g_compile_qpf,
                g_compiles[1], g_compile_ticks[1] * 1000.0 / g_compile_qpf,
                g_compiles[2], g_compile_ticks[2] * 1000.0 / g_compile_qpf);
        fflush(stderr);
    }
}
