#ifndef TSFP_PC_NATIVE_LIGHT_GUEST_H
#define TSFP_PC_NATIVE_LIGHT_GUEST_H
#include <stdint.h>
#include <stddef.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
#define LIGHT_WORD(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
#define LIGHT_FLOAT(a) (*(float *)((uintptr_t)(a)+g_xbox_mem_offset))
/* US MapMaker type-2 entity color preview. Random flicker (mode 3) is an
 * authored discrete decision. Its zero-whole-tick branch writes the dark
 * endpoint instead of retaining the previous color. Keep that color between
 * decisions and while paused, including its propagated model color. Other
 * modes, original-60 behavior and advancing whole-tick decisions stay exact.
 * No host cache: initialization, palette edits and slot reuse remain guest-owned. */
static void pc_native_light_preview_guest(void (*original)(void),uint32_t entity)
{
    if(g_tsfp_native_timing_active &&
       (!LIGHT_WORD(0x4E7944u) || LIGHT_FLOAT(0x4E794Cu)==0.0f)) {
        uint32_t index=LIGHT_WORD(entity+0x34u);
        uint32_t params=LIGHT_WORD(0x73D96Cu)+index*40u;
        if(LIGHT_WORD(params)==3u) { g_esp+=4u;return; }
    }
    original();
}
#undef LIGHT_WORD
#undef LIGHT_FLOAT
#endif
