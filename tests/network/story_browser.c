/* Selected remote Story metadata, original draw ABI, and non-Story fallbacks. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "pc_online.h"
#ifdef TSFP_NATIVE_DISPLAY
#undef _WIN32
#elif !defined(_WIN32)
#define _WIN32 1
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned original_calls;
#ifdef _WIN32
static uint32_t memory[0x800000/4],g_esp,g_eax;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
typedef struct GuestRegisters {uint32_t sp,ax;} GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_esp,g_eax};}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->ax;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static PcOnlineMode mode=PC_ONLINE_STORY;
PcOnlineMode pc_online_mode(void){return mode;}
void sub_00080F20(void){g_eax=1;} void sub_0003FA10(void){g_eax=2;} void sub_0003D7B0(void){g_eax=3;}
void sub_0003FCF0(void){g_eax=4;} void sub_0003F8D0(void){g_eax=5;} void sub_00041DC0(void){g_eax=6;}
static uint32_t expected_sprite=957,expected_name=321,expected_bitmap;
static unsigned texture_loads,names_drawn;
static uint32_t guest_call(void (*fn)(void),unsigned count,...){
    uint32_t args[5];va_list ap;va_start(ap,count);CHECK(count<=5);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_00080F20){CHECK(count==1 && args[0]==expected_sprite);++texture_loads;return 777;}
    if(fn==sub_0003D7B0){CHECK(count==1 && args[0]==expected_name);return 0x7000;}
    if(fn==sub_0003FA10){CHECK(count==1 && (args[0]==3 || args[0]==9));return 9;}
    if(fn==sub_0003FCF0){
        CHECK(count==4 && args[2]==0x7000);float width=2000.f,height=12.f;
        memcpy(&WORD_AT(args[0]),&width,4);memcpy(&WORD_AT(args[1]),&height,4);return 0;
    }
    if(fn==sub_00041DC0){
        CHECK(count==4 && args[2]==0x7000 && args[3]==0x7f7f7fffu);
        float x,y;memcpy(&x,&args[0],4);memcpy(&y,&args[1],4);
        CHECK(x==108.f && y>100.f && y<160.f);
        CHECK(WORD_AT(0x7e7270)!=0x3f800000);++names_drawn;return 0;
    }
    CHECK(fn==sub_0003F8D0 && !count);return 0;
}
#endif
#include "network/pc_story_browser.inc"
#ifdef _WIN32
static void picture(void){
    CHECK(WORD_AT(g_esp)==0x1cbfc4 && WORD_AT(g_esp+16)==expected_bitmap);
    ++original_calls;g_eax=0x1234;g_esp+=4;
}
static void match(void){
    const uint32_t frame=g_esp;g_esp=0x1c00;
    WORD_AT(g_esp)=0x1cbfc4;WORD_AT(g_esp+4)=0x4000;WORD_AT(g_esp+8)=0x4500;
    WORD_AT(g_esp+12)=0x6000;WORD_AT(g_esp+16)=0xdead;WORD_AT(g_esp+20)=0xffffffffu;
    pc_story_browser_thumbnail(picture);
    CHECK(g_esp==0x1c04 && g_eax==0x1234 && WORD_AT(0x1c10)==0xdead);
    g_esp=frame+4;
}
static void draw(unsigned index,unsigned rule,unsigned scene,unsigned custom){
    g_esp=0x2000;WORD_AT(g_esp)=0x76890;WORD_AT(g_esp+4)=0x4000;WORD_AT(g_esp+8)=0x4500;
    WORD_AT(g_esp+12)=0x5000;WORD_AT(0x4500+0x14)=index;
    WORD_AT(0x790960+3*0xcc+0x14)=scene;WORD_AT(0x790960+3*0xcc+0x18)=rule;
    WORD_AT(0x4000+0xf4)=0x3f800000;uint32_t before[4];memcpy(before,&WORD_AT(g_esp),sizeof before);
    const unsigned names=names_drawn;expected_bitmap=custom?777:0xdead;
    pc_story_browser_match(match);
    CHECK(g_esp==0x2004 && !memcmp(before,&WORD_AT(0x2000),sizeof before));
    CHECK(names_drawn==names+custom && !story_browser_draw.active);
    CHECK(WORD_AT(0x7e7270)==0x3f800000 && WORD_AT(0x7e7280)==0x3f800000);
}
#else
static void match(void){++original_calls;}
#endif
int main(void){
#ifdef _WIN32
    g_xbox_mem_offset=(uintptr_t)memory;WORD_AT(0x79094c)=101;
    for(unsigned i=0;i<13;i++)WORD_AT(0x4d1ac6+i*0x44)=0xffff;
    WORD_AT(0x4d1ac6+5*0x44)=44;WORD_AT(0x4d1ac2+5*0x44)=321;WORD_AT(0x4d1ad4+5*0x44)=957;
    const float bounds[4]={100,100,500,200},image[4]={380,100,500,180};
    memcpy(&WORD_AT(0x5000),bounds,16);memcpy(&WORD_AT(0x6000),image,16);
    WORD_AT(0x7e7270)=WORD_AT(0x7e7280)=0x3f800000;
    draw(3,2,44,1);CHECK(texture_loads==1);
    /* A different remote selection must not reuse this peer's previous level. */
    WORD_AT(0x4d1ac6)=2;WORD_AT(0x4d1ac2)=222;WORD_AT(0x4d1ad4)=958;
    expected_sprite=958;expected_name=222;draw(3,2,2,1);CHECK(texture_loads==2);
    draw(20,2,2,0);draw(3,0,2,0);draw(3,10,2,0);draw(3,2,999,0);
    mode=PC_ONLINE_ARCADE;draw(3,2,2,0);mode=PC_ONLINE_STORY;
    WORD_AT(0x79094c)=2;draw(3,2,2,0);WORD_AT(0x79094c)=101;
    WORD_AT(0x4d1ad4)=0xffffffff;draw(3,2,2,0);
    CHECK(texture_loads==2 && names_drawn==2 && original_calls==9);
#else
    pc_story_browser_match(match);pc_story_browser_thumbnail(match);CHECK(original_calls==2);
#endif
    puts("PASS: advertised Story browser name/thumbnail, scoped ABI restoration and original fallbacks");return 0;
}
