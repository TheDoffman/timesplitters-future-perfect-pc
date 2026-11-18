#ifndef TSFP_PC_VSYNC_SETTINGS_H
#define TSFP_PC_VSYNC_SETTINGS_H
/* One preference for frontend/pause menus, the launcher and all renderers. */
int pc_vsync_enabled(void);
int pc_vsync_set_enabled(int enabled);
/* Preserve the caller's existing interval when the optional override is off.
 * In normal gameplay that interval is zero; side devices retain their policy. */
static inline unsigned pc_vsync_present_interval(unsigned interval)
{ return pc_vsync_enabled() ? 1u : interval; }

#ifdef PC_VSYNC_SETTINGS_IMPLEMENTATION
#include "../interface/pc_runtime_settings.h"
static volatile long pc_vsync_live=-1;
int pc_vsync_enabled(void)
{
#ifdef _WIN32
    long value=InterlockedCompareExchange(&pc_vsync_live,0,0);
#else
    long value=__atomic_load_n(&pc_vsync_live,__ATOMIC_ACQUIRE);
#endif
    if(value<0) {
        long initial=recomp_config_bool("TSFP_VSYNC","vsync",0)!=0;
#ifdef _WIN32
        value=InterlockedCompareExchange(&pc_vsync_live,initial,-1);
#else
        value=-1;
        __atomic_compare_exchange_n(&pc_vsync_live,&value,initial,0,__ATOMIC_RELEASE,__ATOMIC_ACQUIRE);
#endif
        if(value<0)value=initial;
    }
    return value!=0;
}
int pc_vsync_set_enabled(int enabled)
{
    if(!pc_runtime_bool_save("vsync",enabled))return 0;
#ifdef _WIN32
    InterlockedExchange(&pc_vsync_live,enabled);
#else
    __atomic_store_n(&pc_vsync_live,enabled,__ATOMIC_RELEASE);
#endif
    return 1;
}
#endif
#endif
