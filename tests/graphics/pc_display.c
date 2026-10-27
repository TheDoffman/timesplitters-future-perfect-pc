#include "pc_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int available=1;
HWND xbox_D3D8Window(void) { return NULL; }
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) { (void)w;(void)h;return available; }
int main(int argc,char **argv)
{
    PcDisplayMode m={3840,2160}, list[64], bad={7680,4321}; unsigned w,h; int n,i,j;
    CHECK(argc==2); CHECK(_putenv_s("TSFP_DISPLAY_CONFIG",argv[1])==0);
    { FILE *f=fopen(argv[1],"wb"); CHECK(f); fputs("TSFP_DISPLAY_V1 0 0\n",f); fclose(f); }
    CHECK(!pc_display_menu_filter());
    pc_display_menu_filter_begin(1);CHECK(pc_display_menu_filter());
    CHECK(pc_display_menu_font_filter(1)==0);CHECK(pc_display_menu_filter()==2);
    CHECK(pc_display_menu_font_filter(0)==1);CHECK(pc_display_menu_filter()==1);
    pc_display_frame_done();CHECK(!pc_display_menu_filter());
    pc_display_menu_filter_begin(0);CHECK(!pc_display_menu_filter());
    CHECK(pc_display_valid(m)); CHECK(!pc_display_valid(bad));
    bad.width=0; bad.height=1080; CHECK(!pc_display_valid(bad));
    bad.width=5120; bad.height=1440; CHECK(pc_display_valid(bad));
    bad.width=3440; CHECK(pc_display_valid(bad));
    bad.width=7680; bad.height=2160; CHECK(pc_display_valid(bad));
    CHECK(pc_display_current().width==0);
    CHECK(pc_display_apply(m)); pc_display_scene_size(640,480,&w,&h);
    CHECK(w==3840 && h==2160);
    pc_display_shape(&w,&h); CHECK(w==3840 && h==2160);
    CHECK(fabsf(pc_display_camera_factor()-4.f/3.f)<1e-5f);
    pc_display_shape(&w,&h); CHECK(w==3840 && h==2160);
    CHECK(pc_display_wide_now()); CHECK(fabsf(pc_display_hud_scale()-.75f)<1e-5f);
    pc_display_frame_done(); CHECK(pc_display_wide_now());
    pc_display_shape(&w,&h); CHECK(w==3840 && h==2160);
    { const PcDisplayMode ratios[]={{1280,960},{1920,1080},{1920,1200},{3440,1440},{5120,1440}};
      for(int r=0;r<sizeof ratios/sizeof ratios[0];r++) {
        CHECK(pc_display_apply(ratios[r]));pc_display_frame_done();pc_display_shape(&w,&h);
        CHECK(w==ratios[r].width && h==ratios[r].height);
        /* Screen text keeps equal physical X/Y scale on the selected canvas. */
        CHECK(fabsf(pc_display_hud_scale()*w/640.f-h/480.f)<.0001f);
      }
      CHECK(pc_display_apply((PcDisplayMode){1280,1024}));pc_display_frame_done();pc_display_shape(&w,&h);
      CHECK(w==1280 && h==1024 && pc_display_wide_now()); /* Vertical widget fit. */
      CHECK(pc_display_apply(m)); }
    CHECK(pc_display_valid((PcDisplayMode){7680,4320}));
    available=0; bad.width=1920; bad.height=1080;
    CHECK(!pc_display_apply(bad)); CHECK(pc_display_current().width==3840);
    CHECK(pc_display_save());
    { FILE *f=fopen(argv[1],"rb"); char line[100]; CHECK(f); CHECK(fgets(line,sizeof line,f)); fclose(f);
      CHECK(!strcmp(line,"TSFP_DISPLAY_V1 3840 2160\n")); }
    bad.width=640; bad.height=480; CHECK(pc_display_valid(bad));
    bad.width=800; bad.height=600; CHECK(pc_display_valid(bad));
    n=pc_display_modes(NULL,list,64); CHECK(n>=1 && n<=64);
    CHECK(list[0].width==0 && list[0].height==0);
    for(i=0;i<n;i++) { CHECK(pc_display_valid(list[i]));
        for(j=0;j<i;j++) CHECK(list[i].width!=list[j].width || list[i].height!=list[j].height); }
    CHECK(pc_display_modes(NULL,list,1)==1); CHECK(list[0].width==0 && list[0].height==0);
    CHECK(pc_display_modes(NULL,list,0)==0); CHECK(pc_display_modes(NULL,NULL,64)==0);
    puts("PASS: resolution bounds, mode deduplication, failed apply, scene size and persistence");
    return 0;
}
