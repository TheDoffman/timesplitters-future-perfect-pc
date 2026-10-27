#ifndef TSFP_PC_DEBUG_OUTPUT_H
#define TSFP_PC_DEBUG_OUTPUT_H
#include <stdlib.h>
#include <string.h>

/* Per-process diagnostic experiment; never persisted in personal settings. */
static inline int pc_debug_output_quiet(void)
{
    const char *value = getenv("TSFP_DEBUG_QUIET");
    return value && !strcmp(value, "1");
}
#endif
