#ifndef TSFP_PC_TERMINAL_CONFIRM_H
#define TSFP_PC_TERMINAL_CONFIRM_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
extern void pc_terminal_confirm_buttons(void);

/* Original zero-tick calls inhibit held confirmation (notably across pause).
 * Native fractional updates are not a pause: leave that inhibition unchanged
 * until the next authored terminal tick can consume the press. */
static inline uint32_t pc_terminal_confirm_latch(uint32_t value)
{
    if(g_tsfp_native_timing_active) {
        float delta=*(const float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
        if(isfinite(delta) && delta>0.f &&
           !*(const uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset))
            return *(const uint32_t *)((uintptr_t)0x76C288u+g_xbox_mem_offset);
    }
    return value;
}
#endif
