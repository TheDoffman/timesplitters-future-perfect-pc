/* Frozen effective pre-optimization body at90f79e93; public runtime HLE code. */
static uint8_t *reference(const void *verts, UINT vertices, UINT *stride,
                                       uint32_t first, int *failed)
{
    const struct shadow_program *p;
    const uint8_t *base[SHADOW_MAX_PACKED];
    uint32_t sstride[SHADOW_MAX_PACKED];
    UINT in_stride = *stride, out_stride, shift, v;
    uint8_t *out;
    int k;

    *failed = 0;
    if (g_ff_active) {
        /* Fixed function by declaration (ff_from_declaration): every vertex
         * rebuilt in FVF order from wherever its inputs are. */
        const uint8_t *src[FF_MAX_INPUTS];
        uint32_t sst[FF_MAX_INPUTS];

        for (k = 0; k < g_ff.count; k++) {
            if (!g_ff.stream[k]) {
                src[k] = (const uint8_t *)verts;
                sst[k] = in_stride;
                continue;
            }
            src[k] = first == NO_FIRST_VERTEX ? NULL
                   : (const uint8_t *)hle_d3d8_stream_vertices(g_ff.stream[k], first,
                                                               vertices, &sst[k]);
            if (!src[k] || sst[k] < g_ff.offset[k] + ff_format_size(g_ff.format[k])) {
                *failed = 1;
                return NULL;
            }
        }
        out_stride = g_ff.out_stride;
        out = calloc((size_t)vertices, out_stride);
        if (!out) {
            *failed = 1;
            return NULL;
        }
        for (v = 0; v < vertices; v++) {
            uint8_t *dst = out + (size_t)v * out_stride;
            for (k = 0; k < g_ff.count; k++) {
                const uint8_t *at = src[k] + (size_t)v * sst[k] + g_ff.offset[k];
                float f[4];
                int c = ff_read_floats(g_ff.format[k], at, f);

                if (g_ff.as_color[k]) {
                    if (g_ff.format[k] == 0x40) {
                        memcpy(dst + g_ff.out[k], at, 4);   /* already a D3DCOLOR */
                    } else {
                        uint8_t bgra[4];
                        float r = c > 0 ? f[0] : 0, g = c > 1 ? f[1] : 0;
                        float b = c > 2 ? f[2] : 0, a = c > 3 ? f[3] : 1;
#define FF_U8(x) (uint8_t)((x) <= 0 ? 0 : (x) >= 1 ? 255 : (int)((x) * 255.0f + 0.5f))
                        bgra[0] = FF_U8(b); bgra[1] = FF_U8(g);
                        bgra[2] = FF_U8(r); bgra[3] = FF_U8(a);
#undef FF_U8
                        memcpy(dst + g_ff.out[k], bgra, 4);
                    }
                } else {
                    memcpy(dst + g_ff.out[k], f, (size_t)g_ff.floats[k] * sizeof f[0]);
                }
            }
        }
        *stride = out_stride;
        return out;
    }
    if (!g_shadow_vs_is_program || g_shadow_vs_slot < 0)
        return NULL;
    p = &g_programs[g_shadow_vs_slot];
    if (!p->packed_count)
        return NULL;
    /* Where each register's vertices are: stream 0 is what the draw was
     * handed, any other stream is looked up at the same first vertex. A UP
     * draw has no other streams to read, so it cannot feed this program. */
    for (k = 0; k < p->packed_count; k++) {
        if (!p->packed_stream[k]) {
            base[k] = (const uint8_t *)verts;
            sstride[k] = in_stride;
            continue;
        }
        base[k] = first == NO_FIRST_VERTEX ? NULL
                : (const uint8_t *)hle_d3d8_stream_vertices(p->packed_stream[k], first,
                                                            vertices, &sstride[k]);
        if (!base[k] || sstride[k] < p->packed_offset[k] + p->packed_size[k]) {
            *failed = 1;
            return NULL;
        }
    }
    shift = p->expanded_bytes;
    /* Rounded up, so the host stride is a multiple of four even when the
     * title's is not (Dino Crisis 3's skinned vertex is 50 bytes). */
    out_stride = shift + ((in_stride + 3u) & ~3u);
    out = calloc((size_t)vertices, out_stride);
    if (!out) {
        *failed = 1;
        return NULL;
    }
    for (v = 0; v < vertices; v++) {
        const uint8_t *src = (const uint8_t *)verts + (size_t)v * in_stride;
        uint8_t *dst = out + (size_t)v * out_stride;

        for (k = 0; k < p->packed_count; k++) {
            const uint8_t *at = base[k] + (size_t)v * sstride[k] + p->packed_offset[k];
            float n[4];
            int c, count;

            if (p->packed_raw[k]) {
                memcpy(dst + p->packed_out[k], at, p->packed_size[k]);
                continue;
            }
            if (p->packed_format[k] == 0x16u) {      /* NORMPACKED3 */
                uint32_t bits;
                memcpy(&bits, at, sizeof bits);
                n[0] = (float)((int32_t)(bits << 21) >> 21) / 1023.0f;
                n[1] = (float)((int32_t)(bits << 10) >> 21) / 1023.0f;
                n[2] = (float)((int32_t)bits >> 22) / 511.0f;
                count = 3;
            } else {                                 /* SHORTn: the value itself */
                count = (int)(p->packed_format[k] >> 4);
                for (c = 0; c < count; c++) {
                    int16_t s;
                    memcpy(&s, at + 2 * c, sizeof s);
                    n[c] = (float)s;
                }
            }
            memcpy(dst + p->packed_out[k], n, (size_t)count * sizeof n[0]);
        }
        memcpy(dst + shift, src, in_stride);
    }
    *stride = out_stride;
    return out;
}
