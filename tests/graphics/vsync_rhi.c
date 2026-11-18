/* Observe the exact arguments handed to a real DXGI swap chain by the
   production backend. WARP keeps this independent of an installed GPU. */
#define COBJMACROS
#include <d3d11.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_config.h"
#include "graphics/pc_vsync_settings.h"
static UINT seen_interval,seen_flags,presents;
static HRESULT observed_present(IDXGISwapChain *swap,UINT interval,UINT flags){
    seen_interval=interval;seen_flags=flags;presents++;
    return swap->lpVtbl->Present(swap,interval,flags);
}
#undef IDXGISwapChain_Present
#define IDXGISwapChain_Present observed_present
#define D3D11CreateDeviceAndSwapChain(adapter,driver,software,flags,levels,count,version,desc,swap,dev,feature,ctx) D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_WARP,software,flags,levels,count,version,desc,swap,dev,feature,ctx)
#include "rhi_d3d11.c"
int main(void){
    const char *path=getenv("RECOMP_DISPLAY_CONFIG");assert(path);
    RecompSettings settings,roundtrip;recomp_settings_defaults(&settings);assert(!settings.vsync);
    settings.vsync=1;assert(recomp_settings_write(path,&settings));
    assert(recomp_settings_read(path,&roundtrip) && roundtrip.vsync);
    settings.vsync=0;assert(recomp_settings_write(path,&settings));
    WNDCLASSA wc={0};wc.lpfnWndProc=DefWindowProcA;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="VSyncRegression";
    assert(RegisterClassA(&wc));
    HWND window=CreateWindowA(wc.lpszClassName,"V-Sync renderer test",WS_OVERLAPPEDWINDOW,0,0,160,120,NULL,NULL,wc.hInstance,NULL);assert(window);
    RhiDeviceDesc desc={window,160,120,2,1,0};assert(!rhi_device_create(&desc));
    int rates[]={0,120,180,240};
    for(unsigned rate=0;rate<4;rate++){
        assert(recomp_config_request_native_rate(rates[rate]));recomp_config_commit_native_rate();
        for(int enabled=0;enabled<=1;enabled++){
            assert(pc_vsync_set_enabled(enabled));
            rhi_swapchain_set_vrr(!enabled);
            rhi_set_render_target(rhi_swapchain_view(),NULL);
            float clear[4]={.1f,.2f,.3f,1};rhi_clear_color(rhi_swapchain_view(),clear);
            unsigned before=presents;
            assert(rhi_present_kind(pc_vsync_present_interval(0),RHI_PRESENT_GUEST)>=0);
            assert(presents==before+1 && seen_interval==(UINT)enabled);
            if(enabled)assert(!(seen_flags & DXGI_PRESENT_ALLOW_TEARING));
            const char *actual=recomp_config_lookup(NULL,"native_fps");assert(actual && atoi(actual)==rates[rate]);
        }
    }
    assert(!rhi_swapchain_resize(128,96));
    assert(pc_vsync_set_enabled(1));assert(rhi_present(pc_vsync_present_interval(0))>=0 && seen_interval==1);
    assert(pc_vsync_set_enabled(0));assert(rhi_present(pc_vsync_present_interval(0))>=0 && seen_interval==0);
    rhi_device_destroy();DestroyWindow(window);
    puts("PASS: DXGI V-sync On/Off, resize, no tearing while synchronized, FPS preferences unchanged");return 0;
}
