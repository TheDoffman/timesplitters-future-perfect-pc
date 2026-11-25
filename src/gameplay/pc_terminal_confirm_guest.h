#ifndef TSFP_PC_TERMINAL_CONFIRM_GUEST_H
#define TSFP_PC_TERMINAL_CONFIRM_GUEST_H
#include "pc_terminal_confirm.h"
#include "pc_guest_span.h"
/* Only US3700EC: the terminal compares the entire held-button mask with the
 * operator's configured confirmation mask. Unrelated held inputs must not
 * hide confirmation or keep its release transition from reaching the UI.
 * The caller saves the operator at ESP+1C before pushing argument/return. */
static inline uint32_t pc_terminal_confirm_mask(uint32_t frame)
{
    PcGuestRegion region={0};
    uint32_t wanted=0;
    uintptr_t stack=(uintptr_t)frame+g_xbox_mem_offset;
    if(pc_guest_memory_in_span(&region,stack,0x28,0)) {
        uint32_t operator_address=*(const uint32_t *)(stack+0x24);
        uintptr_t player=(uintptr_t)operator_address+g_xbox_mem_offset;
        if(operator_address && pc_guest_memory_in_span(&region,player,0x24,0))
            wanted=*(const uint32_t *)(player+0x20);
    }
    return wanted;
}

extern void sub_00042760(void);
void pc_terminal_confirm_buttons(void)
{
    uint32_t wanted=pc_terminal_confirm_mask(g_esp);
    sub_00042760();
    if(wanted)g_eax&=wanted;
}
#endif
