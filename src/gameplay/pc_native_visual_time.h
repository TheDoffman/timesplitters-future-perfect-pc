#ifndef TSFP_PC_NATIVE_VISUAL_TIME_H
#define TSFP_PC_NATIVE_VISUAL_TIME_H
#include <stdint.h>
#include <math.h>
/* Retain the guest integer clock and track only its fractional difference.
 * An explicit reset/rebase clears this difference even if the value is the
 * same. No render callback advances time; several views sample one state. */
typedef struct PcNativeVisualTime {
    double offset;
    uint32_t integer;
    int valid;
} PcNativeVisualTime;
static inline void pc_native_visual_time_store(PcNativeVisualTime *clock,
    int enabled,int increment,uint32_t previous,uint32_t value,
    uint32_t whole,float delta)
{
    if(!enabled) { clock->valid=0;return; }
    if(!increment || value-previous!=whole || !isfinite(delta) || delta<0) {
        clock->offset=0;
    } else {
        if(!clock->valid || clock->integer!=previous)clock->offset=0;
        clock->offset+=(double)delta-(double)whole;
    }
    clock->integer=value;clock->valid=1;
}
static inline double pc_native_visual_time_sample(const PcNativeVisualTime *clock,
    uint32_t value)
{
    /* Preserve the original signed CVTSI2SS convention, including wrap.
     * An unobserved different-value write falls back to the guest clock. */
    return (double)(int32_t)value+
        (clock->valid && clock->integer==value ? clock->offset : 0);
}
#endif
