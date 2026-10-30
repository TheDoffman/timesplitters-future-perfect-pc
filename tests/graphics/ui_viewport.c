#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pc_ui_layout.h"
#define TSFP_PC_DISPLAY
#define TRUE 1
#define FALSE 0
typedef int BOOL;
typedef float FLOAT;
typedef struct {float X,Y,Width,Height,MinZ,MaxZ;} D3DVIEWPORT8;
typedef struct {float x,y,width,height,min_depth,max_depth;} RhiViewport;
typedef struct {int32_t left,top,right,bottom;} RhiRect;
enum {XBOX_D3D8_2D_AUTO,XBOX_D3D8_2D_STRETCH,XBOX_D3D8_2D_CENTRE,XBOX_D3D8_2D_LEFT,XBOX_D3D8_2D_RIGHT,XBOX_D3D8_2D_SIDE};
static struct {unsigned width,height;D3DVIEWPORT8 viewport;} g_device_state;
static int g_2d_squeeze=1,g_cur_rt,g_2d_anchor=XBOX_D3D8_2D_CENTRE,g_scissor_enabled=1;
static uint32_t g_2d_tag;
static int g_2d_placement;
static RhiRect g_scissor={20,20,620,460};
static RhiViewport actual;
typedef struct {unsigned width,height;} Mode;
static Mode pc_display_effective(void){Mode m={g_device_state.width,g_device_state.height};return m;}
static float pc_display_hud_scale(void){return 4.f*g_device_state.height/(3.f*g_device_state.width);}
static float rt_scale_x(void){return g_device_state.width/640.f;}
static float rt_scale_y(void){return g_device_state.height/480.f;}
static int rhi_device_ready(void){return 1;}
static void rhi_set_viewports(int n,const RhiViewport *v){actual=*v;}
static PcUiWorldAnchor g_2d_world_anchor;
/* PRODUCTION */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: line %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 const unsigned sizes[][2]={{640,480},{1280,1024},{1920,1080},{1920,1200},{3440,1440},{5120,1440},{7680,4320}};
 for(unsigned i=0;i<7;i++)for(int anchor=2;anchor<=4;anchor++){
  float w=sizes[i][0],h=sizes[i][1],scale=fminf(w/640,h/480);
  g_device_state.width=sizes[i][0];g_device_state.height=sizes[i][1];
  g_device_state.viewport=(D3DVIEWPORT8){0,0,640,480,0,1};g_2d_anchor=anchor;
  g_2d_squeeze=1;apply_host_viewport();RhiRect clip;CHECK(d3d8_GetScissor(&clip));
  CHECK(fabsf(actual.width/640-actual.height/480)<.001f);
  CHECK(fabsf(actual.width/640-scale)<.001f);
  CHECK(fabsf(clip.left-actual.x-20*scale)<1.1f);
  CHECK(fabsf(clip.right-actual.x-620*scale)<1.1f);
  CHECK(fabsf(clip.top-actual.y-20*scale)<1.1f);
  CHECK(fabsf(clip.bottom-actual.y-460*scale)<1.1f);
  if(anchor==3)CHECK(actual.x==0);
  if(anchor==4)CHECK(fabsf(actual.x+actual.width-w)<.001f);
  g_2d_squeeze=0;apply_host_viewport();CHECK(actual.x==0 && actual.y==0 && actual.width==w && actual.height==h);
 }
 /* Captured crane clip: 639/479 must cover the full physical right/bottom.
    Unrelated tags and partial clips must keep their original bounds. */
 g_2d_squeeze=0;g_2d_placement=1;g_2d_tag=0x137010;
 for(unsigned i=0;i<7;i++)for(unsigned inclusive=0;inclusive<2;inclusive++){
  g_device_state.width=sizes[i][0];g_device_state.height=sizes[i][1];
  g_scissor=(RhiRect){0,0,inclusive?639:640,inclusive?479:480};
  RhiRect stored=g_scissor,clip;CHECK(d3d8_GetScissor(&clip));
  CHECK(clip.left==0 && clip.top==0 && clip.right==sizes[i][0] && clip.bottom==sizes[i][1]);
  CHECK(!memcmp(&stored,&g_scissor,sizeof stored));
  g_2d_tag=0;d3d8_GetScissor(&clip);
  CHECK(clip.right==(int)(stored.right*rt_scale_x()));
  CHECK(clip.bottom==(int)(stored.bottom*rt_scale_y()));g_2d_tag=0x137010;
  g_scissor.left=1;d3d8_GetScissor(&clip);
  CHECK(clip.right==(int)(stored.right*rt_scale_x()));
 }
 /* The marker centre stays at its world-projected pixel while both axes
    use the same pixel scale. Labels retain the outline anchor. */
 for(unsigned i=0;i<7;i++)for(int position=0;position<=10;position++)
 for(unsigned half_width=4;half_width<=64;half_width*=4) {
  float centre=64.f*position,vertices[]={centre-half_width,centre+half_width};
  float label[]={centre+15,centre+22};
  xbox_D3D8SetTwoDPlacement(2,0x142d10);
  CHECK(pc_ui_world_draw(&g_2d_world_anchor,vertices,4,2,0,640));
  float saved=g_2d_world_anchor.x;
  CHECK(!pc_ui_world_draw(&g_2d_world_anchor,label,4,2,0,640));
  CHECK(g_2d_world_anchor.x==saved);
  g_device_state.width=sizes[i][0];g_device_state.height=sizes[i][1];
  g_device_state.viewport=(D3DVIEWPORT8){0,0,640,480,0,1};
  g_2d_anchor=2;g_2d_squeeze=1;apply_host_viewport();
  CHECK(fabsf(actual.x+centre*actual.width/640-centre*sizes[i][0]/640)<.002f);
  CHECK(fabsf(actual.width/640-actual.height/480)<.001f);
  /* Another objective in the same frame must acquire its own anchor. */
  float next[]={640-centre-12,640-centre+12};
  xbox_D3D8SetTwoDPlacement(2,0x142d10);
  CHECK(!g_2d_world_anchor.ready);
  CHECK(pc_ui_world_draw(&g_2d_world_anchor,next,4,2,0,640));
  apply_host_viewport();
  CHECK(fabsf(actual.x+(640-centre)*actual.width/640-
              (640-centre)*sizes[i][0]/640)<.002f);
  xbox_D3D8SetTwoDPlacement(2,0);
  CHECK(!g_2d_world_anchor.active && !g_2d_world_anchor.ready);
  CHECK(!pc_ui_world_draw(&g_2d_world_anchor,vertices,4,2,0,640));
 }
 pc_ui_world_begin(&g_2d_world_anchor,2,0x142d10);
 float invalid[]={NAN,10};
 CHECK(!pc_ui_world_draw(&g_2d_world_anchor,invalid,4,2,0,640));
 CHECK(!g_2d_world_anchor.ready);
 puts("PASS: production viewport/scissor agreement at seven ratios and every anchor");return 0;
}
