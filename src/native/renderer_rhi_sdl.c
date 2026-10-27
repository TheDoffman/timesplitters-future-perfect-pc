/* Native OS window adapter for the shared Xbox/RHI renderer. */
#include <SDL3/SDL.h>
#include "renderer_host_sdl.h"
#include "settings_macos.h"
#include "d3d8_xbox.h"
#include "d3d8_overlay.h"
#include "rhi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "../../tests/support/interpolation_depth_device.h"
#include "renderer_host_selftest.inc"

static SDL_Window *native_window;
void *tsfp_native_renderer_window(unsigned width,unsigned height)
{
    if(native_window)return native_window;
    if(!SDL_IsMainThread()) {
        fprintf(stderr,"[native-rhi] window creation requires the main thread\n");return NULL;
    }
    if(!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO))return NULL;
    native_window=SDL_CreateWindow("TimeSplitters: Future Perfect — Shared Native Renderer",
        width?width:640,height?height:480,SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!native_window){fprintf(stderr,"[native-rhi] window: %s\n",SDL_GetError());return NULL;}
    tsfp_renderer_host_bind(native_window,d3d8_overlay_shutdown);
    tsfp_native_settings_attach(native_window);
    SDL_RaiseWindow(native_window);
    fprintf(stderr,"[native-rhi] common Xbox renderer, SDL Vulkan window\n");
    return native_window;
}

/* Bounded smoke through the actual common Xbox device, not only RHI calls. */
int tsfp_native_renderer_test(void)
{
    IDirect3D8 *api=xbox_Direct3DCreate8(0);IDirect3DDevice8 *dev=NULL;
    D3DPRESENT_PARAMETERS pp={0};pp.BackBufferWidth=pp.BackBufferHeight=64;pp.Windowed=TRUE;
    pp.hDeviceWindow=tsfp_native_renderer_window(64,64);
    if(!pp.hDeviceWindow || FAILED(api->lpVtbl->CreateDevice(api,0,1,pp.hDeviceWindow,0,&pp,&dev)) || !dev)return 1;
    struct Vertex {float x,y,z,w;DWORD color;} vertices[3]={
        {0,0,0,1,0xFFFF0000},{64,0,0,1,0xFFFF0000},{0,64,0,1,0xFFFF0000}};
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZENABLE,FALSE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_ALPHATESTENABLE,FALSE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_ALPHABLENDENABLE,FALSE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_LIGHTING,FALSE);
    dev->lpVtbl->SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    dev->lpVtbl->SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    dev->lpVtbl->SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    dev->lpVtbl->SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    dev->lpVtbl->SetVertexShader(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    int ok=native_test_renderer_host(native_window,d3d8_overlay_shutdown);
    ok &= SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF0000FF,1,0));
    ok &= SUCCEEDED(dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,1,vertices,sizeof vertices[0]));
    IDirect3DSurface8 *surface=NULL;D3DLOCKED_RECT lock;
    ok &= SUCCEEDED(dev->lpVtbl->GetBackBuffer(dev,0,0,&surface));
    if(surface && SUCCEEDED(surface->lpVtbl->LockRect(surface,&lock,NULL,D3DLOCK_READONLY))) {
        const unsigned char *near=(const unsigned char *)lock.pBits+8*lock.Pitch+8*4;
        const unsigned char *far=(const unsigned char *)lock.pBits+56*lock.Pitch+56*4;
        fprintf(stderr,"[native-rhi-test] red pixel %u,%u,%u; blue pixel %u,%u,%u\n",
            near[0],near[1],near[2],far[0],far[1],far[2]);
        /* The common scene target/readback uses R8G8B8A8 host bytes. */
        ok &= near[0]>251 && near[1]<4 && near[2]<4;
        ok &= far[0]<4 && far[1]<4 && far[2]>251;
        surface->lpVtbl->UnlockRect(surface);
    } else ok=0;
    /* Exercise point output through the actual fixed-function generator. */
    struct Vertex point={32.5f,32.5f,0,1,0xFFFF0000};
    ok &= SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF0000FF,1,0));
    ok &= SUCCEEDED(dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_POINTLIST,1,&point,sizeof point));
    if(surface && SUCCEEDED(surface->lpVtbl->LockRect(surface,&lock,NULL,D3DLOCK_READONLY))) {
        unsigned lit=0;
        for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++) {
            const unsigned char *p=(const unsigned char *)lock.pBits+y*lock.Pitch+x*4;
            lit+=p[0]>251 && p[1]<4 && p[2]<4;
        }
        fprintf(stderr,"[native-rhi-test] fixed-function point pixels: %u\n",lit);
        ok &= lit==1;surface->lpVtbl->UnlockRect(surface);
    } else ok=0;
    if(surface)surface->lpVtbl->Release(surface);
    ok &= interpolation_depth_device_test(dev);
    ok &= tsfp_native_graphics_settings_test(native_window);
    /* Settings must leave the common targets usable after resizing/rollback. */
    ok &= SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF0000FF,1,0));
    ok &= depth_triangle(dev,.5f,0xFF00FF00) && depth_pixel(dev,0,255,0);
    ok &= native_test_renderer_window_modes(native_window,dev);
    d3d8_overlay_draw("Shared native renderer");
    ok &= SUCCEEDED(dev->lpVtbl->Swap(dev,0));
    dev->lpVtbl->Release(dev);api->lpVtbl->Release(api);
    tsfp_renderer_host_bind(NULL,NULL);SDL_DestroyWindow(native_window);native_window=NULL;
    fprintf(stderr,"[native-rhi-test] common Xbox clear, fixed-function draw, readback, overlay and present: %s\n",ok?"PASS":"FAIL");
    return ok?0:1;
}
