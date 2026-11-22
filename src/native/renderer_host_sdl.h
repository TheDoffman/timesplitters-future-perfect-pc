#ifndef TSFP_RENDERER_HOST_SDL_H
#define TSFP_RENDERER_HOST_SDL_H
#include <SDL3/SDL.h>
/* Call on the main thread before rendering starts. Unbind with NULL after
 * rendering stops and before destroying the window. */
void tsfp_renderer_host_bind(SDL_Window *window, void (*on_quit)(void));
/* Main-thread startup recovery only, after Vulkan resources are destroyed.
 * Keep the original RHI token valid for drawable-size queries. */
int tsfp_renderer_host_replace_window(SDL_Window *old_window,SDL_Window *new_window);
/* Common RHI adapter only; read from the SDL owner while dispatching work. */
unsigned tsfp_renderer_host_dispatch_serial(void);
#endif
