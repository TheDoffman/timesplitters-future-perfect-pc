/* Scope model-growth normalization to the status-3 scale constructor call.
 * The original rotation and matrix composition still execute each update. */
#ifndef TSFP_PC_NATIVE_CHARACTER_GUEST_H
#define TSFP_PC_NATIVE_CHARACTER_GUEST_H
#include <math.h>
#define CS_FLOAT(va) (*(float *)((uintptr_t)(va)+g_xbox_mem_offset))
extern void sub_00153600(void);
void pc_native_character_scale(void)
{
    uint32_t stack=g_esp;
    float raw=CS_FLOAT(0x4E7948),game=CS_FLOAT(0x4E794C);
    float saved[3];
    int split=g_tsfp_native_timing_active && raw>0.f && raw<1.f && game>0.f;
    if(split)for(unsigned axis=0;axis<3;axis++) {
        float factor=CS_FLOAT(stack+8+axis*4);
        saved[axis]=factor;
        /* The caller supplies 1 + coefficient * gameSeconds. Recover its
         * one-raw-tick factor at the current game speed before subdivision.
         * Nonpositive/nonfinite bases retain the authored arithmetic. */
        float full=1.f+(factor-1.f)/raw;
        if(full>0.f && isfinite(full))CS_FLOAT(stack+8+axis*4)=powf(full,raw);
    }
    sub_00153600();
    if(split)for(unsigned axis=0;axis<3;axis++)CS_FLOAT(stack+8+axis*4)=saved[axis];
}
#undef CS_FLOAT
#endif
