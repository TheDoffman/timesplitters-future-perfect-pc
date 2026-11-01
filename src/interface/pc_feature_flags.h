#ifndef TSFP_PC_FEATURE_FLAGS_H
#define TSFP_PC_FEATURE_FLAGS_H
#include <string.h>
#include "../diagnostics/pc_build_version.h"
/* Shared build policy: unfinished Story networking is opt-in by release name. */
static inline int pc_story_coop_enabled(void)
{
    return strstr(pc_build_version(),"story-coop")!=NULL;
}
#endif
