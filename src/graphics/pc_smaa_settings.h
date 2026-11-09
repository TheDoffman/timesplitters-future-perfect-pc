#ifndef TSFP_PC_SMAA_SETTINGS_H
#define TSFP_PC_SMAA_SETTINGS_H
/* One model for the renderer and both frontend/pause video menus. */
int pc_smaa_enabled(void);
int pc_smaa_set_enabled(int enabled); /* Persist first, then publish atomically. */

#ifdef PC_SMAA_SETTINGS_IMPLEMENTATION
#include "../interface/pc_runtime_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static volatile long pc_smaa_live=-1;
int pc_smaa_enabled(void)
{
#ifdef _WIN32
    long value=InterlockedCompareExchange(&pc_smaa_live,0,0);
#else
    long value=__atomic_load_n(&pc_smaa_live,__ATOMIC_ACQUIRE);
#endif
    return value>=0 ? value!=0 : recomp_config_bool("TSFP_SMAA","smaa",0);
}
int pc_smaa_set_enabled(int enabled)
{
    if(!pc_runtime_bool_save("smaa",enabled))return 0;
#ifdef _WIN32
    InterlockedExchange(&pc_smaa_live,enabled);
#else
    __atomic_store_n(&pc_smaa_live,enabled,__ATOMIC_RELEASE);
#endif
    return 1;
}
#endif
#endif
