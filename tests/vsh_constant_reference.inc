/* Frozen pre-change common d3d8_vsh.c setter and upload block.
 * Runtime shader/host services are controlled by the fixture. */
void d3d8_vsh_set_constant(int start_reg, const float *data, int count)
{
    int end_reg;
    float hp;

    if (!data || start_reg < 0)
        return;

    end_reg = start_reg + count;
    if (end_reg > NV2A_VS_MAX_CONSTANTS)
        end_reg = NV2A_VS_MAX_CONSTANTS;

    for (int i = start_reg; i < end_reg; i++) {
        int src_offset = (i - start_reg) * 4;
        g_vsh_constants.c[i][0] = data[src_offset + 0];
        g_vsh_constants.c[i][1] = data[src_offset + 1];
        g_vsh_constants.c[i][2] = data[src_offset + 2];
        g_vsh_constants.c[i][3] = data[src_offset + 3];
    }

    hp = hor_plus_factor();
    if (hp != 1.0f && !g_hor_plus_claimed) {
        int reg = d3d8_vsh_hor_plus_reg();

        if (reg >= start_reg && reg < end_reg) {
            g_vsh_constants.c[reg][0] *= hp;
            g_vsh_constants.c[reg][1] *= hp;
            g_vsh_constants.c[reg][2] *= hp;
            g_vsh_constants.c[reg][3] *= hp;
        }
    }

    g_vsh_constants_dirty = TRUE;
}

/* REFERENCE_UPLOAD */
    /* Update constant buffer if dirty, or if the camera zoom starts or stops
     * applying (camera_zoom: screen draws only). */
    {
        static BOOL zoomed;
        float zoom = camera_zoom();
        BOOL want = zoom != 1.0f && d3d8_target_is_screen();

        if (want != zoomed)
            g_vsh_constants_dirty = TRUE;
        if (g_vsh_constants_dirty) {
            mapped = rhi_buffer_map(g_vsh_cb, RHI_MAP_WRITE_DISCARD);
            if (mapped) {
                memcpy(mapped, &g_vsh_constants, sizeof(g_vsh_constants));
                if (want) {
                    int reg = d3d8_vsh_hor_plus_reg(), k;
                    float (*c)[4] = ((NV2AVSConstants *)mapped)->c;

                    if (reg + 1 < NV2A_VS_MAX_CONSTANTS)
                        for (k = 0; k < 4; k++) {
                            c[reg][k] *= zoom;
                            c[reg + 1][k] *= zoom;
                        }
                }
                rhi_buffer_unmap(g_vsh_cb);
            }
            g_vsh_constants_dirty = FALSE;
            zoomed = want;
        }
    }
