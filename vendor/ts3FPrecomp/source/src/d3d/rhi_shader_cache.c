/*
 * rhi_shader_cache.c - compiled shaders kept on disk between runs
 *
 * Every shader the renderer makes is HLSL generated at run time and compiled
 * when a draw first needs it: D3DCompile for D3D11, DXC to SPIR-V for
 * Vulkan. Each compile is a hitch of tens of milliseconds on the thread that
 * draws -- OutRun 2 pays about 45 of them a run, ~50 ms each, and pays them
 * again every run, because the in-memory caches are thrown away at exit.
 *
 * So the compiled blob is kept, one file per shader, under
 *   <user dir>\shadercache\<title id>\<backend>\<key>.bin
 * (%APPDATA%\xboxrecomp on Windows). The key is a hash of everything that
 * went into the compile: the HLSL, its macros, entry point, profile and
 * optimisation flag, plus a backend tag that names the compile settings. A
 * change to the generator changes the HLSL and so the key; nothing has to be
 * invalidated by hand. Each file also holds that key text in full and a load
 * compares it byte for byte, so a hash collision reads as a miss, never as
 * the wrong shader.
 *
 * Files are written to a temporary name and renamed into place, since
 * shaders compile on whichever guest thread draws. RECOMP_SHADER_CACHE=0
 * turns it off.
 */

#include "rhi.h"
#include "rhi_shader_cache.h"
#include "recomp_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  include <windows.h>
#endif

#define CACHE_MAGIC   0x43535258u   /* "XRSC" */
#define CACHE_VERSION 1u

/* Everything the compile depended on, as one byte string. */
typedef struct {
    char  *p;
    size_t n, cap;
} KeyText;

static int key_add(KeyText *k, const void *data, size_t n)
{
    if (k->n + n + 1 > k->cap) {
        size_t cap = k->cap ? k->cap * 2 : 4096;
        char *p;

        while (cap < k->n + n + 1)
            cap *= 2;
        p = (char *)realloc(k->p, cap);
        if (!p)
            return 0;
        k->p = p;
        k->cap = cap;
    }
    memcpy(k->p + k->n, data, n);
    k->n += n;
    k->p[k->n++] = 0;               /* a separator no field can contain */
    return 1;
}

static int key_add_str(KeyText *k, const char *s)
{
    return key_add(k, s ? s : "", s ? strlen(s) : 0);
}

static int key_build(KeyText *k, const RhiShaderSource *src, const char *backend)
{
    char opt[8];
    const RhiMacro *m;

    memset(k, 0, sizeof *k);
    snprintf(opt, sizeof opt, "%u", src->optimize ? 1u : 0u);
    if (!key_add_str(k, backend) || !key_add_str(k, src->entry) ||
        !key_add_str(k, src->target) || !key_add_str(k, opt))
        return 0;
    for (m = src->macros; m && m->name; m++)
        if (!key_add_str(k, m->name) || !key_add_str(k, m->value))
            return 0;
    return key_add(k, src->hlsl, src->len);
}

static uint64_t fnv64(const char *p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    size_t i;

    for (i = 0; i < n; i++) {
        h ^= (unsigned char)p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

static int cache_enabled(void)
{
    static int on = -1;

    if (on < 0) {
        const char *v = getenv("RECOMP_SHADER_CACHE");
        on = !(v && (strcmp(v, "0") == 0 || _stricmp(v, "off") == 0));
    }
    return on;
}

#if defined(_WIN32)

/* The directory for this backend, made on first use. 0 if there is none. */
static int cache_dir(const char *backend, char *out, size_t n)
{
    char base[600];
    int len;

    if (!recomp_config_user_dir(base, sizeof base))
        return 0;
    CreateDirectoryA(base, NULL);
    len = snprintf(out, n, "%s\\shadercache", base);
    if (len <= 0 || (size_t)len >= n)
        return 0;
    CreateDirectoryA(out, NULL);
    len = snprintf(out, n, "%s\\shadercache\\%08X", base,
                   (unsigned)recomp_config_title_id());
    if (len <= 0 || (size_t)len >= n)
        return 0;
    CreateDirectoryA(out, NULL);
    len = snprintf(out, n, "%s\\shadercache\\%08X\\%s", base,
                   (unsigned)recomp_config_title_id(), backend);
    if (len <= 0 || (size_t)len >= n)
        return 0;
    CreateDirectoryA(out, NULL);
    return 1;
}

static int cache_path(const char *backend, const KeyText *k, char *out, size_t n)
{
    char dir[800];
    int len;

    if (!cache_dir(backend, dir, sizeof dir))
        return 0;
    len = snprintf(out, n, "%s\\%016llX.bin", dir,
                   (unsigned long long)fnv64(k->p, k->n));
    return len > 0 && (size_t)len < n;
}

static volatile LONG g_hits, g_misses, g_writes;

static void note(const char *what, LONG n)
{
    /* 1, 10, 100, ... so a run says the cache is working without a line a
     * shader. */
    LONG p = 1;

    while (p < n && p < 1000000)
        p *= 10;
    if (p == n) {
        fprintf(stderr, "[SHADER-CACHE] %s: %ld\n", what, (long)n);
        fflush(stderr);
    }
}

int rhi_shader_cache_get(const RhiShaderSource *src, const char *backend,
                         void **blob, size_t *bytes)
{
    KeyText k;
    char path[1024];
    FILE *f = NULL;
    uint32_t hdr[4];
    char *stored = NULL;
    void *data = NULL;
    int ok = 0;

    *blob = NULL;
    *bytes = 0;
    if (!cache_enabled() || !src || !src->hlsl)
        return 0;
    if (!key_build(&k, src, backend))
        goto done;
    if (!cache_path(backend, &k, path, sizeof path))
        goto done;
    f = fopen(path, "rb");
    if (!f)
        goto done;
    if (fread(hdr, sizeof hdr, 1, f) != 1 || hdr[0] != CACHE_MAGIC ||
        hdr[1] != CACHE_VERSION || hdr[2] != (uint32_t)k.n || !hdr[3] ||
        hdr[3] > 64u * 1024u * 1024u)
        goto done;
    stored = (char *)malloc(k.n);
    data = malloc(hdr[3]);
    if (!stored || !data || fread(stored, 1, k.n, f) != k.n ||
        memcmp(stored, k.p, k.n) != 0 || fread(data, 1, hdr[3], f) != hdr[3])
        goto done;
    *blob = data;
    *bytes = hdr[3];
    data = NULL;
    ok = 1;
done:
    if (f)
        fclose(f);
    free(stored);
    free(data);
    free(k.p);
    if (cache_enabled())
        note(ok ? "loaded from disk" : "not on disk, compiling",
             InterlockedIncrement(ok ? &g_hits : &g_misses));
    return ok;
}

void rhi_shader_cache_put(const RhiShaderSource *src, const char *backend,
                          const void *blob, size_t bytes)
{
    KeyText k;
    char path[1024], tmp[1100];
    FILE *f;
    uint32_t hdr[4];
    int ok;

    if (!cache_enabled() || !src || !src->hlsl || !blob || !bytes)
        return;
    if (!key_build(&k, src, backend)) {
        free(k.p);
        return;
    }
    if (!cache_path(backend, &k, path, sizeof path)) {
        free(k.p);
        return;
    }
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)GetCurrentThreadId());
    f = fopen(tmp, "wb");
    if (!f) {
        free(k.p);
        return;
    }
    hdr[0] = CACHE_MAGIC;
    hdr[1] = CACHE_VERSION;
    hdr[2] = (uint32_t)k.n;
    hdr[3] = (uint32_t)bytes;
    ok = fwrite(hdr, sizeof hdr, 1, f) == 1 && fwrite(k.p, 1, k.n, f) == k.n &&
         fwrite(blob, 1, bytes, f) == bytes;
    ok = (fclose(f) == 0) && ok;
    if (!ok || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING))
        DeleteFileA(tmp);
    else
        note("written to disk", InterlockedIncrement(&g_writes));
    free(k.p);
}

int rhi_cache_file_path(const char *backend, const char *name, char *out, size_t n)
{
    char dir[800];
    int len;

    if (!cache_enabled() || !cache_dir(backend, dir, sizeof dir))
        return 0;
    len = snprintf(out, n, "%s\\%s", dir, name);
    return len > 0 && (size_t)len < n;
}

#else  /* not Windows: no cache */

int rhi_shader_cache_get(const RhiShaderSource *src, const char *backend,
                         void **blob, size_t *bytes)
{
    (void)src; (void)backend;
    *blob = NULL;
    *bytes = 0;
    return 0;
}

void rhi_shader_cache_put(const RhiShaderSource *src, const char *backend,
                          const void *blob, size_t bytes)
{
    (void)src; (void)backend; (void)blob; (void)bytes;
}

int rhi_cache_file_path(const char *backend, const char *name, char *out, size_t n)
{
    (void)backend; (void)name; (void)out; (void)n;
    return 0;
}

#endif
