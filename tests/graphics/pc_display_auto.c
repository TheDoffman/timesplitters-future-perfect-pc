/* Exercise the shared Auto policy through the SDL snapshot adapter on either host.
 * Window queries are mocked; this does not validate Cocoa or GPU allocation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#undef _WIN32
#endif
#include "../src/interface/pc_display.c"
static int available=1;
static unsigned applied_w,applied_h;
HWND xbox_D3D8Window(void) { return NULL; }
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) { applied_w=w;applied_h=h;return available; }
int tsfp_config_write(const char *name,const char *text,int backup) {
    (void)backup;FILE *f=fopen(name,"wb");if(!f)return 0;int ok=fputs(text,f)>=0;return fclose(f)==0 && ok;
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    unsigned w,h;PcDisplayMode modes[128],filtered[128];int selected;
    CHECK(pc_display_is_auto(pc_display_current()));
    pc_display_scene_size(640,480,&w,&h);CHECK(w==640 && h==480);
    pc_display_host_size(3440,1440);pc_display_scene_size(640,480,&w,&h);
    CHECK(w==3440 && h==1440);pc_display_shape(&w,&h);CHECK(w==3440 && h==1440);
    CHECK(fabsf(pc_display_camera_factor()-3440.f*3/(1440*4))<.001f);
    CHECK(pc_display_save());initialized=0;CHECK(pc_display_is_auto(pc_display_current()));
    int count=pc_display_modes(NULL,modes,128);
    CHECK(pc_display_filter_modes(modes,count,7,(PcDisplayMode){3440,1440},filtered,128,&selected)==1);
    CHECK(pc_display_is_auto(filtered[0]));
    pc_display_host_size(2560,1440);fake_ticks+=300;pc_display_frame_done();
    CHECK(applied_w==2560 && applied_h==1440 && pc_display_effective().width==2560);
    pc_display_host_size(0,0);fake_ticks+=300;pc_display_frame_done();CHECK(pc_display_effective().width==2560);
    pc_display_host_size(100000,1440);fake_ticks+=300;pc_display_frame_done();CHECK(pc_display_effective().width==2560);
    pc_display_host_size(1920,1080);available=0;fake_ticks+=300;pc_display_frame_done();CHECK(pc_display_effective().width==2560);
    available=1;fake_ticks+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1920);
    CHECK(pc_display_apply((PcDisplayMode){1280,720}));pc_display_host_size(3840,2160);
    fake_ticks+=300;pc_display_frame_done();CHECK(pc_display_effective().width==1280);
    CHECK(pc_display_apply((PcDisplayMode){1,1}));CHECK(pc_display_effective().width==3840);
    puts("PASS: shared Auto drawable, fallback, filtering, persistence, failure retry and manual override");return 0;
}
