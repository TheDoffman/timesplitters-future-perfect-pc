#include "opengl_host.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
struct TsfpGlHost { HWND window;HDC dc;HGLRC context;HMODULE library; };
void *tsfp_gl_host_proc(TsfpGlHost *host,const char *name)
{
    if(!host || !host->context || !name)return NULL;
    PROC proc=wglGetProcAddress(name);
    if(!proc || (uintptr_t)proc<=3 || (intptr_t)proc==-1)proc=GetProcAddress(host->library,name);
    return (void *)proc;
}
TsfpGlHost *tsfp_gl_host_create(void *window)
{
    typedef HGLRC (WINAPI *CreateContext)(HDC,HGLRC,const int *);
    PIXELFORMATDESCRIPTOR pixel={0};TsfpGlHost *host=calloc(1,sizeof *host);
    if(!host || !window){free(host);return NULL;}
    host->window=window;host->dc=GetDC(window);
    host->library=LoadLibraryW(L"opengl32.dll");
    if(!host->dc || !host->library)goto failed;
    if(!GetPixelFormat(host->dc)) {
        pixel.nSize=sizeof pixel;pixel.nVersion=1;
        pixel.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
        pixel.iPixelType=PFD_TYPE_RGBA;pixel.cColorBits=32;pixel.cAlphaBits=8;
        pixel.cDepthBits=24;pixel.cStencilBits=8;
        int format=ChoosePixelFormat(host->dc,&pixel);
        if(!format || !SetPixelFormat(host->dc,format,&pixel))goto failed;
    }
    host->context=wglCreateContext(host->dc);
    if(!host->context || !wglMakeCurrent(host->dc,host->context))goto failed;
    CreateContext create=(CreateContext)tsfp_gl_host_proc(host,"wglCreateContextAttribsARB");
    if(!create)goto failed;
    const int attributes[]={0x2091,4,0x2092,1,0x9126,1,0}; /* Core OpenGL4.1. */
    HGLRC core=create(host->dc,NULL,attributes);
    if(!core)goto failed;
    wglMakeCurrent(NULL,NULL);wglDeleteContext(host->context);host->context=core;
    if(!wglMakeCurrent(host->dc,host->context))goto failed;
    fprintf(stderr,"[RHI] opengl: %s; %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    return host;
failed:
    fprintf(stderr,"[RHI] opengl: could not create a core4.1 context (Windows error %lu)\n",GetLastError());
    tsfp_gl_host_destroy(host);return NULL;
}
int tsfp_gl_host_present(TsfpGlHost *host,int interval)
{
    typedef BOOL (WINAPI *SwapInterval)(int);
    if(!host)return -1;
    SwapInterval set=(SwapInterval)tsfp_gl_host_proc(host,"wglSwapIntervalEXT");
    if(set && !set(interval?1:0))return -1;
    return SwapBuffers(host->dc)?0:-1;
}
void tsfp_gl_host_destroy(TsfpGlHost *host)
{
    if(!host)return;
    if(host->context){wglMakeCurrent(NULL,NULL);wglDeleteContext(host->context);}
    if(host->dc)ReleaseDC(host->window,host->dc);
    if(host->library)FreeLibrary(host->library);
    free(host);
}
#else
#include <SDL3/SDL.h>
struct TsfpGlHost {SDL_Window *window;SDL_GLContext context;};
TsfpGlHost *tsfp_gl_host_create(void *window)
{
    TsfpGlHost *host=calloc(1,sizeof *host);
    if(!host || !window){free(host);return NULL;}
    host->window=window;
    /* SDL window creation/replacement belongs to the OS window adapter. */
    if(!(SDL_GetWindowFlags(host->window)&SDL_WINDOW_OPENGL))goto failed;
    host->context=SDL_GL_CreateContext(host->window);
    if(!host->context || !SDL_GL_MakeCurrent(host->window,host->context))goto failed;
    int major=0,minor=0;
    if(!SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major) ||
       !SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor) ||
       major<4 || (major==4 && minor<1))goto failed;
    return host;
failed:
    fprintf(stderr,"[RHI] opengl: context creation failed: %s\n",SDL_GetError());
    tsfp_gl_host_destroy(host);return NULL;
}
void *tsfp_gl_host_proc(TsfpGlHost *host,const char *name)
{return host && host->context && name?(void *)SDL_GL_GetProcAddress(name):NULL;}
int tsfp_gl_host_present(TsfpGlHost *host,int interval)
{
    if(!host || !SDL_GL_SetSwapInterval(interval?1:0))return -1;
    return SDL_GL_SwapWindow(host->window)?0:-1;
}
void tsfp_gl_host_destroy(TsfpGlHost *host)
{
    if(!host)return;
    if(host->context){SDL_GL_MakeCurrent(host->window,NULL);SDL_GL_DestroyContext(host->context);}
    free(host);
}
#endif
