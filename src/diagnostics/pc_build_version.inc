#include "pc_build_version.h"
#ifndef TSFP_PC_VERSION
#define TSFP_PC_VERSION "development"
#endif
const char *pc_build_version(void)
{
    return TSFP_PC_VERSION;
}
