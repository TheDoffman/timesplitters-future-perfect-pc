/* Test the real SDL window transaction with failure-injectable OS calls.
 * No real display, driver, settings or saves are used. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SDL3/SDL.h>
#include <assert.h>
#include <string.h>
#include "opengl_window_sdl.h"
struct SDL_Window {int w,h,x,y,destroyed;SDL_WindowFlags flags;const char *title;};
static SDL_Window old,staged,*owner,*settings_window;
static void *logical_token;
static int position_available,main_thread,step,fail_step,commit_calls,reject_commit,show_calls,raise_calls;
static SDL_DisplayMode mode;
static int primary_mode,primary_calls,fail_vulkan,fail_neutral,wayland;
const char *SDL_GetCurrentVideoDriver(void){return wayland?"wayland":"x11";}
static int advance(void){return ++step!=fail_step;}
bool SDL_IsMainThread(void){return main_thread!=0;}
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window *w){assert(w && !w->destroyed);return w->flags;}
bool SDL_GetWindowSize(SDL_Window *w,int *width,int *height){*width=w->w;*height=w->h;return true;}
bool SDL_GetWindowPosition(SDL_Window *w,int *x,int *y){*x=w->x;*y=w->y;return position_available!=0;}
const char *SDL_GetWindowTitle(SDL_Window *w){return w->title;}
bool SDL_GL_SetAttribute(SDL_GLAttr attribute,int value){
    if(attribute==SDL_GL_CONTEXT_MAJOR_VERSION)assert(value==4);
    if(attribute==SDL_GL_CONTEXT_MINOR_VERSION)assert(value==1);
    if(attribute==SDL_GL_CONTEXT_PROFILE_MASK)assert(value==SDL_GL_CONTEXT_PROFILE_CORE);
    return advance()!=0;
}
SDL_Window *SDL_CreateWindow(const char *title,int w,int h,SDL_WindowFlags flags){
    if(primary_mode) {
        primary_calls++;assert(!(flags&SDL_WINDOW_OPENGL));
        if((flags&SDL_WINDOW_VULKAN)?fail_vulkan:fail_neutral)return NULL;
        SDL_Window *result=(flags&SDL_WINDOW_VULKAN)?&old:&staged;
        *result=(SDL_Window){w,h,0,0,0,flags,title};return result;
    }
    assert(flags&SDL_WINDOW_OPENGL);assert(!(flags&SDL_WINDOW_VULKAN));
    if(!advance())return NULL;
    staged=(SDL_Window){w,h,0,0,0,flags,title};return &staged;
}
const SDL_DisplayMode *SDL_GetWindowFullscreenMode(SDL_Window *w){(void)w;return &mode;}
bool SDL_SetWindowPosition(SDL_Window *w,int x,int y){assert(!wayland);if(!advance())return false;w->x=x;w->y=y;return true;}
bool SDL_SetWindowFullscreenMode(SDL_Window *w,const SDL_DisplayMode *m){(void)w;assert(m==&mode);return advance()!=0;}
bool SDL_SetWindowFullscreen(SDL_Window *w,bool on){assert(on);if(!advance())return false;w->flags|=SDL_WINDOW_FULLSCREEN;return true;}
bool SDL_MaximizeWindow(SDL_Window *w){if(!advance())return false;w->flags|=SDL_WINDOW_MAXIMIZED;return true;}
bool SDL_MinimizeWindow(SDL_Window *w){if(!advance())return false;w->flags|=SDL_WINDOW_MINIMIZED;return true;}
bool SDL_ShowWindow(SDL_Window *w){if(!advance())return false;show_calls++;w->flags&=~SDL_WINDOW_HIDDEN;return true;}
bool SDL_RaiseWindow(SDL_Window *w){assert(w==&staged && old.destroyed);raise_calls++;return true;}
void SDL_DestroyWindow(SDL_Window *w){assert(w && !w->destroyed);w->destroyed=1;}
const char *SDL_GetError(void){return "injected SDL failure";}
static int commit(SDL_Window *before,SDL_Window *after){
    assert(before==owner && !old.destroyed && !staged.destroyed);commit_calls++;
    if(reject_commit)return 0;
    owner=settings_window=after;return 1;
}
static void reset(SDL_WindowFlags extra){
    old=(SDL_Window){960,540,123,456,0,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|extra,"Original title"};
    staged=(SDL_Window){0};owner=settings_window=&old;logical_token=&old;
    wayland=0;main_thread=position_available=1;step=fail_step=commit_calls=reject_commit=show_calls=raise_calls=0;
}
int main(void){
    reset(SDL_WINDOW_FULLSCREEN);
    assert(tsfp_native_gl_window(&old,commit)==&staged);
    assert(owner==&staged && settings_window==&staged && logical_token==&old);
    assert(old.destroyed && !staged.destroyed && commit_calls==1 && show_calls==1 && raise_calls==1);
    assert(staged.w==960 && staged.h==540 && staged.x==123 && staged.y==456);
    assert(staged.flags&SDL_WINDOW_FULLSCREEN);assert(staged.flags&SDL_WINDOW_RESIZABLE);
    assert(staged.flags&SDL_WINDOW_HIGH_PIXEL_DENSITY);assert(!strcmp(staged.title,"Original title"));
    int operations=step;
    for(int fail=1;fail<=operations;fail++){
        reset(SDL_WINDOW_FULLSCREEN);fail_step=fail;
        assert(!tsfp_native_gl_window(&old,commit));
        assert(!old.destroyed && owner==&old && settings_window==&old && logical_token==&old);
        assert(!commit_calls);if(staged.w)assert(staged.destroyed);
    }
    reset(0);reject_commit=1;assert(!tsfp_native_gl_window(&old,commit));
    assert(!old.destroyed && staged.destroyed && owner==&old && settings_window==&old && commit_calls==1);
    reset(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN);
    assert(tsfp_native_gl_window(&old,commit)==&staged);
    assert((staged.flags&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN))==(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN));
    assert(!show_calls && !raise_calls);
    /* Wayland may not expose window coordinates. Let the compositor place
     * the replacement rather than turning an unsupported query into failure. */
    reset(0);position_available=0;assert(tsfp_native_gl_window(&old,commit)==&staged);
    assert(staged.x==0 && staged.y==0 && old.destroyed);
    /* Cached Wayland coordinates do not authorize placing a normal window. */
    reset(0);wayland=1;assert(position_available);
    assert(tsfp_native_gl_window(&old,commit)==&staged);
    assert(staged.x==0 && staged.y==0 && old.destroyed && owner==&staged);
    reset(0);old.flags=SDL_WINDOW_OPENGL;assert(tsfp_native_gl_window(&old,commit)==&old);
    assert(!step && !commit_calls && !old.destroyed);
    reset(0);main_thread=0;assert(!tsfp_native_gl_window(&old,commit));assert(!step && !old.destroyed);
    reset(0);primary_mode=1;primary_calls=fail_vulkan=fail_neutral=0;
    SDL_WindowFlags flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIDDEN|SDL_WINDOW_HIGH_PIXEL_DENSITY;
    assert(tsfp_native_primary_window("Primary",640,480,flags)==&old);
    assert(primary_calls==1 && old.flags==(flags|SDL_WINDOW_VULKAN));
    fail_vulkan=1;primary_calls=0;
    assert(tsfp_native_primary_window("Primary",640,480,flags)==&staged);
    assert(primary_calls==2 && staged.flags==flags && staged.w==640 && staged.h==480);
    assert(!strcmp(staged.title,"Primary") && !commit_calls && !old.destroyed);
    fail_neutral=1;primary_calls=0;
    assert(!tsfp_native_primary_window("Primary",640,480,flags));assert(primary_calls==2);
    main_thread=0;primary_calls=0;
    assert(!tsfp_native_primary_window("Primary",640,480,flags));assert(!primary_calls);
    puts("PASS: SDL primary-window failure reaches shared startup without selecting a renderer");
    puts("PASS: SDL OpenGL window staging, preservation and rollback");return 0;
}
