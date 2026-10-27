/* Run the actual draw wrapper against changing full/split viewports. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pc_objectives.h"
#define TSFP_PC_CONTROLS
static uint8_t memory[0x800000];
static uint32_t g_esp;
#define FP_GUEST(a) (memory+(uint32_t)(a))
#define FP_MEM32(a) (*(uint32_t *)FP_GUEST(a))
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while(0)
static float rendered;
static unsigned draws, placements;
void sub_0013AB60_gen(void) {
    memcpy(&rendered,FP_GUEST(g_esp+16),4);++draws;g_esp+=4;
}
/* Placement scopes and geometry are exercised by test_ui_layout. */
static void pc_display_health(void (*original)(void)) { ++placements; original(); }
/* PRODUCTION_LAYOUT */
static float panel_width[2];
void sub_002F5E60_gen(void) {
    memcpy(&panel_width[0],FP_GUEST(0x47D1F4),4);
    memcpy(&panel_width[1],FP_GUEST(0x478628),4);
    ++draws;g_esp+=4;
}
/* PRODUCTION_PANEL */
static void panel(unsigned mode) {
    float widths[2]={80.f,90.f};
    memcpy(FP_GUEST(0x47D1F4),&widths[0],4);
    memcpy(FP_GUEST(0x478628),&widths[1],4);
    FP_MEM32(0x7DE454)=mode<<8;g_esp=0x10000;draws=0;
    sub_002F5E60();
    CHECK(draws==1 && g_esp==0x10004);
    CHECK(memcmp(FP_GUEST(0x47D1F4),&widths[0],4)==0);
    CHECK(memcmp(FP_GUEST(0x478628),&widths[1],4)==0);
}
static void draw(unsigned caller,float scale,unsigned mode) {
    g_esp=0x10000;FP_MEM32(g_esp)=caller;FP_MEM32(0x7DE454)=mode<<8;
    memcpy(FP_GUEST(g_esp+16),&scale,4);draws=0;placements=0;
    sub_0013AB60();
    CHECK(placements==1); /* Both Objectives and gameplay route through placement. */
    CHECK(draws==1 && g_esp==0x10004);
    CHECK(memcmp(FP_GUEST(0x10010),&scale,4)==0); /* Caller's argument restored. */
}
int main(void) {
    const unsigned shapes[][2]={{640,480},{1280,720},{1920,1080},{2560,1440},
        {3840,2160},{1920,1200},{3440,1440},{5120,1440},{720,1280}};
    unsigned s,n,i,phase;
    FP_MEM32(0x7B0C7C)=0x2000;FP_MEM32(0x7E71C8)=0x3000;FP_MEM32(0x2008)=0x3000-0x224; /* Camera contains the HUD viewport. */
    for(s=0;s<sizeof shapes/sizeof *shapes;s++)for(n=1;n<=4;n++)for(i=0;i<n;i++) {
        /* Guest viewports are independent of native output size. Include
         * changed guest dimensions too, rather than assuming 640x480. */
        int w=(int)shapes[s][0],h=(int)shapes[s][1],l=0,t=0;
        float halfw,halfh,mx,my,x,y,original;
        if(n==2) { h/=2;t=(int)i*h; }
        if(n>2) { w/=2;h/=2;l=(int)(i%2)*w;t=(int)(i/2)*h; }
        FP_MEM32(0x790950)=n;FP_MEM32(0x2004)=i;
        FP_MEM32(0x3000)=l;FP_MEM32(0x3004)=l+w-1;FP_MEM32(0x3014)=t;FP_MEM32(0x3018)=t+h-1;
        FP_MEM32(0x3008)=l+w/2;FP_MEM32(0x301C)=t+h/2;
        x=(float)l;y=(float)t;memcpy(FP_GUEST(0x10008),&x,4);memcpy(FP_GUEST(0x1000C),&y,4);
        halfw=(n>2 && !(n==3 && i==0))?162.f:307.2f;halfh=n==1?170.f:110.f;
        mx=w*.04f;my=h*.04f;
        for(phase=0;phase<3;phase++) {
            original=phase*.6f;draw(0x2E58C9,original,10);
            CHECK(rendered>=0 && rendered<=original);
            CHECK(halfw*rendered<=w/2.f-mx+.001f && halfh*rendered<=h/2.f-my+.001f);
        }
        /* Panel construction and its text-measurement width must shrink
         * together with the fully opened indicators, at every viewport. */
        draw(0x2E58C9,1.2f,10);panel(10);
        CHECK(fabsf(panel_width[0]/80.f-rendered/1.2f)<.0001f);
        CHECK(fabsf(panel_width[1]/90.f-rendered/1.2f)<.0001f);
        panel(1);CHECK(panel_width[0]==80.f && panel_width[1]==90.f);
        draw(0x14D46D,1.2f,10);CHECK(rendered==1.2f); /* Normal gameplay HUD. */
        draw(0x2E58C9,1.2f,1);CHECK(rendered==1.2f); /* Other mode. */
    }
    /* Reproduced reference viewport: original x=-48.64, now inset 25.6. */
    CHECK(fabsf(pc_objectives_scale(1.2f,0,0,640,480,320,240,1,0)*307.2f-294.4f)<.001f);
    /* The reported single-objective frame spans y=205..296. Its side
     * borders must move beyond the visible crescent (x~80 / x~560),
     * leaving room for the rounded corners and keeping the font unchanged. */
    {
        float ratio=pc_objectives_scale(1.2f,0,0,640,480,320,240,1,0)/1.2f;
        float panel_left=320.f-256.f*ratio;
        CHECK(panel_left>100.f && panel_left<125.f);
    }
    /* Small viewport, off-center animation, invalid viewport, and no enlargement. */
    CHECK(pc_objectives_scale(.2f,0,0,640,480,320,240,1,0)==.2f);
    CHECK(pc_objectives_scale(1.2f,0,0,320,240,80,80,1,0)<.3f);
    CHECK(pc_objectives_scale(1.2f,0,0,0,0,0,0,1,0)==1.2f);
    puts("PASS: automatic Objectives sizing across resolutions and viewports");return 0;
}
