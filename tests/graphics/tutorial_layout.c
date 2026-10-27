/* Exercise the production tutorial hook with measured font widths and guest ABI. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#define RECOMP_TLS
#define TSFP_PC_PROMPTS_H
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
static uint32_t memory[0x800000/4],g_esp;
#define WORD_AT(a) memory[(a)/4]
static int keyboard=1,players=1,font=7,measurements,expected_growth;
static float widths[4]={84,24,24,64},expected_extra;
static int guest_writable(uint32_t a,size_t n){return a && a+n<sizeof memory;}
int pc_prompts_keyboard_active(void){return keyboard;}
void pc_prompts_layout_begin(void){}
void pc_prompts_layout_end(void){}
static volatile int marker;
void sub_001CDAE0(void){marker=1;}
void sub_00147080(void){marker=2;}
void sub_00042D10(void){marker=3;}
void sub_0003FA10(void){marker=4;}
void sub_0003FCF0(void){marker=5;}
void sub_0003F8D0(void){marker=6;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t a[4]={0};va_list ap;va_start(ap,count);
    for(unsigned i=0;i<count;i++)a[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_0003F8D0)return 0;
    if(fn==sub_001CDAE0)return players;
    if(fn==sub_00147080)return a[0];
    if(fn==sub_00042D10){WORD_AT(a[1])=a[0];return 0;}
    if(fn==sub_0003FA10){int old=font;font=a[0];return old;}
    if(fn==sub_0003FCF0){
        CHECK(font==18);float scale;memcpy(&scale,&WORD_AT(0x7e7270),4);
        float w=widths[WORD_AT(a[2])]*scale;
        memcpy(&WORD_AT(a[0]),&w,4);measurements++;return 0;
    }
    CHECK(0);return 0;
}
static int ui_placement=4,wide=1,placement_changes;
static uint32_t ui_tag=123;
static float hud_scale=.56f;
int pc_display_wide_now(void){return wide;}
float pc_display_hud_scale(void){return hud_scale;}
static void ui_place(int value,uint32_t tag){ui_placement=value;ui_tag=tag;placement_changes++;}
static int footer_hover;
static int menu_mouse_profile_text(void (*original)(void)) { (void)original;return 0; }
static int menu_mouse_footer_text(void) { return footer_hover; }
#include "pc_tutorial_ui.inc"
static float seen;
static uint32_t seen_color,seen_tag;
static int seen_placement;
static void text_original(void){seen_placement=ui_placement;seen_tag=ui_tag;seen=tutorial_float(g_esp+4);seen_color=WORD_AT(g_esp+16);g_esp+=4;}
static void wrap_original(void){seen=tutorial_float(g_esp+8);g_esp+=4;}
static void original(void)
{
    uint32_t frame=g_esp,box=0x7a5940;
    CHECK(fabsf(tutorial_float(0x7e7270)-(players==1?.9f:1.f))<.0001f);
    CHECK(fabsf(tutorial_float(0x7e7280)-(players==1?.9f:1.f))<.0001f);
    CHECK(ui_placement==((players==1 && wide && hud_scale<1.f)?3:4));
    if(ui_placement==3)CHECK(ui_tag==0x1476f0);
    CHECK(font==7);CHECK(WORD_AT(box+0xec)==270+expected_growth);
    CHECK(fabsf(tutorial_extra-expected_extra)<.01f);
    /* Each normal action uses the same widest-label column, including wraps. */
    for(unsigned i=0;i<4;i++){
        g_esp=frame-128;WORD_AT(g_esp)=0x147ce4;
        tutorial_set_float(g_esp+8,230.f+expected_growth);
        uint32_t saved=WORD_AT(g_esp+8);
        pc_display_tutorial_wrap(wrap_original);
        CHECK(fabsf(seen-(230.f+expected_growth-expected_extra))<.01f);
        CHECK(WORD_AT(frame-120)==saved && g_esp==frame-124);
        g_esp=frame-128;WORD_AT(g_esp)=0x147d17;tutorial_set_float(g_esp+4,99);
        pc_display_tutorial_text(text_original);
        CHECK(fabsf(seen-(99+expected_extra))<.01f && tutorial_float(frame-124)==99);
        /* The widest key must end before the action begins. */
        if(expected_extra>0)CHECK(59+widths[i]*.9f+8<=seen+.01f);
    }
    /* Compound stick descriptions and unrelated font calls keep their layout. */
    g_esp=frame-128;WORD_AT(g_esp)=0x147c51;tutorial_set_float(g_esp+8,150);
    pc_display_tutorial_wrap(wrap_original);CHECK(seen==150);
    g_esp=frame-128;WORD_AT(g_esp)=0x147d17;tutorial_set_float(g_esp+4,100);
    pc_display_tutorial_text(text_original);CHECK(seen==100);
    g_esp=frame-128;WORD_AT(g_esp)=0x12345;tutorial_set_float(g_esp+4,100);
    tutorial_normal_row=1;pc_display_tutorial_text(text_original);CHECK(seen==100);tutorial_normal_row=0;
    g_esp=frame+4;
}
static void run(float extra,int growth)
{
    int changes=placement_changes;
    expected_extra=extra;expected_growth=growth;g_esp=0x2000;
    WORD_AT(g_esp+4)=0;pc_display_tutorial(original);
    CHECK(tutorial_float(0x7e7270)==1.f && tutorial_float(0x7e7280)==1.f);
    CHECK(g_esp==0x2004 && WORD_AT(0x7a5a2c)==270);
    CHECK(tutorial_extra==0 && tutorial_normal_row==0 && font==7);
    CHECK(ui_placement==4 && ui_tag==123);
    CHECK(placement_changes-changes==((players==1 && wide && hud_scale<1.f)?2:0));
}
int main(void)
{
    WORD_AT(0x7e71c8)=0x100;WORD_AT(0x104)=639;
    tutorial_set_float(0x4c0460,1);
    tutorial_set_float(0x7e7270,1);tutorial_set_float(0x7e7280,1);
    WORD_AT(0x7a5a20)=4;WORD_AT(0x7a5a34)=1;
    WORD_AT(0x7a5a24)=59;WORD_AT(0x7a5a2c)=270;
    for(unsigned i=0;i<4;i++){
        WORD_AT(0x7a5940+i*28+4)=18;
        WORD_AT(0x7a5940+i*28+12)=i;
        WORD_AT(0x7a5940+i*28+24)=UINT32_MAX;
    }
    run(43.6f,44);CHECK(measurements==4);
    /* Rebinding to a longer label changes all row columns immediately. */
    widths[1]=128;run(83.2f,84);
    /* Near an edge, growth is bounded and remaining description width wraps. */
    WORD_AT(0x104)=359;run(83.2f,15);WORD_AT(0x104)=639;
    /* Switching to a controller or split screen preserves original geometry. */
    keyboard=0;run(0,0);keyboard=1;players=2;run(0,0);players=1;
    /* No prompt rows must not resize ordinary tutorial prose. */
    for(unsigned i=0;i<4;i++)WORD_AT(0x7a5940+i*28+24)=0;
    run(0,0);
    wide=0;run(0,0);wide=1;hud_scale=1.25f;run(0,0);hud_scale=.56f;
    /* Hover changes only RGB for this draw; fades and later text are intact. */
    for(unsigned alpha=0;alpha<256;alpha+=17) {
        g_esp=0x2000;WORD_AT(g_esp)=0x2d91b9;WORD_AT(g_esp+16)=0x64646400u|alpha;
        footer_hover=1;pc_display_tutorial_text(text_original);
        CHECK(seen_color==(0x405a7f00u|alpha) && WORD_AT(0x2010)==(0x64646400u|alpha));
        g_esp=0x2000;footer_hover=0;pc_display_tutorial_text(text_original);
        CHECK(seen_color==(0x64646400u|alpha));
    }
    /* Loading text scales with the proportional UI and anchors to the left.
       Keep the original font/arguments and restore placement after every draw. */
    const unsigned sizes[][2]={{640,480},{1280,1024},{1920,1080},{1920,1200},
        {3440,1440},{5120,1440},{7680,4320},{1080,1920}};
    WORD_AT(0x118)=479;
    for(unsigned i=0;i<8;i++)for(unsigned alpha=0;alpha<128;alpha+=31) {
        g_esp=0x2000;WORD_AT(g_esp)=0x1560e5;
        tutorial_set_float(g_esp+4,45.f);tutorial_set_float(g_esp+8,60.f);
        WORD_AT(g_esp+12)=0x3000;WORD_AT(g_esp+16)=0x64646400u|alpha;
        hud_scale=4.f*sizes[i][1]/(3.f*sizes[i][0]);wide=hud_scale!=1.f;
        uint32_t args[5];memcpy(args,&WORD_AT(g_esp),sizeof args);
        int changes=placement_changes;
        pc_display_tutorial_text(text_original);
        CHECK(seen_placement==(wide?3:4) && seen_tag==(wide?0x1560e5:123));
        CHECK(seen==45.f && seen_color==(0x64646400u|alpha));
        CHECK(ui_placement==4 && ui_tag==123 && g_esp==0x2004);
        CHECK(!memcmp(args,&WORD_AT(0x2000),sizeof args));
        CHECK(placement_changes-changes==(wide?2:0));
        CHECK(tutorial_float(0x7e7270)==1.f && tutorial_float(0x7e7280)==1.f);
    }
    wide=1;
    const unsigned offsets[]={0,4,0x14,0x18};
    for(unsigned i=0;i<4;i++) {
        uint32_t save=WORD_AT(0x100+offsets[i]);WORD_AT(0x100+offsets[i])=100;
        g_esp=0x2000;pc_display_tutorial_text(text_original);CHECK(seen_placement==4);
        WORD_AT(0x100+offsets[i])=save;
    }
    WORD_AT(0x7e71c8)=0;g_esp=0x2000;pc_display_tutorial_text(text_original);CHECK(seen_placement==4);
    WORD_AT(0x7e71c8)=0x100;WORD_AT(0x2000)=0x12345;g_esp=0x2000;
    pc_display_tutorial_text(text_original);CHECK(seen_placement==4);
    puts("PASS: tutorial left-edge scope/restoration, measured columns, rebound labels, edge wrapping, controller/split preservation, stack/font/geometry restoration");
    return 0;
}
