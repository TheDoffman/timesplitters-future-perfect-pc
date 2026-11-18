/* Compile and execute the production legacy-GL swap adapter with a fake driver.
   The native Mac/SDL GPU acceptance remains a separate check. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "graphics/pc_vsync_settings.h"
typedef void *SDL_GLContext;
typedef int GLint;
#define GL_DRAW_FRAMEBUFFER_BINDING 1
#define GL_DRAW_FRAMEBUFFER 2
static struct {void *window;SDL_GLContext glctx;} g;
static unsigned g_present_interval;
static int enabled,sets,applied=-1,fail,restores,swaps;
int pc_vsync_enabled(void){return enabled;}
static bool SDL_GL_SetSwapInterval(int interval){sets++;applied=interval;return !fail;}
static const char *SDL_GetError(void){return "test failure";}
static void glGetIntegerv(int key,GLint *out){assert(key==GL_DRAW_FRAMEBUFFER_BINDING);*out=7;}
static void glBindFramebuffer(int key,int value){assert(key==GL_DRAW_FRAMEBUFFER);if(value==7)restores++;else assert(value==0);}
static bool SDL_GL_SwapWindow(void *window){assert(window==g.window);swaps++;return true;}
#include "native_swap_under_test.inc"
int main(void){
    g.window=(void *)(uintptr_t)3;g.glctx=(void *)(uintptr_t)1;
    assert(native_swap_window() && sets==1 && applied==0);
    assert(native_swap_window() && sets==1);
    enabled=1;assert(native_swap_window() && sets==2 && applied==1);
    enabled=0;assert(native_swap_window() && sets==3 && applied==0);
    g_present_interval=1;assert(native_swap_window() && sets==4 && applied==1);
    g.glctx=(void *)(uintptr_t)2;assert(native_swap_window() && sets==5 && applied==1);
    fail=1;g_present_interval=0;assert(native_swap_window() && sets==6);
    assert(native_swap_window() && sets==6); /* Do not flood/retry per frame. */
    fail=0;enabled=1;assert(native_swap_window() && sets==7 && applied==1);
    assert(swaps==restores && swaps==9);
    puts("PASS: production GL swap observes V-sync and context changes");return 0;
}
