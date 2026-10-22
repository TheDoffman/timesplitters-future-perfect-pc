#ifndef TSFP_PC_NATIVE_RATE_H
#define TSFP_PC_NATIVE_RATE_H
#include "recomp_config.h"
#include <stdlib.h>
/* Explicit title-specific opt-in, independent of retired presentation options.
 * Legacy frame_target remains fixed at zero. Never change the console ISR. */
static inline int pc_native_rate(void)
{
    const char *value = recomp_config_lookup("TSFP_NATIVE_FPS", "native_fps");
    char *end;
    long rate;
    if (!value || !*value) return 0;
    rate = strtol(value, &end, 10);
    if (*end) return 0;
    return rate == 60 || rate == 120 || rate == 144 ? (int)rate : 0;
}
#endif
