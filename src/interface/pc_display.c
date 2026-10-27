#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <SDL3/SDL.h>
#include "config_files.h"
#define ULONGLONG Uint64
#define GetTickCount64 SDL_GetTicks
#endif

static PcDisplayMode current={1,1};
static PcDisplayMode automatic; /* Use guest output until a valid window is available. */
static ULONGLONG automatic_checked;
#ifndef _WIN32
static SDL_AtomicInt host_pixels;
#endif

void pc_display_host_size(unsigned width,unsigned height)
{
#ifndef _WIN32
    if(width<=65535 && height<=65535)
        SDL_SetAtomicInt(&host_pixels,(int)((width<<16)|height));
#else
    (void)width;(void)height;
#endif
}

static PcDisplayMode detected_mode(void)
{
    PcDisplayMode mode={0,0};
#ifdef _WIN32
    HWND window=xbox_D3D8Window();
    RECT client;
    if(window && (GetWindowLongPtrA(window,GWL_STYLE)&WS_POPUP)) {
        MONITORINFOEXA monitor;DEVMODEA desktop;
        memset(&monitor,0,sizeof monitor);monitor.cbSize=sizeof monitor;
        memset(&desktop,0,sizeof desktop);desktop.dmSize=sizeof desktop;
        if(GetMonitorInfoA(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),(MONITORINFO *)&monitor) &&
           EnumDisplaySettingsA(monitor.szDevice,ENUM_CURRENT_SETTINGS,&desktop)) {
            mode.width=desktop.dmPelsWidth;mode.height=desktop.dmPelsHeight;
        }
    } else if(window && !IsIconic(window) && GetClientRect(window,&client)) {
        mode.width=client.right-client.left;mode.height=client.bottom-client.top;
    }
#else
    unsigned pixels=(unsigned)SDL_GetAtomicInt(&host_pixels);
    mode.width=pixels>>16;mode.height=pixels&65535u;
#endif
    return mode.width>=640 && pc_display_valid(mode)?mode:automatic;
}
static int initialized;
static int vertical_fov=PC_FOV_DEFAULT, preview_fov;
static int camera_frame;
static int menu_filter_frame, menu_font_filter;
void pc_display_menu_filter_begin(int frontend) { menu_filter_frame=!!frontend; }
int pc_display_menu_font_filter(int active)
{
    int previous=menu_font_filter;menu_font_filter=!!active;return previous;
}
int pc_display_menu_filter(void)
{
    static int configured, enabled=1;
    if(!configured) {
        const char *value=getenv("TSFP_MENU_FILTER");
        enabled=!value || strcmp(value,"linear");configured=1;
    }
    return menu_filter_frame && enabled ? (menu_font_filter?2:1) : 0;
}
static PcDisplayMode preview_previous;
static ULONGLONG preview_deadline;
static char path[32768];

int pc_display_valid(PcDisplayMode m)
{
    return pc_display_is_auto(m) || (!m.width && !m.height) || (m.width >= 640 && m.height >= 480 &&
        m.width <= 7680 && m.height <= 4320 &&
        (unsigned long long)m.width * m.height <= 33177600 &&
        m.width >= m.height && m.width <= 4 * m.height);
}

static void init(void)
{
    const char *p; FILE *f; unsigned w,h; int fov=PC_FOV_DEFAULT; char version[32], tail;
    if(initialized) return;
    initialized=1;
    p=getenv("TSFP_DISPLAY_CONFIG");
    if(p && *p) snprintf(path,sizeof path,"%s",p);
    else {
        p=getenv("RECOMP_INPUT_CONFIG");
        if(p && *p) {
            char *a,*b,*slash;
            snprintf(path,sizeof path,"%s",p);
            a=strrchr(path,'/'); b=strrchr(path,'\\');
            slash=a && b ? (a>b?a:b) : a?a:b;
            if(slash) snprintf(slash+1,sizeof path-(size_t)(slash+1-path),"pc-display.ini");
            else snprintf(path,sizeof path,"pc-display.ini");
        } else snprintf(path,sizeof path,"pc-display.ini");
    }
    f=fopen(path,"rb");
    if(f) {
        if(fscanf(f,"%31s",version)==1 &&
           ((!strcmp(version,"TSFP_DISPLAY_V1") && fscanf(f," %u %u %c",&w,&h,&tail)==2) ||
            (!strcmp(version,"TSFP_DISPLAY_V2") && fscanf(f," %u %u %d %c",&w,&h,&fov,&tail)==3)) &&
           fov>=PC_FOV_MIN && fov<=PC_FOV_MAX) {
            PcDisplayMode m={w,h};
            if(pc_display_valid(m)) { current=m; vertical_fov=fov; }
        }
        fclose(f);
    }
    fprintf(stderr,"[PC-DISPLAY] Settings %s, resolution %ux%u\n",path,current.width,current.height);
}

int pc_display_fov(void) { init(); return vertical_fov; }
int pc_display_set_fov(int degrees)
{
    init();
    if(degrees<PC_FOV_MIN || degrees>PC_FOV_MAX) return 0;
    vertical_fov=degrees; return 1;
}
float pc_display_fov_scale(void)
{
    init();
    return vertical_fov==PC_FOV_DEFAULT?1.f:
        tanf(vertical_fov*.00872664626f)/tanf(PC_FOV_DEFAULT*.00872664626f);
}
/* Keep legacy files readable when the new setting is at its default. */
static void config_text(char *text,size_t size)
{
    if(vertical_fov==PC_FOV_DEFAULT)
        snprintf(text,size,"TSFP_DISPLAY_V1 %u %u\n",current.width,current.height);
    else snprintf(text,size,"TSFP_DISPLAY_V2 %u %u %d\n",current.width,current.height,vertical_fov);
}

PcDisplayMode pc_display_current(void) { init(); return current; }
PcDisplayMode pc_display_effective(void) { init(); return pc_display_is_auto(current)?automatic:current; }

typedef struct PcAspect { const char *name; unsigned width,height; } PcAspect;
static const PcAspect aspects[]={ {"All",0,0},
#define PC_DISPLAY_ASPECT(label,w,h) {label,w,h},
#define PC_DISPLAY_PRESET(w,h)
#include "pc_display_modes.inc"
#undef PC_DISPLAY_ASPECT
#undef PC_DISPLAY_PRESET
};
static const PcDisplayMode presets[]={
#define PC_DISPLAY_ASPECT(label,w,h)
#define PC_DISPLAY_PRESET(w,h) {w,h},
#include "pc_display_modes.inc"
#undef PC_DISPLAY_ASPECT
#undef PC_DISPLAY_PRESET
};
const char *pc_display_aspect_name(int aspect)
{ return aspect>=0 && aspect<PC_DISPLAY_ASPECT_COUNT?aspects[aspect].name:"Custom"; }
int pc_display_aspect(PcDisplayMode mode)
{
    if(!mode.width && !mode.height) return 1; /* Original 4:3 picture. */
    if(pc_display_is_auto(mode)) return PC_DISPLAY_ASPECT_COUNT-1;
    if(!pc_display_valid(mode)) return -1;
    for(int i=1;i<PC_DISPLAY_ASPECT_COUNT-1;i++) {
        double ratio=(double)mode.width*aspects[i].height/(mode.height*aspects[i].width);
        /* Marketed 21:9 modes include 2560x1080 and 3440x1440; other
           groups tolerate only pixel rounding (e.g. 1366x768). */
        if(fabs(ratio-1.)< (i==5?.02:.002)) return i;
    }
    return -1;
}
static void add_mode(PcDisplayMode *out,int *count,int cap,unsigned w,unsigned h)
{
    int i; PcDisplayMode m={w,h};
    if(!pc_display_valid(m)) return;
    for(i=0;i<*count;i++) if(out[i].width==w && out[i].height==h) return;
    if(*count<cap) out[(*count)++]=m;
}

int pc_display_modes(HWND window,PcDisplayMode *out,int capacity)
{
#ifdef _WIN32
    DISPLAY_DEVICEA primary; DEVMODEA dm; int count=0,reported_capacity; DWORD i;
    (void)window;
    if(!out || capacity<=0) return 0;
    init(); reported_capacity=capacity-(current.width?1:0);
    add_mode(out,&count,capacity,0,0);
    /* These are render targets, not exclusive-fullscreen display modes.
       Offer common presets even when the monitor does not enumerate them. */
    for(int preset=0;preset<sizeof presets/sizeof presets[0];preset++)
        add_mode(out,&count,reported_capacity,presets[preset].width,presets[preset].height);
    /* Also include the primary adapter's reported modes. */
    for(i=0;;i++) {
        memset(&primary,0,sizeof primary); primary.cb=sizeof primary;
        if(!EnumDisplayDevicesA(NULL,i,&primary,0)) { primary.DeviceName[0]=0; break; }
        if((primary.StateFlags & (DISPLAY_DEVICE_PRIMARY_DEVICE|DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) ==
            (DISPLAY_DEVICE_PRIMARY_DEVICE|DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) break;
    }
    if(primary.DeviceName[0]) for(i=0;;i++) {
        memset(&dm,0,sizeof dm); dm.dmSize=sizeof dm;
        if(!EnumDisplaySettingsA(primary.DeviceName,i,&dm)) break;
        if(dm.dmBitsPerPel>=24)
            add_mode(out,&count,reported_capacity,dm.dmPelsWidth,dm.dmPelsHeight);
    }
#else
    int count=0,n=0,reported_capacity;
    if(!out || capacity<=0)return 0;
    add_mode(out,&count,capacity,0,0);
    init();reported_capacity=capacity-(current.width?1:0);
    SDL_DisplayID display=window?SDL_GetDisplayForWindow((SDL_Window *)window):SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *desktop=SDL_GetDesktopDisplayMode(display);
    if(desktop)add_mode(out,&count,reported_capacity,desktop->w,desktop->h);
    for(int preset=0;preset<sizeof presets/sizeof presets[0];preset++)
        add_mode(out,&count,reported_capacity,presets[preset].width,presets[preset].height);
    SDL_DisplayMode **modes=SDL_GetFullscreenDisplayModes(display,&n);
    for(int i=0;modes&&i<n;i++)add_mode(out,&count,reported_capacity,modes[i]->w,modes[i]->h);
    SDL_free(modes);
#endif
    /* Stable ascending dimensions, with the console default first. Keep the
       current custom mode visible separately, so opening cannot change it. */
    for(int a=1;a<count;a++) for(int b=a;b>1;b--) {
        PcDisplayMode left=out[b-1],right=out[b];
        if(left.width<right.width || (left.width==right.width && left.height<=right.height)) break;
        out[b-1]=right; out[b]=left;
    }
    add_mode(out,&count,reported_capacity,1,1);
    init(); add_mode(out,&count,capacity,current.width,current.height);
    return count;
}

int pc_display_apply(PcDisplayMode m)
{
    PcDisplayMode before,old_automatic=automatic,target;
    init();
    if(!pc_display_valid(m)) return 0;
    before=current;
    current=m;
    if(pc_display_is_auto(m)) automatic=detected_mode();
    target=pc_display_effective();
    if(!xbox_D3D8ChangeResolution(target.width,target.height)) { current=before; automatic=old_automatic; return 0; }
    preview_deadline=0;
    fprintf(stderr,"[PC-DISPLAY] Applied %ux%u\n",m.width,m.height);
    return 1;
}

int pc_display_preview(PcDisplayMode mode)
{
    PcDisplayMode before=pc_display_current();
    if(!pc_display_apply(mode)) return 0;
    preview_previous=before; preview_fov=vertical_fov; preview_deadline=GetTickCount64()+15000;
    return 1;
}

int pc_display_save(void)
{
#ifndef _WIN32
    char text[128];init();
    config_text(text,sizeof text);
    int ok=tsfp_config_write(path,text,0);
    if(ok)preview_deadline=0;
    return ok;
#else
    char temp[32768],text[128]; FILE *f; int ok;
    init();
    if(snprintf(temp,sizeof temp,"%s.tmp",path)<0 || strlen(path)+5>=sizeof temp) return 0;
    f=fopen(temp,"wb"); if(!f) return 0;
    config_text(text,sizeof text);
    ok=fputs(text,f)>=0;
    if(fflush(f)) ok=0;
    if(fclose(f)) ok=0;
    if(ok) ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileA(temp);
    if(ok) { preview_deadline=0; fprintf(stderr,"[PC-DISPLAY] Saved %ux%u\n",current.width,current.height); }
    return ok;
#endif
}

void pc_display_scene_size(unsigned gw,unsigned gh,unsigned *w,unsigned *h)
{
    init();
    if(pc_display_is_auto(current)) automatic=detected_mode();
    PcDisplayMode output=pc_display_effective();
    if(w) *w=output.width?output.width:gw;
    if(h) *h=output.height?output.height:gh;
}

void pc_display_shape(unsigned *w,unsigned *h)
{
    init();
    /* Backgrounds use the complete canvas; 2D widgets fit proportionally. */
    PcDisplayMode output=pc_display_effective();
    if(w) *w=output.width?output.width:4;
    if(h) *h=output.height?output.height:3;
}

float pc_display_camera_factor(void)
{
    init(); camera_frame=1;
    PcDisplayMode output=pc_display_effective();
    return output.height?(float)output.width*3.f/((float)output.height*4.f):1.f;
}
float pc_display_hud_scale(void)
{
    PcDisplayMode output=pc_display_effective();
    return output.width?(4.f*(float)output.height)/(3.f*(float)output.width):1.f;
}
int pc_display_wide_now(void) { PcDisplayMode output=pc_display_effective(); return output.width && output.width*3u!=output.height*4u; }
void pc_display_frame_done(void)
{
    menu_filter_frame=0;
    menu_font_filter=0;
    camera_frame=0;
    if(pc_display_is_auto(pc_display_current()) && GetTickCount64()-automatic_checked>=250) {
        PcDisplayMode wanted=detected_mode(),before=automatic;
        automatic_checked=GetTickCount64();
        if(wanted.width!=before.width || wanted.height!=before.height) {
            automatic=wanted;
            if(!xbox_D3D8ChangeResolution(wanted.width,wanted.height)) automatic=before;
            else fprintf(stderr,"[PC-DISPLAY] Auto output %ux%u\n",wanted.width,wanted.height);
        }
    }
    /* Covers leaving the guest menu through a global pause/Start action too.
       Never persist a preview; a crash or forced exit also reloads the old mode. */
    if(preview_deadline && GetTickCount64()>=preview_deadline && pc_display_apply(preview_previous)) {
        vertical_fov=preview_fov;
        fprintf(stderr,"[PC-DISPLAY] Preview timed out; previous display settings restored\n");
    }
}
