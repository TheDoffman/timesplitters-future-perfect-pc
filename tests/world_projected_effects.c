#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define BOOL int
#define TRUE 1
#define FALSE 0
typedef unsigned UINT;
#define XBOX_D3D8_2D_AUTO 0
#define XBOX_D3D8_2D_STRETCH 1
#define XBOX_D3D8_2D_CENTRE 2
#define XBOX_D3D8_2D_LEFT 3
#define XBOX_D3D8_2D_RIGHT 4
#define XBOX_D3D8_2D_SIDE 5
static int g_2d_screen_space=1,g_2d_squeeze=1,g_2d_placement=XBOX_D3D8_2D_AUTO,g_2d_anchor;
static BOOL two_d_tag_log(void){return FALSE;}
static void two_d_tag_note(const void *v,UINT s,UINT n){(void)v;(void)s;(void)n;}
static void d3d8_SetTwoDSqueeze(BOOL squeeze){g_2d_squeeze=squeeze;}
static int two_d_side_anchor(const void *v,UINT s,UINT n){(void)v;(void)s;(void)n;return 0;}
static void apply_host_viewport(void){}
static BOOL d3d8_draw_escapes_squeeze(const void *v,UINT s,UINT n){(void)v;(void)s;(void)n;return FALSE;}
/* PRODUCTION */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    struct V24 {float x,y,z;uint32_t color;float u,v;} shield[20]={0};
    struct V16 {float x,y,z,w;} health[6]={0};
    struct V24 hud[20]={0};
    /* Captured enemy shield: 20 projected strip vertices, 24-byte stride,
       first vertex (317.417,143.83,0.978526). */
    shield[0].x=317.417f;shield[0].y=143.83f;shield[0].z=.978526f;
    d3d8_place_2d_draw(shield,sizeof shield[0],20);
    CHECK(!g_2d_squeeze);
    g_2d_squeeze=TRUE;health[0].z=.98f;
    d3d8_place_2d_draw(health,sizeof health[0],6);
    CHECK(!g_2d_squeeze);
    g_2d_squeeze=TRUE;
    d3d8_place_2d_draw(health,sizeof health[0],5);
    CHECK(g_2d_squeeze);
    g_2d_squeeze=TRUE;hud[0].z=.001f;
    d3d8_place_2d_draw(hud,sizeof hud[0],20);
    CHECK(g_2d_squeeze);
    g_2d_placement=XBOX_D3D8_2D_CENTRE;g_2d_squeeze=TRUE;
    d3d8_place_2d_draw(shield,sizeof shield[0],20);
    CHECK(g_2d_squeeze);
    g_2d_placement=XBOX_D3D8_2D_AUTO;g_2d_screen_space=FALSE;
    d3d8_place_2d_draw(shield,sizeof shield[0],20);
    CHECK(g_2d_squeeze);
    puts("PASS: captured shield and health-bar placement; HUD and explicit placement preserved");
    return 0;
}
