/* Thin window adapter for the shared, single-threaded frame replay tool. */
#include <SDL3/SDL.h>
#include "rhi.h"

static SDL_Window *replay_sdl_window;
static int replay_quit;

static void pump(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            replay_quit = 1;
}

static void replay_pump(void *context) { (void)context; pump(); }

static int replay_size(void *context, void *window, uint32_t *width, uint32_t *height)
{
    int w = 0, h = 0;
    (void)context;
    if (!SDL_GetWindowSizeInPixels(window, &w, &h) || w <= 0 || h <= 0) return 0;
    *width = (uint32_t)w; *height = (uint32_t)h;
    return 1;
}

static HWND replay_window(UINT width, UINT height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return NULL;
    replay_sdl_window = SDL_CreateWindow("TimeSplitters frame replay", width, height,
                                         SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (replay_sdl_window) {
        RhiHost callbacks = {.pump_events = replay_pump, .drawable_size = replay_size};
        rhi_set_host(&callbacks);
    }
    return replay_sdl_window;
}

static void replay_show(HWND window, int hold)
{
    if (hold) SDL_ShowWindow(window);
}

static void replay_hold(void)
{
    while (!replay_quit) { pump(); SDL_Delay(10); }
}
