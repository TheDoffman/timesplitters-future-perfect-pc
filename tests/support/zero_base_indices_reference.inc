/* Pre-optimization shared HLE draw driver: reference for index/dispatch parity. */
HLE_EXPORT(D3DDevice_DrawIndexedVertices)
{
    static int seen;
    uint32_t xpt = HLE_ARG(0);
#if defined(_WIN32) || defined(__APPLE__)
    uint32_t count = HLE_ARG(1), index_va = HLE_ARG(2);
#endif

    first_call(&seen, "D3DDevice_DrawIndexedVertices", xpt);
    if (original_missing(hle_original_D3DDevice_DrawIndexedVertices,
                         "D3DDevice_DrawIndexedVertices"))
        HLE_RETURN(0u);
    HLE_CALL_ORIGINAL(D3DDevice_DrawIndexedVertices);
#if defined(_WIN32) || defined(__APPLE__)
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
