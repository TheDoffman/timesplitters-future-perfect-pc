#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "texture_mode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static volatile LONG enabled;

static BOOL CALLBACK init(PINIT_ONCE unused, PVOID parameter, PVOID *context)
{
    const char *mode = getenv("TSFP_TEXTURE_MODE");
    (void)unused; (void)parameter; (void)context;
    /* Preserve explicit pack-preview launches; ordinary launchers request classic. */
    const char *pack = getenv("TSFP_TEXTURE_PACK_DIR");
    enabled = mode ? !_stricmp(mode,"4k") : (pack && *pack);
    fprintf(stderr,"[TEXTURE-MODE] %s; F10 switches Classic / 4K. Missing replacements use Classic.\n",
            enabled ? "4K" : "Classic");
    return TRUE;
}

int tsfp_texture_is_4k(void)
{
    InitOnceExecuteOnce(&once,init,NULL,NULL);
    return (int)InterlockedCompareExchange(&enabled,0,0);
}

int tsfp_texture_toggle(void)
{
    LONG before;
    tsfp_texture_is_4k();
    do { before=InterlockedCompareExchange(&enabled,0,0); }
    while(InterlockedCompareExchange(&enabled,!before,before)!=before);
    fprintf(stderr,"[TEXTURE-MODE] %s; missing replacements use Classic.\n",before ? "Classic" : "4K");
    return !before;
}
