#ifndef TSFP_GRAPHICS_OPTIONS_H
#define TSFP_GRAPHICS_OPTIONS_H

/* Wire format shared with the launcher. Zero-cost original rendering is the
 * default; shader caching only changes compilation, never rendered output. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

typedef struct TsfpGraphicsOptions {
    int msaa;             /* 1 (off), 2, 4, 8 samples */
    int edge_smoothing;   /* 0 off, 1 FXAA */
    int anisotropy;       /* 1 preserves title filtering; 2/4/8/16 override */
    int ambient_occlusion;/* 0 off, 1 half-resolution SSAO */
    int shader_cache;     /* 0 off, 1 on */
    int supersampling;    /* 0=100%, 1=125%, 2=150%, 3=200%; restart required */
} TsfpGraphicsOptions;

static inline TsfpGraphicsOptions tsfp_graphics_defaults(void)
{
    const TsfpGraphicsOptions options = {1, 0, 1, 0, 1, 0};
    return options;
}

static inline int tsfp_graphics_valid(const TsfpGraphicsOptions *o)
{
    return o && (o->msaa == 1 || o->msaa == 2 || o->msaa == 4 || o->msaa == 8) &&
        (o->edge_smoothing == 0 || o->edge_smoothing == 1) &&
        (o->anisotropy >= 1 && o->anisotropy <= 16) &&
        (o->ambient_occlusion == 0 || o->ambient_occlusion == 1) &&
        (o->shader_cache == 0 || o->shader_cache == 1) &&
        o->supersampling >= 0 && o->supersampling <= 3;
}

/* Invalid/truncated/future files leave the caller's current settings intact. */
static inline int tsfp_graphics_read(FILE *file, TsfpGraphicsOptions *out)
{
    char text[128], *cursor, *end;
    int values[6] = {0}, i, ch, count;
    TsfpGraphicsOptions candidate;
    if (!file || !out || !fgets(text, sizeof text, file)) return 0;
    cursor = text;
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (!strncmp(cursor, "TSFP_GRAPHICS_V1", 16)) count=5;
    else if (!strncmp(cursor, "TSFP_GRAPHICS_V2", 16)) count=6;
    else return 0;
    cursor += 16;
    for (i = 0; i < count; ++i) {
        long value;
        if (!isspace((unsigned char)*cursor)) return 0;
        errno = 0;
        value = strtol(cursor, &end, 10);
        if (end == cursor || errno || value < INT_MIN || value > INT_MAX) return 0;
        values[i] = (int)value;
        cursor = end;
    }
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (*cursor) return 0;
    while ((ch = fgetc(file)) != EOF) if (!isspace((unsigned char)ch)) return 0;
    if (ferror(file)) return 0;
    candidate.msaa = values[0]; candidate.edge_smoothing = values[1];
    candidate.anisotropy = values[2]; candidate.ambient_occlusion = values[3];
    candidate.shader_cache = values[4];
    candidate.supersampling = values[5];
    if (!tsfp_graphics_valid(&candidate)) return 0;
    *out = candidate;
    return 1;
}

static inline int tsfp_graphics_write(FILE *file, const TsfpGraphicsOptions *o)
{
    if(!file || !tsfp_graphics_valid(o))return 0;
    /* Keep default files readable by older builds and launchers. */
    if(!o->supersampling)return fprintf(file,
        "TSFP_GRAPHICS_V1 %d %d %d %d %d\n", o->msaa, o->edge_smoothing,
        o->anisotropy, o->ambient_occlusion, o->shader_cache)>0;
    return fprintf(file,"TSFP_GRAPHICS_V2 %d %d %d %d %d %d\n",
        o->msaa,o->edge_smoothing,o->anisotropy,o->ambient_occlusion,
        o->shader_cache,o->supersampling)>0;
}

/* Both attachments must support the selected sample count. Never silently
 * raise quality or rewrite the preference when running on a weaker device. */
static inline unsigned tsfp_graphics_samples(unsigned requested,
    unsigned color_mask, unsigned depth_mask)
{
    unsigned count = requested == 2 || requested == 4 || requested == 8 ? requested : 1;
    for (; count > 1; count /= 2)
        if ((color_mask & depth_mask & count) != 0) return count;
    return 1;
}
#endif
