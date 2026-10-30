#ifndef TSFP_PC_DISPLAY_H
#define TSFP_PC_DISPLAY_H
#ifdef _WIN32
#include <windows.h>
#else
typedef void *HWND;
#endif
#define PC_FOV_DEFAULT 55
#define PC_FOV_MIN 40
#define PC_FOV_MAX 100
int pc_display_fov(void);
int pc_display_set_fov(int degrees);
float pc_display_fov_scale(void);

typedef struct PcDisplayMode { unsigned width, height; } PcDisplayMode;
/* Selected mode: 0x0 retains console output; 1x1 selects Auto. */
PcDisplayMode pc_display_current(void);
PcDisplayMode pc_display_effective(void);
static inline int pc_display_is_auto(PcDisplayMode m) { return m.width==1 && m.height==1; }
/* Native window thread publishes physical drawable pixels; no GPU work here. */
void pc_display_host_size(unsigned width,unsigned height);
int pc_display_valid(PcDisplayMode mode);
#define PC_DISPLAY_MODE_CAPACITY 128
#define PC_DISPLAY_ASPECT_COUNT 8 /* All, six ratios, Auto. */
const char *pc_display_aspect_name(int aspect);
int pc_display_aspect(PcDisplayMode mode);
/* Same nearest-mode rule for the guest menu and native settings adapters.
 * Keep an exact staged/custom mode in All; otherwise prefer matching height. */
static inline int pc_display_filter_modes(const PcDisplayMode *all,int count,int aspect,
        PcDisplayMode staged,PcDisplayMode *out,int capacity,int *selected)
{
    unsigned long long best=~0ull;int n=0;
    if(selected)*selected=0;
    if(!all || !out || !selected || capacity<=0 || aspect<0 || aspect>=PC_DISPLAY_ASPECT_COUNT)return 0;
    for(int i=0;i<count && n<capacity;i++) {
        PcDisplayMode m=all[i];
        if(aspect && pc_display_aspect(m)!=aspect)continue;
        unsigned mw=m.width?m.width:640,mh=m.height?m.height:480;
        unsigned sw=staged.width?staged.width:640,sh=staged.height?staged.height:480;
        unsigned long long score=(unsigned long long)(mh>sh?mh-sh:sh-mh)*10000+(mw>sw?mw-sw:sw-mw);
        score=score*2+1;
        if(m.width==staged.width && m.height==staged.height)score=0;
        if(score<best){best=score;*selected=n;}
        out[n++]=m;
    }
    return n;
}
int pc_display_modes(HWND window, PcDisplayMode *out, int capacity);
int pc_display_apply(PcDisplayMode mode);
int pc_display_preview(PcDisplayMode mode);
int pc_display_save(void);
void pc_display_scene_size(unsigned guest_w, unsigned guest_h, unsigned *w, unsigned *h);
void pc_display_shape(unsigned *w, unsigned *h);
float pc_display_camera_factor(void);
float pc_display_hud_scale(void);
int pc_display_wide_now(void);
void pc_display_frame_done(void);
void pc_display_menu_filter_begin(int frontend);
int pc_display_menu_filter(void);
int pc_display_menu_font_filter(int active);
HWND xbox_D3D8Window(void);
int xbox_D3D8ChangeResolution(unsigned width, unsigned height);
#endif
