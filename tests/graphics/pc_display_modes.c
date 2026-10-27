/* Deterministic Windows primary-display enumeration; no display/settings changes. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int present=1, calls, fullscreen=1, minimized, desktop_ok=1, resize_ok=1;
static unsigned desktop_w=3440,desktop_h=1440,client_w=1280,client_h=720,last_w,last_h;
static ULONGLONG now=1000;
static LONG_PTR WINAPI style(HWND w,int index) { (void)w;(void)index;return fullscreen?WS_POPUP:WS_OVERLAPPEDWINDOW; }
static HMONITOR WINAPI monitor_for(HWND w,DWORD flags) { (void)w;(void)flags;return (HMONITOR)2; }
static BOOL WINAPI monitor_info(HMONITOR m,LPMONITORINFO info) {
    if(m!=(HMONITOR)2)abort();strcpy(((MONITORINFOEXA *)info)->szDevice,"secondary");return TRUE;
}
static BOOL WINAPI iconic(HWND w) { (void)w;return minimized; }
static BOOL WINAPI client_rect(HWND w,LPRECT r) { (void)w;*r=(RECT){0,0,client_w,client_h};return TRUE; }
static ULONGLONG WINAPI clock_now(void) { return now; }
static BOOL WINAPI devices(LPCSTR unused,DWORD index,PDISPLAY_DEVICEA d,DWORD flags)
{
    (void)unused;(void)flags;
    if(!present || index>1) return FALSE;
    strcpy(d->DeviceName,index?"primary":"secondary");
    d->StateFlags=DISPLAY_DEVICE_ATTACHED_TO_DESKTOP|(index?DISPLAY_DEVICE_PRIMARY_DEVICE:0);
    return TRUE;
}
static BOOL WINAPI settings(LPCSTR device,DWORD index,DEVMODEA *d)
{
    const unsigned sizes[][3]={{1920,1080,32},{640,480,32},{1920,1080,32},
        {800,600,16},{3840,2160,32},{1080,1920,32},{7680,4320,32},{1280,720,32}};
    if(index==ENUM_CURRENT_SETTINGS) {
        if(strcmp(device,"secondary"))abort();
        d->dmPelsWidth=desktop_w;d->dmPelsHeight=desktop_h;return desktop_ok;
    }
    if(strcmp(device,"primary") || d->dmSize!=sizeof *d || d->dmDriverExtra) abort();
    calls++;
    if(index>=sizeof sizes/sizeof sizes[0]) return FALSE;
    d->dmPelsWidth=sizes[index][0];d->dmPelsHeight=sizes[index][1];d->dmBitsPerPel=sizes[index][2];return TRUE;
}
#define GetWindowLongPtrA style
#define MonitorFromWindow monitor_for
#define GetMonitorInfoA monitor_info
#define IsIconic iconic
#define GetClientRect client_rect
#define GetTickCount64 clock_now
#define EnumDisplayDevicesA devices
#define EnumDisplaySettingsA settings
#include "../src/interface/pc_display.c"
HWND xbox_D3D8Window(void) { return (HWND)123; }
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) { last_w=w;last_h=h;return resize_ok; }
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(int argc,char **argv)
{
    PcDisplayMode modes[128], custom={3000,1400}; int n;
    CHECK(argc==2);CHECK(!_putenv_s("TSFP_DISPLAY_CONFIG",argv[1]));
    {FILE *f=fopen(argv[1],"wb");CHECK(f);fputs("TSFP_DISPLAY_V1 0 0\n",f);fclose(f);}
    n=pc_display_modes((HWND)123,modes,128);
    CHECK(n>40 && calls==9);CHECK(!modes[0].width && !modes[0].height);
    CHECK(modes[1].width==640 && pc_display_is_auto(modes[n-1]));
    for(int i=0;i<n;i++) {
        CHECK(pc_display_valid(modes[i]));
        for(int j=0;j<i;j++)CHECK(modes[i].width!=modes[j].width || modes[i].height!=modes[j].height);
    }
    CHECK(pc_display_aspect((PcDisplayMode){854,480})==3);
    CHECK(pc_display_aspect((PcDisplayMode){1366,768})==3);
    CHECK(pc_display_aspect((PcDisplayMode){3440,1440})==5);
    CHECK(pc_display_aspect((PcDisplayMode){5120,1440})==6);
    CHECK(pc_display_aspect((PcDisplayMode){1280,1024})==2);
    CHECK(pc_display_aspect(custom)==-1);
    CHECK(pc_display_apply(custom));CHECK(pc_display_modes(NULL,modes,128)==n+1);
    CHECK(modes[n].width==3000 && modes[n].height==1400);
    modes[2].width=12345;CHECK(pc_display_modes(NULL,modes,2)==2);CHECK(modes[2].width==12345);
    CHECK(modes[1].width==3000); /* Capacity reserves the saved mode. */
    present=0;CHECK(pc_display_modes(NULL,modes,128)>40);CHECK(!modes[0].width);
    CHECK(!pc_display_modes(NULL,NULL,8) && !pc_display_modes(NULL,modes,0));
    CHECK(pc_display_aspect((PcDisplayMode){1,1})==7);
    CHECK(pc_display_aspect((PcDisplayMode){1000,1000})==-1);
    CHECK(pc_display_apply((PcDisplayMode){1,1}));
    CHECK(last_w==3440 && last_h==1440 && pc_display_is_auto(pc_display_current()));
    CHECK(pc_display_effective().width==3440);
    CHECK(fabsf(pc_display_hud_scale()*3440.f/640.f-1440.f/480.f)<.001f);
    CHECK(pc_display_save());
    {FILE *f=fopen(argv[1],"rb");char line[80];CHECK(f);CHECK(fgets(line,sizeof line,f));fclose(f);CHECK(!strcmp(line,"TSFP_DISPLAY_V1 1 1\n"));}
    initialized=0;CHECK(pc_display_is_auto(pc_display_current()));
    CHECK(pc_display_preview(custom));now+=16000;pc_display_frame_done();CHECK(pc_display_is_auto(pc_display_current()));
    desktop_w=2560;desktop_h=1440;now+=300;pc_display_frame_done();CHECK(last_w==2560 && pc_display_effective().width==2560);
    resize_ok=0;desktop_w=1920;desktop_h=1080;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==2560);
    resize_ok=1;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1920);
    desktop_ok=0;desktop_w=3840;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1920);
    desktop_ok=1;desktop_w=0;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1920);
    fullscreen=0;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1280);
    minimized=1;client_w=800;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1280);
    minimized=0;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==800);
    CHECK(pc_display_apply(custom));fullscreen=1;desktop_w=3840;desktop_h=2160;now+=300;pc_display_frame_done();CHECK(pc_display_effective().width==3000);
    puts("PASS: shared presets, primary modes, deduplication, aspect groups, bounds, custom preservation, fallback and capacity");
    return 0;
}
