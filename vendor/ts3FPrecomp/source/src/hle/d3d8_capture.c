/*
 * d3d8_capture.c -- reading and writing the frame capture container.
 *
 * The format is documented in d3d8_capture.h. This file is deliberately free
 * of Direct3D, Windows and guest-memory dependencies: it builds on POSIX as
 * well as Windows so tests/d3d8_capture round-trips it in the Linux CI job,
 * where none of the rest of shadow mode compiles.
 *
 * Both halves treat a capture as untrusted input. The writer is driven by a
 * running title and can be cut off mid-frame when that title crashes -- which
 * is often exactly the frame worth keeping -- so the reader validates every
 * chunk against the end of the buffer instead of trusting the header's count.
 */
#include "d3d8_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Chunks start on 4-byte boundaries so every payload struct, all of whose
 * members are 4 bytes wide, can be read in place with no unaligned access. */
#define CAP_ALIGN 4u

static size_t cap_pad(size_t n)
{
    return (CAP_ALIGN - (n & (CAP_ALIGN - 1u))) & (CAP_ALIGN - 1u);
}

/* ------------------------------------------------------------------ writer */

struct D3D8CapWriter {
    FILE    *f;
    uint32_t chunks;
    int      failed;
};

D3D8CapWriter *d3d8cap_create(const char *path, uint32_t frame,
                              uint32_t width, uint32_t height)
{
    D3D8CapWriter *w;
    D3D8CapHeader h;

    if (!path || !*path)
        return NULL;
    w = calloc(1, sizeof *w);
    if (!w)
        return NULL;
    w->f = fopen(path, "wb");
    if (!w->f) {
        free(w);
        return NULL;
    }

    memset(&h, 0, sizeof h);
    memcpy(h.magic, D3D8CAP_MAGIC, sizeof h.magic);
    h.version      = D3D8CAP_VERSION;
    h.header_bytes = (uint32_t)sizeof h;
    h.frame        = frame;
    h.width        = width;
    h.height       = height;
    h.chunk_count  = 0;             /* patched by d3d8cap_close */
    if (fwrite(&h, sizeof h, 1, w->f) != 1)
        w->failed = 1;
    return w;
}

static int cap_put(D3D8CapWriter *w, const void *p, size_t n)
{
    if (!n)
        return 0;
    if (!p || fwrite(p, 1, n, w->f) != n) {
        w->failed = 1;
        return -1;
    }
    return 0;
}

int d3d8cap_chunk(D3D8CapWriter *w, uint32_t type,
                  const void *p0, size_t n0,
                  const void *p1, size_t n1,
                  const void *p2, size_t n2)
{
    static const uint8_t zeros[CAP_ALIGN] = { 0 };
    uint32_t head[2];
    size_t total = n0 + n1 + n2;

    if (!w || w->failed)
        return -1;
    /* bytes is a uint32_t in the file; a single chunk larger than 4 GB would
     * be a bug in the caller rather than a real frame. */
    if (total > 0xFFFFFFFFu) {
        w->failed = 1;
        return -1;
    }
    head[0] = type;
    head[1] = (uint32_t)total;
    if (cap_put(w, head, sizeof head) ||
        cap_put(w, p0, n0) || cap_put(w, p1, n1) || cap_put(w, p2, n2) ||
        cap_put(w, zeros, cap_pad(total)))
        return -1;
    w->chunks++;
    return 0;
}

uint32_t d3d8cap_chunk_count(const D3D8CapWriter *w)
{
    return w ? w->chunks : 0;
}

int d3d8cap_close(D3D8CapWriter *w)
{
    int ok;

    uint32_t n;

    if (!w)
        return -1;
    /* Counted before the terminator goes out: END marks the end of the
     * stream and is not content, so chunk_count is the number of real chunks
     * and matches what a reader walks. */
    n = w->chunks;
    d3d8cap_chunk(w, D3D8CAP_END, NULL, 0, NULL, 0, NULL, 0);

    /* chunk_count is only known now. Seeking back to patch it keeps the
     * header fixed-size and the stream append-only, so a capture whose run
     * died before close is still readable -- the reader stops at the end of
     * the buffer, and chunk_count reads 0. */
    if (!w->failed) {
        if (fseek(w->f, (long)offsetof(D3D8CapHeader, chunk_count), SEEK_SET) != 0 ||
            fwrite(&n, sizeof n, 1, w->f) != 1)
            w->failed = 1;
    }
    ok = !w->failed;
    if (fclose(w->f) != 0)
        ok = 0;
    free(w);
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ reader */

struct D3D8CapReader {
    uint8_t *buf;
    size_t   bytes;
    size_t   pos;        /* offset of the next chunk */
};

static void cap_err(char *err, size_t n, const char *msg)
{
    if (err && n) {
        size_t len = strlen(msg);
        if (len >= n)
            len = n - 1;
        memcpy(err, msg, len);
        err[len] = '\0';
    }
}

D3D8CapReader *d3d8cap_open(const char *path, char *err, size_t err_bytes)
{
    D3D8CapReader *r;
    const D3D8CapHeader *h;
    FILE *f;
    long size;

    cap_err(err, err_bytes, "unknown error");
    if (!path || !*path) {
        cap_err(err, err_bytes, "no capture path given");
        return NULL;
    }
    f = fopen(path, "rb");
    if (!f) {
        cap_err(err, err_bytes, "cannot open the capture file");
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0) {
        cap_err(err, err_bytes, "cannot size the capture file");
        fclose(f);
        return NULL;
    }
    if ((size_t)size < sizeof(D3D8CapHeader)) {
        cap_err(err, err_bytes, "the file is shorter than a capture header");
        fclose(f);
        return NULL;
    }

    r = calloc(1, sizeof *r);
    if (!r) {
        cap_err(err, err_bytes, "out of memory");
        fclose(f);
        return NULL;
    }
    r->bytes = (size_t)size;
    r->buf = malloc(r->bytes);
    if (!r->buf || fread(r->buf, 1, r->bytes, f) != r->bytes) {
        cap_err(err, err_bytes, "cannot read the capture file");
        fclose(f);
        d3d8cap_close_read(r);
        return NULL;
    }
    fclose(f);

    h = (const D3D8CapHeader *)r->buf;
    if (memcmp(h->magic, D3D8CAP_MAGIC, sizeof h->magic) != 0) {
        cap_err(err, err_bytes, "not a D3D8 capture (bad magic)");
        d3d8cap_close_read(r);
        return NULL;
    }
    /* A range whose floor moves with every layout change (see
     * D3D8CAP_VERSION_MIN_READ): silently mis-reading an older layout would
     * show up as a corrupt image rather than an error, so anything below the
     * floor or newer than this build is refused. */
    if (h->version < D3D8CAP_VERSION_MIN_READ || h->version > D3D8CAP_VERSION) {
        cap_err(err, err_bytes, "capture version is not one this build reads");
        d3d8cap_close_read(r);
        return NULL;
    }
    if (h->header_bytes < sizeof(D3D8CapHeader) || h->header_bytes > r->bytes) {
        cap_err(err, err_bytes, "capture header length is out of range");
        d3d8cap_close_read(r);
        return NULL;
    }
    r->pos = h->header_bytes;
    return r;
}

const D3D8CapHeader *d3d8cap_header(const D3D8CapReader *r)
{
    return r ? (const D3D8CapHeader *)r->buf : NULL;
}

int d3d8cap_next(D3D8CapReader *r, D3D8CapChunk *out)
{
    const uint32_t *head;
    size_t bytes;

    if (!r || !out || r->pos + 8u > r->bytes)
        return 0;
    head = (const uint32_t *)(const void *)(r->buf + r->pos);
    bytes = head[1];
    /* The payload must lie inside the buffer. A truncated capture -- the run
     * died mid-frame -- ends the walk here instead of reading past the end. */
    if (bytes > r->bytes - r->pos - 8u)
        return 0;
    if (head[0] == D3D8CAP_END)
        return 0;

    out->type  = head[0];
    out->bytes = (uint32_t)bytes;
    out->data  = bytes ? (const void *)(r->buf + r->pos + 8u) : NULL;
    r->pos += 8u + bytes + cap_pad(bytes);
    return 1;
}

void d3d8cap_rewind(D3D8CapReader *r)
{
    if (r)
        r->pos = ((const D3D8CapHeader *)r->buf)->header_bytes;
}

void d3d8cap_close_read(D3D8CapReader *r)
{
    if (!r)
        return;
    free(r->buf);
    free(r);
}

const char *d3d8cap_chunk_name(uint32_t type)
{
    static const char *const names[D3D8CAP_CHUNK_KINDS] = {
        "end", "frame_start", "clear", "render_state", "texture_stage_state",
        "transform", "viewport", "set_texture", "set_vertex_shader", "draw_up",
        "draw_indexed_up", "texture", "texture_level", "texture_release",
        "vs_create", "vs_delete", "vs_declaration", "vs_constants",
        "vs_screenspace", "ps_token", "depth_surface", "set_render_target",
        "vs_vertex_data", "cube_texture", "scissors", "material", "light",
        "light_enable", "two_d_placement", "cube_level", "palette"
    };

    return type < D3D8CAP_CHUNK_KINDS ? names[type] : "unknown";
}

const void *d3d8cap_tail(const D3D8CapChunk *c, size_t head, size_t want)
{
    if (!c || !c->data || c->bytes < head || c->bytes - head < want)
        return NULL;
    return (const uint8_t *)c->data + head;
}
