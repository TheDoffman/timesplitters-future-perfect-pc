#include "texture_mode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static volatile LONG enabled;
static void mode_store(int value) { InterlockedExchange(&enabled,value); }
static int mode_load(void) { return (int)InterlockedCompareExchange(&enabled,0,0); }
static int mode_flip(void) {
    LONG before;
    do { before=InterlockedCompareExchange(&enabled,0,0); }
    while(InterlockedCompareExchange(&enabled,!before,before)!=before);
    return !before;
}
#define mode_equal _stricmp
#else
#include <pthread.h>
#include <stdatomic.h>
#include <strings.h>
static pthread_once_t once = PTHREAD_ONCE_INIT;
static atomic_int enabled;
static void mode_store(int value) { atomic_store(&enabled,value); }
static int mode_load(void) { return atomic_load(&enabled); }
static int mode_flip(void) { return !atomic_fetch_xor(&enabled,1); }
#define mode_equal strcasecmp
#endif

static void configure(void)
{
    const char *mode = getenv("TSFP_TEXTURE_MODE");
    /* Preserve explicit pack-preview launches; ordinary launchers request classic. */
    const char *pack = getenv("TSFP_TEXTURE_PACK_DIR");
    mode_store(mode ? !mode_equal(mode,"4k") : (pack && *pack));
    fprintf(stderr,"[TEXTURE-MODE] %s; missing replacements use Classic.\n",
            mode_load() ? "4K" : "Classic");
}

#ifdef _WIN32
static BOOL CALLBACK init(PINIT_ONCE unused, PVOID parameter, PVOID *context)
{
    (void)unused; (void)parameter; (void)context;
    configure();
    return TRUE;
}
#endif

int tsfp_texture_is_4k(void)
{
#ifdef _WIN32
    InitOnceExecuteOnce(&once,init,NULL,NULL);
#else
    pthread_once(&once,configure);
#endif
    return mode_load();
}

void tsfp_texture_set_4k(int value)
{
    tsfp_texture_is_4k();
    mode_store(!!value);
}

int tsfp_texture_toggle(void)
{
    int after;
    tsfp_texture_is_4k();
    after=mode_flip();
    fprintf(stderr,"[TEXTURE-MODE] %s; missing replacements use Classic.\n",after ? "4K" : "Classic");
    return after;
}
