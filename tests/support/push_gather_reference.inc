static uint8_t *push_gather_reference(uint32_t first, uint32_t vertices, uint32_t *stride)
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
