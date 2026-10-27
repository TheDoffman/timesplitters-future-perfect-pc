#ifndef TSFP_PC_NATIVE_STATISTICS_GUEST_H
#define TSFP_PC_NATIVE_STATISTICS_GUEST_H
#include <stddef.h>
#include <stdint.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
/* D1FD0's time counters must share the carried integer clock and original
 * float-addition cadence. Its distance inputs are already computed by the
 * caller and continue to be recorded on every native update. */
static inline void pc_native_statistics_guest(void (*original)(void))
{
    if(!g_tsfp_native_timing_active) { original();return; }
    const uint32_t *ticks=(const uint32_t *)((uintptr_t)0x4E7944u+g_xbox_mem_offset);
    float *delta=(float *)((uintptr_t)0x4E794Cu+g_xbox_mem_offset);
    float saved=*delta;
    *delta=saved>0.f?(float)*ticks:0.f;
    original();
    *delta=saved;
}
#endif
