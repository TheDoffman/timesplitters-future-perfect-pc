/* Execute the legacy GL adapter with the captured crane clipping bounds. */
#include <stdio.h>
#include <stdlib.h>
#include "pc_ui_layout.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %d %s\n",__LINE__,#x);exit(1);}}while(0)
#define GL_SCISSOR_TEST 1
static struct {unsigned backbuf_w,backbuf_h;} g={640,480};
static struct {int x1,y1,x2,y2;} g_scissor_rect={0,0,639,479};
static int native_scene_active=1,g_scissor_count=1,g_scissor_exclusive;
static int native_placement=1,enabled,box[4];
static unsigned native_scene_w,native_scene_h,native_target_height=480;
static uint32_t native_placement_tag=0x137010;
static double native_hud_anchor=.5,native_hud_k=1,native_hud_ky=1;
static void glEnable(int v){enabled=1;}
static void glDisable(int v){enabled=0;}
static void glScissor(int x,int y,int w,int h){box[0]=x;box[1]=y;box[2]=w;box[3]=h;}
/* PRODUCTION */
int main(void){
 const unsigned sizes[][2]={{640,480},{1920,1080},{3440,1440},{5120,1440},{7680,4320}};
 for(unsigned i=0;i<5;i++)for(int right=639;right<=640;right++)for(int bottom=479;bottom<=480;bottom++){
  native_scene_w=sizes[i][0];native_scene_h=sizes[i][1];
  g_scissor_rect.x2=right;g_scissor_rect.y2=bottom;
  native_apply_scissor();CHECK(enabled && box[0]==0 && box[1]==0 && box[2]==native_scene_w && box[3]==native_scene_h);
  CHECK(g_scissor_rect.x2==right && g_scissor_rect.y2==bottom);
 }
 int32_t original[]={0,0,639,479};
 for(unsigned i=0;i<8;i++){
  int32_t rect[4];memcpy(rect,original,sizeof rect);
  uint32_t tag=0x137010;int placement=1;
  if(i==0)tag=0;if(i==1)placement=2;
  if(i==2)rect[0]=1;if(i==3)rect[1]=1;
  if(i==4)rect[2]=319;if(i==5)rect[3]=239;
  if(i==6)rect[2]=641;if(i==7)rect[3]=481;
  int32_t before[4];memcpy(before,rect,sizeof before);pc_ui_vignette_clip(tag,placement,rect);
  CHECK(!memcmp(before,rect,sizeof rect));
 }
 g_scissor_count=0;native_apply_scissor();CHECK(!enabled);
 g_scissor_count=1;g_scissor_exclusive=1;native_apply_scissor();CHECK(!enabled);
 puts("PASS: production GL crane clip covers all pixels and preserves unrelated clips");return 0;
}
