#ifndef TSFP_OPENGL_WINDOW_SDL_H
#define TSFP_OPENGL_WINDOW_SDL_H
#include <SDL3/SDL.h>
#include <stdio.h>
/* Called only after failed Vulkan resources have been destroyed. Stage a new
 * drawable without touching the original or saved configuration. The owner
 * commits event/input/settings bindings once all required window operations
 * succeed; failure destroys only the staged window. */
static SDL_Window *tsfp_native_gl_window(SDL_Window *old,
                                        int (*commit)(SDL_Window *,SDL_Window *))
{
    if(!SDL_IsMainThread() || !old || !commit)return NULL;
    if(SDL_GetWindowFlags(old)&SDL_WINDOW_OPENGL)return old;
    int width=0,height=0,x=SDL_WINDOWPOS_UNDEFINED,y=SDL_WINDOWPOS_UNDEFINED;
    if(!SDL_GetWindowSize(old,&width,&height) || width<=0 || height<=0)return NULL;
    int positioned=SDL_GetWindowPosition(old,&x,&y);
    SDL_WindowFlags previous=SDL_GetWindowFlags(old);
    SDL_WindowFlags flags=SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN|
        (previous&(SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_BORDERLESS));
    SDL_Window *next=NULL;
    if(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4) ||
       !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1) ||
       !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE) ||
       !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1) ||
       !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24) || !SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8))goto failed;
    next=SDL_CreateWindow(SDL_GetWindowTitle(old),width,height,flags);
    if(!next)goto failed;
    const SDL_DisplayMode *mode=SDL_GetWindowFullscreenMode(old);
    if((positioned && !SDL_SetWindowPosition(next,x,y)) ||
       (mode && !SDL_SetWindowFullscreenMode(next,mode)) ||
       ((previous&SDL_WINDOW_FULLSCREEN) && !SDL_SetWindowFullscreen(next,true)) ||
       ((previous&SDL_WINDOW_MAXIMIZED) && !(previous&SDL_WINDOW_FULLSCREEN) && !SDL_MaximizeWindow(next)) ||
       ((previous&SDL_WINDOW_MINIMIZED) && !SDL_MinimizeWindow(next)) ||
       (!(previous&SDL_WINDOW_HIDDEN) && !SDL_ShowWindow(next)))goto failed;
    if(!commit(old,next))goto failed;
    SDL_DestroyWindow(old);
    if(!(previous&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED)))SDL_RaiseWindow(next);
    fprintf(stderr,"[native-rhi] replaced Vulkan drawable with OpenGL; logical window retained\n");
    return next;
failed:
    fprintf(stderr,"[RHI] opengl: native window replacement failed: %s\n",SDL_GetError());
    if(next)SDL_DestroyWindow(next);return NULL;
}
#endif
