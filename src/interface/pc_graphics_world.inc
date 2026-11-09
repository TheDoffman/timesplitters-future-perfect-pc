/* The guest enters 1CE9A0 after world drawing, before its HUD/effects. */
#include "graphics/native_hud.h"
#include "pc_developer.h"
int pc_graphics_hud_begin(void)
{
    if(WORD_AT(0x0079094Cu)<=100u &&
       (guest_call(sub_001CDAE0,0)==1u || !pc_developer_hud_visible())) {
        guest_call(sub_00022FF0,0);
        host_SetTwoDPlacement(TSFP_NATIVE_HUD_BEGIN,TSFP_NATIVE_HUD_TAG);
        return 1;
    }
    return 0;
}
void pc_graphics_hud_end(void)
{
    guest_call(sub_00022FF0,0);
    host_SetTwoDPlacement(TSFP_NATIVE_HUD_END,TSFP_NATIVE_HUD_TAG);
}

void pc_graphics_world_finish(void (*original)(void))
{
    pc_developer_draw_body();
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
