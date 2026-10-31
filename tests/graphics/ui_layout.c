/* Execute production placement scopes with a simulated deferred draw queue. */
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pc_ui_layout.h"
#define RECOMP_TLS
static uint32_t stack[32],g_esp,g_esi,scene=101;
static float hud_scale=.75f;
static uint32_t font_xy[2],menu_data[0x240/4],item_data[0x90/4],descriptor=0x2d0be0;
static uint32_t projection[16],effect_data[32];
static int projection_sets;
static float drawn_projection[1024];
static uint32_t phase,mode;
static uint32_t view_address=0x100,view[9]={0,639,320,640,0,0,479,240,480};
static uint32_t *word(uint32_t a) {
    if(a>=0x4000 && a<0x4080)return &effect_data[(a-0x4000)/4];
    if(a==0x7e7270)return &font_xy[0];
    if(a==0x7e7280)return &font_xy[1];
    if(a>=0x1000 && a<0x1240)return &menu_data[(a-0x1000)/4];
    if(a>=0x2000 && a<0x2090)return &item_data[(a-0x2000)/4];
    if(a==0x3000)return &descriptor;
    if(a>=0x55fa40 && a<0x55fa80)return &projection[(a-0x55fa40)/4];
    if(a==0x79094c)return &scene;
    if(a==0x7e71c8)return &view_address;
    if(a==0x7de30c)return &phase;
    if(a==0x7de454)return &mode;
    if(a>=0x100 && a<0x124)return &view[(a-0x100)/4];
    return &stack[a/4];
}
#define WORD_AT(a) (*word(a))
static int guest_writable(uint32_t a,size_t n){return (a>=0x4000 && a<0x4080 && n<=0x4080-a) || (a==0x1000 && n<=sizeof menu_data) || (a==0x2000 && n<=sizeof item_data) || (a==0x3000 && n<=4) || (a==0x100 && n<=sizeof view) || (a==0x55fa40 && n<=sizeof projection);}
static int wide=1,players=1,placement,pending=-1,draw_count;
static int drawn[1024],selected[1024],selection_count;
static uint32_t tag;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL: %s line %d\n",#x,__LINE__);exit(1);} } while(0)
float pc_display_hud_scale(void) { return hud_scale; }
static int menu_filter_frontend;
void pc_display_menu_filter_begin(int frontend) { menu_filter_frontend=frontend; }
static int menu_font_filter;
int pc_display_menu_font_filter(int active)
{ int old=menu_font_filter;menu_font_filter=active;return old; }
int pc_display_wide_now(void) { return wide; }
void sub_00022FF0(void) { if(pending>=0){memcpy(&drawn_projection[draw_count],projection,4);drawn[draw_count++]=placement;pending=-1;} }
void sub_001CDAE0(void) {}
void sub_0003F8D0(void) {}
void sub_0001C5D0(void) { CHECK(pending<0);projection_sets++; }
static unsigned scope_masks,scope_rects[2][4],scope_blend=7,scope_mode=9;
static unsigned expected_scope_mode=1,expected_scope_colour=0x7f;
void sub_00019650(void) { scope_blend=31; }
void sub_00019D70(void) { scope_mode=33; }
void sub_0001EFB0(void) { CHECK(0); }
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    if(fn==sub_001CDAE0)return players;
    if(fn==sub_00019650 || fn==sub_00019D70) {
        CHECK(count==1 && pending<0);
        unsigned *state=fn==sub_00019650?&scope_blend:&scope_mode,old=*state;
        va_list args;va_start(args,count);*state=va_arg(args,unsigned);va_end(args);return old;
    }
    if(fn==sub_0001EFB0) {
        CHECK(scope_blend==0 && scope_mode==expected_scope_mode);
        CHECK(placement==1 && count==5 && scope_masks<2);
        va_list args;va_start(args,count);
        for(unsigned i=0;i<4;i++)scope_rects[scope_masks][i]=va_arg(args,unsigned);
        CHECK(va_arg(args,unsigned)==expected_scope_colour);va_end(args);
        scope_masks++;pending=1;return 0;
    }
    fn();return 0;
}
void host_SetTwoDPlacement(int value,uint32_t t)
{ CHECK(pending<0);placement=value;tag=t;selected[selection_count++]=value; }
/* Character hit capture belongs to the menu adapter (covered by menu_mouse.c).
 * These placement fixtures have no active character carousel. */
static void menu_character_quad(void) {}
void pc_prompts_layout_begin(void){}
void pc_prompts_layout_end(void){}
#include "pc_display_ui.inc"
static void draw(void){pending=1;g_esp+=4;}
static void rank_widget(void)
{
    /* The label, rank number and frame can be separate deferred batches. */
    for(int part=0;part<3;part++){
        pending=1;
        if(part<2)sub_00022FF0();
    }
    g_esp+=4;
}
static void radar(void)
{
    CHECK(placement==0);pending=1; /* World targets precede the radar. */
    pc_display_radar_begin();CHECK(placement==4);pending=1;g_esp+=4;
}
static void health(void)
{
    float scale;memcpy(&scale,&stack[4],4);
    CHECK(scale<=.958334f);CHECK(placement==2);
    pending=1; /* The central hit marker precedes both side gauges. */
    for(int pass=0;pass<3;pass++)for(int side=0;side<2;side++){
        stack[13]=side;pc_display_health_side();
        CHECK(placement==(wide?(side?4:3):2));pending=1;
    }
    g_esp+=4;
}
static int32_t mask_seen[4];
static void mask_draw(void)
{
    CHECK(placement==1);
    for(unsigned i=0;i<4;i++)mask_seen[i]=(int32_t)stack[i+1];
    pending=1;g_esp+=4;
}
static float font_seen[2];
static int font_filter_seen;
static void font_draw(void)
{
    font_filter_seen=menu_font_filter;
    memcpy(font_seen,font_xy,sizeof font_seen);pending=1;g_esp+=4;
}
static float aim_seen[8];
static unsigned aim_offset,aim_points;
static int aim_anchor;
static void aim_draw(void)
{
    aim_anchor=placement;
    for(unsigned i=0;i<aim_points*2;i++)memcpy(&aim_seen[i],&WORD_AT(g_esp+aim_offset+i*4),4);
    pending=1;g_esp+=4;
}
static void test_aim_overlays(void)
{
    const unsigned sizes[][2]={{640,480},{1280,1024},{1920,1080},{1920,1200},
        {3440,1440},{5120,1440},{7680,4320}};
    const uint32_t callers[]={0x14cc10,0x1481e9,0x1482f7,0x136ffc,0x135f1e,
        0x135bb4,0x135c5a,0x135ca7,0x135cf6,0x135d58,0x135d95};
    for(unsigned mode=0;mode<7;mode++)for(unsigned call=0;call<11;call++) {
        float w=(float)sizes[mode][0],h=(float)sizes[mode][1];
        hud_scale=4.f*h/(3.f*w);wide=hud_scale!=1.f;g_esp=0;
        unsigned kind=call<4?0:call==4?1:2;
        aim_points=kind==1?4:2;aim_offset=kind==1?8:4;
        /* Deliberately displaced aim point, and a rotated quad. */
        float xy[8]={398,181,422,199,0,0,0,0};
        if(kind==1){float quad[8]={398,190,410,178,422,190,410,202};memcpy(xy,quad,sizeof xy);}
        memset(stack,0x5a,sizeof stack);stack[0]=callers[call];
        for(unsigned i=0;i<aim_points*2;i++)memcpy(&WORD_AT(aim_offset+i*4),&xy[i],4);
        uint32_t saved[32];memcpy(saved,stack,sizeof saved);
        ui_place(2,99);pending=1;int n=draw_count;
        pc_display_aim_primitive(aim_draw,kind);sub_00022FF0();
        CHECK(g_esp==4 && !memcmp(stack,saved,sizeof saved));
        CHECK(placement==2 && tag==99 && drawn[n]==2);
        if(wide && kind!=2) {
            CHECK(aim_anchor==1);
            float cx=0,cy=0;
            for(unsigned i=0;i<aim_points;i++){cx+=aim_seen[i*2];cy+=aim_seen[i*2+1];}
            CHECK(fabsf(cx/aim_points-410)<.001f && fabsf(cy/aim_points-190)<.001f);
            float scale=fminf(w/640,h/480);
            for(unsigned i=0;i<aim_points;i++) {
                CHECK(fabsf((aim_seen[i*2]-410)*w/640-(xy[i*2]-410)*scale)<.002f);
                CHECK(fabsf((aim_seen[i*2+1]-190)*h/480-(xy[i*2+1]-190)*scale)<.002f);
            }
        } else {
            CHECK(!memcmp(aim_seen,xy,aim_points*2*sizeof(float)));
            CHECK(aim_anchor==(wide?(call<8?3:4):2));
        }
    }
    wide=1;hud_scale=.75f;aim_points=2;aim_offset=4;
    for(unsigned mode=0;mode<4;mode++) {
        g_esp=0;stack[0]=mode==0?0x123456:0x14cc10;
        players=mode==1?2:1;view[0]=mode==2?10:0;view[4]=0;
        view_address=mode==3?0:0x100;
        int before=selection_count;pc_display_aim_primitive(aim_draw,0);
        CHECK(before==selection_count && g_esp==4);sub_00022FF0();
    }
    players=1;view[0]=0;view_address=0x100;
    for(unsigned i=0;i<2;i++){
        g_esp=0;pc_display_aim_panel(aim_draw,i?0x37a9d0:0x138f70);
        CHECK(aim_anchor==1 && placement==2 && tag==99);
    }
    ui_place(0,0);
}
int main(void)
{
    test_aim_overlays();
    /* Measurement and glyph rendering must see the same two-axis scale.
       Other menu rows, descriptions and gameplay retain their original size. */
    for(unsigned mode=0;mode<5;mode++) {
        float initial[2]={1.2f,.8f};memcpy(font_xy,initial,sizeof initial);
        g_esp=0;stack[1]=0x1000;stack[2]=0x2000;
        menu_data[1]=0x3000;item_data[0]=7;item_data[0x88/4]=0x1000;
        descriptor=mode==2?0x1234:0x2d0be0;
        if(mode==3)item_data[0]=9;
        scene=mode==4?1:101;
        pc_display_main_menu_font(font_draw,mode==0);
        CHECK(font_filter_seen==(mode==1) && menu_font_filter==0);
        float scale=mode<2?.75f:1.f;
        CHECK(fabsf(font_seen[0]-initial[0]*scale)<.00001f);
        CHECK(fabsf(font_seen[1]-initial[1]*scale)<.00001f);
        CHECK(!memcmp(font_xy,initial,sizeof initial) && g_esp==4);
        sub_00022FF0();
    }
    scene=101;
    for(unsigned i=0;i<2;i++){
        g_esp=0;stack[5]=i;pending=1;int n=draw_count;
        pc_display_ui(draw,0x1334d0);
        CHECK(drawn[n]==0 && drawn[n+1]==(i?4:3));
        CHECK(placement==0 && tag==0 && g_esp==4);
    }
    g_esp=0;int n=draw_count;pc_display_ui(radar,0x133ab0);
    CHECK(drawn[n]==0 && drawn[n+1]==4 && !radar_group && placement==0);
    int count=selection_count;pc_display_radar_begin();CHECK(count==selection_count);
    const uint32_t left[]={0x12f670,0x14b930,0x1409c0,0x13fc50};
    for(unsigned i=0;i<4;i++){g_esp=0;n=draw_count;pc_display_ui(draw,left[i]);CHECK(drawn[n]==3);}
    g_esp=0;n=draw_count;pc_display_ui(draw,0x1358a0);CHECK(drawn[n]==5 && placement==0);
    const uint32_t arcade_panels[]={0x12ec8b,0x12ea9b};
    for(unsigned panel=0;panel<2;panel++) {
        g_esp=0;stack[0]=arcade_panels[panel];n=draw_count;pending=1;
        pc_display_ui(rank_widget,0x12e3f0);
        CHECK(drawn[n]==0); /* Earlier HUD batches retain their own anchor. */
        /* Both complete panels must share an origin while simultaneously
           visible; the original compositor retains their spacing/animation. */
        for(int part=1;part<=3;part++)CHECK(drawn[n+part]==3);
        CHECK(placement==0 && tag==0 && g_esp==4);
        draw();sub_00022FF0();CHECK(drawn[n+4]==0);
    }
    /* The same renderer is used for timers and other counters. */
    g_esp=0;stack[0]=0x14f5cc;n=draw_count;int before_count=selection_count;
    pc_display_ui(rank_widget,0x12e3f0);sub_00022FF0();
    CHECK(selection_count==before_count);
    for(int part=0;part<3;part++)CHECK(drawn[n+part]==0);
    /* Scope decorations in the middle retain the central anchor on both hosts. */
    CHECK(pc_ui_side_anchor(20,120,640)==0);
    CHECK(pc_ui_side_anchor(520,620,640)==1);
    CHECK(pc_ui_side_anchor(300,340,640)==.5f);
    CHECK(pc_ui_side_anchor(0,640,640)==.5f);
    for(int mode=0;mode<2;mode++){
        wide=mode;g_esp=0;stack[2]=stack[3]=0;
        float scale=1.2f;memcpy(&stack[4],&scale,4);uint32_t saved=stack[4];
        n=draw_count;pc_display_health(health);
        CHECK(stack[4]==saved && g_esp==4 && placement==0 && !health_group);
        CHECK(drawn[n]==2); /* Hit direction never inherits a side anchor. */
        count=selection_count;pc_display_health_side();CHECK(count==selection_count);
    }
    for(int i=0;i<2;i++){
        wide=i;players=i?2:1;g_esp=0;count=selection_count;
        pc_display_ui(draw,0x1334d0);CHECK(count==selection_count && g_esp==4);
        sub_00022FF0();
        for(unsigned panel=0;panel<2;panel++) {
            g_esp=0;stack[0]=arcade_panels[panel];count=selection_count;
            pc_display_ui(rank_widget,0x12e3f0);
            CHECK(count==selection_count && g_esp==4 && placement==0);
            sub_00022FF0();
        }
    }
    players=2;g_esp=0;count=selection_count;stack[4]=0x3f99999a;
    pc_display_health(draw);
    CHECK(selection_count==count && stack[4]==0x3f99999a && g_esp==4);
    sub_00022FF0();
    /* Completed Results/Scores/Options suppress the actual draw wrapper,
       without disturbing its caller stack, scale or queued placement state. */
    phase=8;mode=11u<<8;g_esp=0;stack[0]=0x2e58c9;
    n=draw_count;count=selection_count;
    pc_display_health(draw);sub_00022FF0();
    CHECK(g_esp==4 && draw_count==n && selection_count==count && stack[4]==0x3f99999a);
    mode=10u<<8;g_esp=0;pc_display_health(draw);sub_00022FF0();
    CHECK(draw_count==n+1 && g_esp==4); /* Story objectives still draw. */
    phase=4;mode=11u<<8;g_esp=0;pc_display_health(draw);sub_00022FF0();
    CHECK(draw_count==n+2 && g_esp==4); /* Ordinary pause still draws. */
    phase=8;stack[0]=0x14d46d;g_esp=0;pc_display_health(draw);sub_00022FF0();
    CHECK(draw_count==n+3 && g_esp==4); /* Gameplay caller is unchanged. */
    /* Proportional pixels and original margins at every supported wide ratio. */
    const float sizes[][2]={{640,480},{1280,1024},{1920,1080},{1920,1200},{3440,1440},{5120,1440},{7680,4320}};
    for(unsigned i=0;i<7;i++){
        float w=sizes[i][0],h=sizes[i][1],scale=h/480.f,k=4*h/(3*w);
        CHECK(fabsf(pc_ui_scale_x(k)*w/640-pc_ui_scale_y(k)*h/480)<.0001f);
        float fit=pc_ui_health_scale(1.2f,0,0,640,480,320,240);
        CHECK(pc_ui_hide_results_health(0x2e58c9u,8,11));
        CHECK(pc_ui_hide_results_health(0x2e58c9u,8,9));
        CHECK(!pc_ui_hide_results_health(0x2e58c9u,8,10));
        CHECK(!pc_ui_hide_results_health(0x2e58c9u,4,11));
        CHECK(!pc_ui_hide_results_health(0x14d46du,8,11));
        CHECK(fit<1 && pc_ui_health_scale(.5f,0,0,640,480,320,240)==.5f);
        /* Full outer mesh stays inside its authored inset after anchoring. */
        CHECK(320-307.2f*fit>=25.59f && 320+307.2f*fit<=614.41f);
        if(k>1)continue;
        CHECK(fabsf((w-(640-20)*w/640*k-w*(1-k))-20*scale)<.001f);
    }
    /* Scope exterior coverage and aperture alignment at 4:3, 5:4, 16:9,
       16:10, ultrawide and high-resolution outputs; preserve guest ABI. */
    const uint32_t masks[]={0x136014,0x136032,0x13604d,0x13606e};
    const int32_t mask_rects[][4]={{0,0,161,480},{0,0,640,121},
                                  {480,0,640,480},{0,360,640,480}};
    for(unsigned r=0;r<7;r++)for(unsigned side=0;side<4;side++) {
        hud_scale=4*sizes[r][1]/(3*sizes[r][0]);wide=hud_scale!=1.f;players=1;
        g_esp=0;stack[0]=masks[side];memcpy(stack+1,mask_rects[side],16);
        ui_place(2,123);pending=1;n=draw_count;
        int handled=pc_display_scope_mask(mask_draw);
        CHECK(handled==(hud_scale!=1.f));
        if(handled) {
            CHECK(drawn[n]==2 && drawn[n+1]==1 && g_esp==4);
            CHECK(placement==2 && tag==123 && !memcmp(stack+1,mask_rects[side],16));
            unsigned inner=side==0?2:side==1?3:side==2?0:1;
            float centre=(inner&1)?240.f:320.f;
            float scale=(inner&1)?pc_ui_scale_y(hud_scale):pc_ui_scale_x(hud_scale);
            float expected=centre+(mask_rects[side][inner]-centre)*scale;
            CHECK(fabsf(mask_seen[inner]-expected)<1.001f);
            CHECK(mask_seen[side]==(side<2?0:side==2?640:480));
        }else {CHECK(g_esp==0);sub_00022FF0();}
    }
    for(unsigned guard=0;guard<4;guard++) {
        wide=1;hud_scale=.5f;players=guard==0?2:1;
        view[0]=guard==1?320:0;view_address=guard==2?0:0x100;
        g_esp=0;stack[0]=guard==3?0x136019:masks[0];count=selection_count;
        CHECK(!pc_display_scope_mask(mask_draw) && count==selection_count && g_esp==0);
    }
    view_address=0x100;view[0]=0;players=1;
    /* A scope can touch the authored edges and emit no exterior masks.
       Its complete group must still cover the extra host canvas. */
    for(unsigned r=0;r<7;r++) {
        hud_scale=4*sizes[r][1]/(3*sizes[r][0]);wide=hud_scale!=1.f;
        scope_masks=0;ui_place(4,99);g_esp=0;pending=1;n=draw_count;
        pc_display_scope(draw,0x136b40);
        sub_00022FF0();CHECK(placement==4 && tag==99 && g_esp==4);
        CHECK(scope_blend==7 && scope_mode==9);
        if(wide) {
            CHECK(drawn[n]==4 && drawn[n+1]==2 && drawn[n+2]==1);
            CHECK(scope_masks==2);
            unsigned axis=hud_scale<1?0:1,extent=axis?480:640;
            CHECK(scope_rects[0][axis]==0 && scope_rects[1][axis+2]==extent);
            CHECK(scope_rects[0][axis+2]==extent-scope_rects[1][axis]);
            float edge=extent*.5f*(1.f-(axis?pc_ui_scale_y(hud_scale):pc_ui_scale_x(hud_scale)));
            CHECK(scope_rects[0][axis+2]>=edge && scope_rects[0][axis+2]<edge+1.001f);
        }else CHECK(scope_masks==0);
    }
    /* Sci-Fi uses the original half-alpha destination mask before artwork,
       never an opaque RGB strip afterward. Existing modes must be restored. */
    expected_scope_mode=14;expected_scope_colour=0x40;
    for(unsigned r=0;r<7;r++) {
        hud_scale=4*sizes[r][1]/(3*sizes[r][0]);wide=hud_scale!=1.f;
        scope_masks=0;ui_place(4,99);g_esp=0;pending=1;n=draw_count;
        pc_display_scope(draw,0x1364a0);
        sub_00022FF0();CHECK(placement==4 && tag==99 && g_esp==4);
        CHECK(scope_blend==7 && scope_mode==9);
        if(wide) {
            CHECK(scope_masks==2);
            CHECK(drawn[n]==4 && drawn[n+1]==1 && drawn[n+2]==2);
        } else CHECK(scope_masks==0);
    }
    /* Only its full-view destination-alpha composite stretches. The pass
       keeps its arguments, flushes, and restores the proportional artwork. */
    for(unsigned guard=0;guard<3;guard++) {
        ui_place(guard==2?1:2,guard==1?0x136b40:0x1364a0);
        g_esp=0;stack[0]=0x136856;pending=1;n=draw_count;
        int handled=pc_display_scope_mask(draw);
        CHECK(handled==(guard==0));
        if(handled) {
            CHECK(g_esp==4 && placement==2 && tag==0x1364a0);
            CHECK(drawn[n]==2 && drawn[n+1]==1 && stack[0]==0x136856);
        } else CHECK(g_esp==0);
        sub_00022FF0();
    }
    expected_scope_mode=1;expected_scope_colour=0x7f;
    /* The footer's three deferred primitives fill wide screens; earlier and
       later text batches retain their placement, even inside another scope. */
    const uint32_t footer[]={0x2d90eb,0x2d911b,0x2d9148};
    for(unsigned i=0;i<3;i++)for(unsigned r=0;r<7;r++)for(unsigned inclusive=0;inclusive<2;inclusive++){
        view[1]=640-inclusive;view[6]=480-inclusive;
        hud_scale=4*sizes[r][1]/(3*sizes[r][0]);wide=hud_scale!=1.f;
        ui_place(2,123);g_esp=0;stack[0]=footer[i];pending=1;n=draw_count;
        pc_display_footer_background(draw);sub_00022FF0();
        if(hud_scale<1.f){CHECK(drawn[n]==2 && drawn[n+1]==1);}
        else CHECK(drawn[n]==2);
        CHECK(placement==2 && tag==123 && g_esp==4);
        draw();sub_00022FF0();CHECK(drawn[draw_count-1]==2);
    }
    /* Shared helpers elsewhere, gameplay, split viewports and bad pointers
       must preserve the original layout. */
    wide=1;hud_scale=.75f;
    for(int guard=0;guard<5;guard++){
        g_esp=0;stack[0]=guard==0?0x2d9000:footer[0];scene=guard==1?100:101;
        view[0]=guard==2?320:0;view[6]=guard==3?239:479;
        view_address=guard==4?0:0x100;count=selection_count;
        pc_display_footer_background(draw);sub_00022FF0();
        CHECK(count==selection_count && placement==2 && g_esp==4);
    }
    view_address=0x100;view[0]=0;view[1]=639;view[6]=479;scene=2;
    const uint32_t objectives[]={0x2f3aed,0x2f3b0f,0x2f3b48,0x2f3b77,0x2f3ba9,0x2f3bdc};
    for(unsigned i=0;i<6;i++) {
        players=1;g_esp=0;stack[0]=objectives[i];pending=1;n=draw_count;
        pc_display_footer_background(draw);sub_00022FF0();
        CHECK(drawn[n]==2 && drawn[n+1]==1 && placement==2 && tag==123);
        players=2;g_esp=0;count=selection_count;
        pc_display_footer_background(draw);sub_00022FF0();CHECK(count==selection_count);
    }
    players=1;
    /* The blue sheet widens independently of the camera and subsequent
       artwork; its projection must be restored without cumulative scaling. */
    view_address=0x100;view[0]=0;view[1]=640;view[6]=480;scene=101;
    float original_projection=.8f;memcpy(projection,&original_projection,4);
    for(unsigned r=0;r<7;r++)for(int repeat=0;repeat<2;repeat++){
        hud_scale=4*sizes[r][1]/(3*sizes[r][0]);wide=hud_scale!=1.f;
        g_esp=0;stack[0]=0x2da51b;pending=1;n=draw_count;count=projection_sets;
        pc_display_menu_backdrop(draw);sub_00022FF0();
        CHECK(menu_filter_frontend==1);
        if(hud_scale<1.f){
            CHECK(drawn_projection[n]==original_projection);
            CHECK(fabsf(drawn_projection[n+1]-original_projection/hud_scale)<.00001f);
            CHECK(projection_sets==count+2);
        } else CHECK(projection_sets==count);
        CHECK(!memcmp(projection,&original_projection,4) && g_esp==4);
        draw();sub_00022FF0();CHECK(drawn_projection[draw_count-1]==original_projection);
    }
    wide=1;hud_scale=.5f;
    for(int guard=0;guard<4;guard++){
        g_esp=0;stack[0]=guard==0?0x2da564:0x2da51b;
        scene=guard==1?1:101;view[1]=guard==2?320:640;hud_scale=guard==3?0.f:.5f;
        count=projection_sets;pc_display_menu_backdrop(draw);sub_00022FF0();
        CHECK(menu_filter_frontend==(scene>100));
        CHECK(projection_sets==count && !memcmp(projection,&original_projection,4));
    }
    /* Projected Assault brackets use the full world canvas at every wide
       ratio, flushing queued HUD draws and restoring the parent scope. */
    for(unsigned r=0;r<7;r++)for(int split=1;split<=2;split++) {
        hud_scale=4*sizes[r][1]/(3*sizes[r][0]);wide=hud_scale!=1.f;
        players=split;ui_place(2,123);g_esp=0;pending=1;n=draw_count;
        pc_display_ui(draw,0x142d10);sub_00022FF0();
        CHECK(drawn[n]==2 && placement==2 && tag==123 && g_esp==4);
        if(wide && hud_scale<1.f && split==1)CHECK(drawn[n+1]==2);
        else CHECK(draw_count==n+1);
        draw();sub_00022FF0();CHECK(drawn[draw_count-1]==2);
    }
    puts("PASS: queued widget boundaries, radar world-marker separation, anchors, defaults and split-screen guard");
}
