#ifndef TSFP_PC_FEATURE_FLAGS_H
#define TSFP_PC_FEATURE_FLAGS_H
#include <string.h>
#ifndef TSFP_PC_VERSION
#define TSFP_PC_VERSION "development"
#endif
/* Shared build policy: unfinished Story networking is opt-in by release name. */
static inline int pc_story_coop_enabled(void)
{
    return strstr(TSFP_PC_VERSION,"story-coop")!=NULL;
}
#endif
