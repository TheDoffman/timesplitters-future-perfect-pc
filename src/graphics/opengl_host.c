#include "opengl_host.h"
#include "opengl_present.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
static void *(*prepare_window)(void *);
void tsfp_gl_host_set_window_adapter(void *(*prepare)(void *))
{prepare_window=prepare;}
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
struct TsfpGlHost {TsfpGlPresentState presentation; HWND window;HDC dc;HGLRC context;HMODULE library; };
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
/* One serialized command at a time; pointer arguments remain valid because
 * dispatch waits for completion. CPU-only state never needs a context switch.
 * Worker creation/shutdown and native synchronization live at this OS boundary. */
static SRWLOCK call_lock=SRWLOCK_INIT,queue_lock=SRWLOCK_INIT;
static CONDITION_VARIABLE queue_changed=CONDITION_VARIABLE_INIT;
static HANDLE graphics_thread;
static void (*pending_call)(void *);
static void *pending_argument;
static int pending_done,stopping;
static DWORD WINAPI graphics_loop(void *unused)
{
    (void)unused;AcquireSRWLockExclusive(&queue_lock);
    for(;;) {
        while(!pending_call && !stopping)SleepConditionVariableSRW(&queue_changed,&queue_lock,INFINITE,0);
        if(stopping)break;
        void (*operation)(void *)=pending_call;void *argument=pending_argument;
        ReleaseSRWLockExclusive(&queue_lock);operation(argument);AcquireSRWLockExclusive(&queue_lock);
        pending_call=NULL;pending_done=1;WakeAllConditionVariable(&queue_changed);
    }
    ReleaseSRWLockExclusive(&queue_lock);return 0;
}
int tsfp_gl_host_run(void (*operation)(void *),void *argument,int gpu)
{
    int ok=1;AcquireSRWLockExclusive(&call_lock);
    if(!gpu)operation(argument);
    else {
        if(!graphics_thread)graphics_thread=CreateThread(NULL,0,graphics_loop,NULL,0,NULL);
        if(!graphics_thread){fprintf(stderr,"[RHI] ERROR: opengl graphics thread creation failed\n");ok=0;}
        else {
            AcquireSRWLockExclusive(&queue_lock);pending_done=0;
            pending_call=operation;pending_argument=argument;WakeAllConditionVariable(&queue_changed);
            while(!pending_done)SleepConditionVariableSRW(&queue_changed,&queue_lock,INFINITE,0);
            ReleaseSRWLockExclusive(&queue_lock);
        }
    }
    ReleaseSRWLockExclusive(&call_lock);return ok;
}
void tsfp_gl_host_stop(void)
{
    AcquireSRWLockExclusive(&call_lock);
    if(graphics_thread) {
        AcquireSRWLockExclusive(&queue_lock);stopping=1;WakeAllConditionVariable(&queue_changed);ReleaseSRWLockExclusive(&queue_lock);
        WaitForSingleObject(graphics_thread,INFINITE);CloseHandle(graphics_thread);graphics_thread=NULL;stopping=0;
    }
    ReleaseSRWLockExclusive(&call_lock);
}
int tsfp_gl_host_make_current(TsfpGlHost *host)
{return host && wglMakeCurrent(host->dc,host->context);}
void tsfp_gl_host_release(TsfpGlHost *host)
{if(host)wglMakeCurrent(NULL,NULL);}
static int driver_interval(void *context,int interval,const char **reason)
{
    typedef BOOL (WINAPI *SwapInterval)(int);TsfpGlHost *host=context;
    SwapInterval set=(SwapInterval)tsfp_gl_host_proc(host,"wglSwapIntervalEXT");
    if(!set){*reason="WGL_EXT_swap_control unavailable";return 0;}
    if(!set(interval)){*reason="WGL driver refused request";return 0;}return 1;
}
static int driver_swap(void *context,const char **reason)
{
    TsfpGlHost *host=context;
    if(!SwapBuffers(host->dc)){*reason="SwapBuffers failed";return 0;}return 1;
}
int tsfp_gl_host_present(TsfpGlHost *host,int interval)
{return host?tsfp_gl_present_driver(&host->presentation,host,interval,driver_interval,driver_swap):-1;}
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
#include <pthread.h>
struct TsfpGlHost {TsfpGlPresentState presentation;SDL_Window *window;SDL_GLContext context;};
TsfpGlHost *tsfp_gl_host_create(void *window)
{
    TsfpGlHost *host=calloc(1,sizeof *host);
    if(!host || !window){free(host);return NULL;}
    if(!SDL_IsMainThread()) {
        fprintf(stderr,"[RHI] opengl: SDL context creation requires main-thread dispatch\n");
        free(host);return NULL;
    }
    host->window=prepare_window?prepare_window(window):window;
    if(!host->window || !(SDL_GetWindowFlags(host->window)&SDL_WINDOW_OPENGL)) {
        fprintf(stderr,"[RHI] opengl: no compatible SDL OpenGL window\n");
        free(host);return NULL;
    }
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
static pthread_mutex_t call_lock=PTHREAD_MUTEX_INITIALIZER;
typedef struct {void (*operation)(void *);void *argument;} MainCall;
static void main_operation(void *argument)
{
    MainCall *call=argument;pthread_mutex_lock(&call_lock);
    call->operation(call->argument);pthread_mutex_unlock(&call_lock);
}
int tsfp_gl_host_run(void (*operation)(void *),void *argument,int gpu)
{
    MainCall call={operation,argument};
    if(!gpu || SDL_IsMainThread()){main_operation(&call);return 1;}
    /* Do not hold call_lock while queueing: the main event thread must remain
     * free to service this callback while guest threads change CPU-only state. */
    if(SDL_RunOnMainThread(main_operation,&call,true))return 1;
    fprintf(stderr,"[RHI] ERROR: opengl main-thread dispatch failed: %s\n",SDL_GetError());return 0;
}
void tsfp_gl_host_stop(void){}
int tsfp_gl_host_make_current(TsfpGlHost *host)
{return host && SDL_GL_MakeCurrent(host->window,host->context);}
void tsfp_gl_host_release(TsfpGlHost *host)
{if(host)SDL_GL_MakeCurrent(host->window,NULL);}
static int driver_interval(void *context,int interval,const char **reason)
{
    (void)context;if(!SDL_GL_SetSwapInterval(interval)){*reason=SDL_GetError();return 0;}return 1;
}
static int driver_swap(void *context,const char **reason)
{
    TsfpGlHost *host=context;
    if(!SDL_GL_SwapWindow(host->window)){*reason=SDL_GetError();return 0;}return 1;
}
int tsfp_gl_host_present(TsfpGlHost *host,int interval)
{return host?tsfp_gl_present_driver(&host->presentation,host,interval,driver_interval,driver_swap):-1;}
void tsfp_gl_host_destroy(TsfpGlHost *host)
{
    if(!host)return;
    if(host->context){SDL_GL_MakeCurrent(host->window,NULL);SDL_GL_DestroyContext(host->context);}
    free(host);
}
#endif
