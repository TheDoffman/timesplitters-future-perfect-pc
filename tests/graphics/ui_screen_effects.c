#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "pc_ui_layout.h"
#define TSFP_PC_DISPLAY
#define TRUE 1
#define FALSE 0
typedef int BOOL;
typedef unsigned UINT;
typedef uint32_t DWORD;
typedef int32_t LONG;
static struct {DWORD vertex_shader;} g_device_state={1};
static struct {LONG left,top,right,bottom;} g_scissor={0,0,640,480};
static struct {int centre_2d;} policy={1};
static struct {unsigned width;} mode={3440};
static int g_extent_fixed_function,g_extent_full_canvas,g_scissor_enabled,g_cur_rt;
static unsigned pos_offset;
static int screen_pass;
static UINT d3d8_GetGuestWidth(void){return 640;}
static UINT d3d8_GetGuestHeight(void){return 480;}
static int d3d8_vsh_is_programmable(DWORD h){return h==1;}
static int d3d8_vsh_bound_pos_input(void){return 0;}
static int d3d8_vsh_bound_input_offset(int reg,UINT *offset){*offset=pos_offset;return 1;}
static int d3d8_fvf_transformed(DWORD h){return h==2;}
static BOOL draw_is_screen_pass(void){return screen_pass;}
#define pc_display_effective() mode
#define d3d8_display_policy() (&policy)
/* PRODUCTION */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    /* Captured first-mission textured noise quad, inclusive pixel bounds. */
    float quad[4][3]={{0,0,.001f},{639,0,.001f},{639,479,.001f},{0,479,.001f}};
    for(unsigned shader=1;shader<=2;shader++) {
        g_device_state.vertex_shader=shader;
        CHECK(d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));
        g_scissor_enabled=1;g_scissor.left=20;g_scissor.right=620;
        CHECK(!d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));
        g_scissor.left=0;g_scissor.right=640;g_scissor.top=60;
        CHECK(!d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));
        g_scissor.top=0;g_scissor_enabled=0;
        quad[2][1]=quad[3][1]=60; /* Wide textured banner is still a widget. */
        CHECK(!d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));
        screen_pass=1; /* Existing flat fades/bars still cover the width. */
        CHECK(d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));screen_pass=0;
        quad[2][1]=quad[3][1]=479;
    }
    /* Separate corner HUD widgets span the canvas without filling it. */
    float hud[8][2]={{0,0},{30,0},{30,30},{0,30},{610,450},{639,450},{639,479},{610,479}};
    CHECK(!d3d8_draw_escapes_squeeze(hud,sizeof hud[0],8));
    CHECK(!pc_ui_full_canvas_quad(quad,sizeof quad[0],3,0,640,480));
    CHECK(!pc_ui_full_canvas_quad(quad,4,4,0,640,480));
    CHECK(!pc_ui_full_canvas_quad(quad,12,4,8,640,480));
    quad[1][0]=NAN;
    CHECK(!d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));quad[1][0]=639;
    unsigned char padded[4][17];
    for(unsigned i=0;i<4;i++)memcpy(padded[i]+1,quad[i],12);
    g_device_state.vertex_shader=1;pos_offset=1;
    CHECK(d3d8_draw_escapes_squeeze(padded,17,4));pos_offset=0;
    mode.width=0; /* Other titles retain their existing conservative policy. */
    CHECK(!d3d8_draw_escapes_squeeze(quad,sizeof quad[0],4));
    puts("PASS: production textured overlay classification, scissor, HUD, declaration and non-TSFP guards");
    return 0;
}
