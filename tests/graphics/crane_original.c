/* Lift the checksum-verified original vignette; observe emitted geometry. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
#define WORD_AT(a) MEM32(a)
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: line %d %s\n",__LINE__,#x);exit(1);}}while(0)
static int ui_placement,wide,players=1,pending,flushes,draws;
static uint32_t ui_tag;
static float scale;
static uint32_t rectangles[8][9];
static int placements[8];
static int guest_writable(uint32_t a,size_t n){return a==0x3000 && n<=0x24;}
static int pc_display_wide_now(void){return wide;}
static float pc_display_hud_scale(void){return scale;}
static void sub_001CDAE0(void){}
static void sub_00022FF0(void){pending=0;flushes++;}
static uint32_t guest_call(void(*f)(void),unsigned n){(void)n;if(f==sub_001CDAE0)return players;f();return 0;}
static void host_SetTwoDPlacement(int p,uint32_t tag){(void)p;(void)tag;CHECK(!pending);}
/* PRODUCTION */
void sub_00080F20(void){CHECK(MEM32(g_esp+4)==0x295);g_eax=0;g_esp+=4;}
void sub_00019D70(void){CHECK(MEM32(g_esp+4)==1);g_esp+=4;}
void sub_00019650(void){CHECK(MEM32(g_esp+4)==0);g_esp+=4;}
void sub_0001ED50(void){CHECK(MEM32(g_esp+4)==0);g_esp+=4;}
void sub_0001F090(void){
    CHECK(draws<8);for(unsigned i=0;i<9;i++)rectangles[draws][i]=MEM32(g_esp+4+i*4);
    placements[draws++]=ui_placement;pending=1;g_esp+=4;
}
void sub_0001EFB0(void){
    CHECK(draws<8);for(unsigned i=0;i<5;i++)rectangles[draws][i]=MEM32(g_esp+4+i*4);
    placements[draws++]=ui_placement;pending=1;g_esp+=4;
}
/* ORIGINAL */
static void run(int wrapped,float radius,unsigned alpha){
    g_esp=0x10000;MEM32(g_esp)=0x1371d7;MEMF(g_esp+4)=radius;MEM32(g_esp+8)=alpha;
    g_ebx=11;g_ebp=22;g_esi=33;g_edi=44;
    draws=flushes=0;pending=1;ui_placement=2;ui_tag=123;
    memset(rectangles,0,sizeof rectangles);
    if(wrapped)sub_00137010();else sub_00137010_gen();
    CHECK(g_esp==0x10004);
    CHECK(g_ebx==11 && g_ebp==22 && g_esi==33 && g_edi==44);
    CHECK(ui_placement==2 && ui_tag==123);
    CHECK(draws>=4 && draws<=6);
    if(wrapped && wide)CHECK(flushes==2 && !pending);
}
int main(void){
    unsigned char *memory=calloc(1,0x900000);CHECK(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* CONSTANTS */
    MEM32(0x7e71c8)=0x3000;MEM32(0x3004)=639;MEM32(0x3008)=320;MEM32(0x300c)=640;
    MEM32(0x3018)=479;MEM32(0x301c)=240;MEM32(0x3020)=480;
    MEMF(0x4e68f4)=MEMF(0x4e68f8)=1.f;
    MEM32(0x4b85c8)=0x4000;MEM32(0x4000)=0x5000;
    MEM16(0x5026)=MEM16(0x502a)=128;
    const unsigned sizes[][2]={{640,480},{1920,1080},{3440,1440},{5120,1440},{1080,1920}};
    const float radii[]={.69f,.59f};
    for(unsigned r=0;r<2;r++)for(unsigned a=0;a<=127;a+=127){
        uint32_t baseline[8][9];run(0,radii[r],a);uint32_t args[]={MEM32(0x10004),MEM32(0x10008)};unsigned count=draws;memcpy(baseline,rectangles,sizeof baseline);
        for(unsigned m=0;m<5;m++){
            wide=sizes[m][0]*3!=sizes[m][1]*4;scale=4.f*sizes[m][1]/(3.f*sizes[m][0]);
            run(1,radii[r],a);CHECK(draws==count && !memcmp(baseline,rectangles,sizeof baseline));
            CHECK(MEM32(0x10004)==args[0] && MEM32(0x10008)==args[1]);
            for(unsigned i=0;i<draws;i++)CHECK(placements[i]==(wide?1:2));
            /* The quartet spans the full canvas, with shared centre seams. */
            CHECK((int32_t)rectangles[0][1]<=0);
            CHECK((int32_t)rectangles[0][0]<=0 || (draws==6 && rectangles[4][0]==0));
            CHECK(rectangles[0][2]==320 && rectangles[0][3]==240);
            CHECK(rectangles[3][0]==320 && rectangles[3][1]==240);
            CHECK(rectangles[3][3]>=480);
            CHECK(rectangles[3][2]>=640 || (draws==6 && rectangles[5][2]==639));
        }
    }
    free(memory);puts("PASS: original US crane vignette geometry, UVs, alpha and guest ABI preserved; all quadrants use full-screen placement");return 0;
}
