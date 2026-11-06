/* Real SDL regression: audio starts before the first game window. */
#include "video_startup_sdl.h"
#include <pthread.h>
static void *audio(void *unused) {
    (void)unused;
    return SDL_InitSubSystem(SDL_INIT_AUDIO) ? NULL : (void *)1;
}
int main(int argc, char **argv) {
    (void)argv;
    /* Omit startup in the negative control to reproduce the original race. */
    if (argc == 1 && !tsfp_native_video_startup()) return 2;
    pthread_t thread; void *result;
    if (pthread_create(&thread,NULL,audio,NULL) || pthread_join(thread,&result) || result) return 3;
    if (!SDL_IsMainThread()) { SDL_Quit(); return 4; }
    SDL_Window *window=SDL_CreateWindow("Native startup regression",320,240,0);
    if (!window) { SDL_Quit(); return 5; }
    SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
