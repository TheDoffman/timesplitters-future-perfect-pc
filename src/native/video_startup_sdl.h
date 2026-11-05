#ifndef TSFP_VIDEO_STARTUP_SDL_H
#define TSFP_VIDEO_STARTUP_SDL_H
#include <SDL3/SDL.h>
#include <stdio.h>

/* Run before guest/audio workers: the first SDL initialization establishes
 * thread ownership. Audio-first initialization can otherwise prevent the
 * actual main thread from creating a window on either Linux display backend. */
static int tsfp_native_video_startup(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[native-video] initialization failed: %s\n", SDL_GetError());
        return 0;
    }
    fprintf(stderr, "[native-video] display backend: %s\n", SDL_GetCurrentVideoDriver());
    return 1;
}
#endif
