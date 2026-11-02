/* The guest enters 1CE9A0 after world drawing, before its HUD/effects. */
void pc_graphics_world_finish(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    if(pc_graphics_get().ambient_occlusion && WORD_AT(0x0079094Cu)<=100u) {
        PcGuestRegion region={0,0,0};
        uint32_t camera=WORD_AT(0x007B0CC0u);
        if(pc_display_writable(&region,g_xbox_mem_offset,camera,0x208)) {
            uint32_t matrix=WORD_AT(camera+0x204u);
            if(pc_display_writable(&region,g_xbox_mem_offset,matrix,64)) {
                extern void host_AmbientOcclusion(const float *);
                float projection[16];
                memcpy(projection,(void *)((uintptr_t)matrix+g_xbox_mem_offset),sizeof projection);
                host_AmbientOcclusion(projection);
            }
        }
    }
#endif
    original();
}
