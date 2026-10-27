/* Execute the original US living-actor reveal with isolated memory/render stubs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "pc_ui_layout.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
#define WORD_AT(a) MEM32(a)
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: line %d %s\n",__LINE__,#x);exit(1);}}while(0)
static int ui_placement,wide,wrapped,pending,draws,blurs;
static uint32_t ui_tag,rectangle[9];
static float scale;
static int projected_x;
static int guest_writable(uint32_t a,size_t n){return a==0x3000 && n<=0x24;}
static int pc_display_wide_now(void){return wide;}
static float pc_display_hud_scale(void){return scale;}
static void sub_001CDAE0(void){}
static void sub_00022FF0(void){pending=0;}
static uint32_t guest_call(void(*f)(void),unsigned n){(void)n;if(f==sub_001CDAE0)return 1;f();return 0;}
static void host_SetTwoDPlacement(int p,uint32_t tag){(void)p;(void)tag;CHECK(!pending);}
/* PRODUCTION */
void sub_000A4D70(void){
    MEM32(MEM32(g_esp+8))=projected_x;MEM32(MEM32(g_esp+12))=100;
    MEM32(MEM32(g_esp+16))=projected_x+60;MEM32(MEM32(g_esp+20))=350;
    MEMF(MEM32(g_esp+24))=.5f;g_eax=1;g_esp+=4;
}
void sub_00019640(void){g_eax=2;g_esp+=4;}
void sub_00019650(void){g_esp+=4;}
void sub_0001C6F0(void){g_esp+=4;}
void sub_000389B0(void){g_esp+=4;}
void sub_000942B0(void){g_esp+=4;}
void sub_00094220(void){g_esp+=4;}
void sub_00019D70(void){g_esp+=4;}
void sub_0008F950(void){g_esp+=4;}
void sub_0001F790(void){g_esp+=4;}
void sub_0001EDE0(void){g_esp+=4;}
void sub_0001A010(void){g_esp+=4;}
void sub_0001A0D0(void){g_esp+=4;}
void sub_0001ED60(void){g_esp+=4;}
void sub_0002A2D0(void){g_esp+=4;}
void sub_0002A310(void){g_esp+=4;}
void sub_0002A320(void){g_esp+=4;}
void sub_00019D40(void){g_esp+=4;}
void sub_0001B7B0(void){
    CHECK(MEM32(g_esp)==0x1231fb && ui_placement==0);blurs++;g_esp+=4;
}
void sub_0001F090_gen(void){
    CHECK(MEM32(g_esp)==0x1232a9);CHECK(ui_placement==(wrapped && wide?1:0));
    CHECK(blurs==5);for(unsigned i=0;i<9;i++)rectangle[i]=MEM32(g_esp+4+i*4);
    CHECK(rectangle[4]==0 && rectangle[5]==0 && rectangle[6]==0xff0 && rectangle[7]==0xff0);
    CHECK(rectangle[8]==0x8080807f);draws++;pending=1;g_esp+=4;
}
void sub_0001F090(void){
    if(wrapped)pc_display_weapon_effect(sub_0001F090_gen,0);else sub_0001F090_gen();
}
/* ORIGINAL */
static void run(int fix){
    wrapped=fix;g_esp=0x10000;MEM32(g_esp)=0x126453;g_eax=0x4000;
    g_ebx=11;g_ebp=22;g_esi=33;g_edi=44;
    draws=blurs=0;pending=0;ui_placement=0;ui_tag=123;
    sub_00122F10_gen();
    CHECK(g_esp==0x10004 && draws==1 && blurs==5);
    CHECK(g_ebx==11 && g_ebp==22 && g_esi==33 && g_edi==44);
    CHECK(ui_placement==0 && ui_tag==123);
    if(fix && wide)CHECK(!pending);
}
int main(void){
    unsigned char *memory=calloc(1,0x900000);CHECK(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* CONSTANTS */
    MEM32(0x7e71c8)=0x3000;MEM32(0x3004)=639;MEM32(0x3008)=320;MEM32(0x300c)=640;
    MEM32(0x3018)=479;MEM32(0x301c)=240;MEM32(0x3020)=480;
    MEM32(0x4000)=0x5000;MEM32(0x5000)=0x6000;MEM32(0x6004)=0x6100;
    MEM32(0x7b0c7c)=0x7000;MEM32(0x7b0cc0)=0x8000;
    MEM32(0x4e0d6c)=12;
    const unsigned sizes[][2]={{640,480},{1920,1080},{3440,1440},{5120,1440},{1280,1024}};
    for(projected_x=40;projected_x<=520;projected_x+=160){
        uint32_t baseline[9];run(0);memcpy(baseline,rectangle,sizeof baseline);
        for(unsigned m=0;m<5;m++){
            wide=sizes[m][0]*3!=sizes[m][1]*4;scale=4.f*sizes[m][1]/(3.f*sizes[m][0]);
            run(1);CHECK(!memcmp(baseline,rectangle,sizeof baseline));
        }
    }
    free(memory);puts("PASS: original US living-actor reveal keeps geometry, five blur passes, UVs, tint and ABI; final composite follows world placement");return 0;
}
