#ifndef TSFP_WINDOW_STARTUP_SDL_H
#define TSFP_WINDOW_STARTUP_SDL_H
#include <SDL3/SDL.h>
#include "recomp_config.h"

/* Both native renderers consume the same persisted preference as the launcher
 * and Windows. Set it at creation, before focus and drawable size are observed. */
static SDL_WindowFlags tsfp_native_window_flags(SDL_WindowFlags flags)
{
    if (recomp_config_bool("RECOMP_FULLSCREEN", "fullscreen", 0))
        flags |= SDL_WINDOW_FULLSCREEN;
    return flags;
}
#endif
