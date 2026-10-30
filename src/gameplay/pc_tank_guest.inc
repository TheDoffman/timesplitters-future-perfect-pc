extern RECOMP_MANUAL_TLS uint32_t g_edx;
#include "pc_tank.h"
#include "pc_controls.h"
#include "pc_prompts.h"
static void pc_tank_driver(void (*original)(void))
{
    double sensitivity;int enabled,inverted;
    pc_controls_get_mouse(&sensitivity,&enabled,&inverted);
    PcTankDriverScope scope={0};
    if(enabled && pc_prompts_keyboard_active()) {
        PcGuestRegion region={0};
        uintptr_t current=pc_mounted_read(0x7b0c7c,g_xbox_mem_offset,4,&region);
        if(current)scope=pc_tank_driver_input(g_eax,g_edx,*(uint32_t*)current,g_xbox_mem_offset,&region);
    }
    original();
    pc_tank_driver_restore(scope);
}
