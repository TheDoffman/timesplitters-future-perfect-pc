/* Execute production chooser callbacks with isolated guest RAM and menu spies. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pc_online.h"
#ifdef TSFP_NATIVE_DISPLAY
#undef _WIN32
#elif !defined(_WIN32)
#define _WIN32 1
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t memory[0x10000/4],g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
typedef struct GuestRegisters {uint32_t sp;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static int allocation_ok=1,connection_ok,connects,disconnects,launches,rebuilt;
#ifdef _WIN32
static unsigned profile_captures;
static void story_rewards_profiles_capture(void){CHECK(!launches);++profile_captures;}
#endif

static unsigned rows,finalized;
static uint32_t next_parent,next_menu,default_row,row_ids[2],row_callbacks[2];
static uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t align)
{CHECK(bytes==144 && align==4);return allocation_ok?0x8000:0;}
int pc_online_connect_mode(PcOnlineMode mode){CHECK(mode==PC_ONLINE_STORY);++connects;return connection_ok;}
void pc_online_disconnect(void){++disconnects;}
static volatile unsigned route;
void sub_00079B70(void){route=1;}
void sub_00075B70(void){route=2;}
void sub_00071B10(void){route=3;}
void sub_00071C50(void){route=4;}
void sub_00079D60_gen(void){route=5;}
void sub_000771D0(void){route=6;}
void sub_000770E0(void){route=7;}
void sub_00078980(void){route=8;}
void sub_001A0A60(void){route=9;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t args[6];va_list ap;va_start(ap,count);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(fn==sub_00079D60_gen){
        CHECK(count==6 && rows<2);
        CHECK(!strcmp((char *)((uintptr_t)args[0]+g_xbox_mem_offset),rows?"Online":"Local"));
        row_ids[rows]=args[1];row_callbacks[rows++]=args[4];
    }else if(fn==sub_00079B70){next_parent=args[0];next_menu=args[1];}
    else if(fn==sub_00075B70){float width;memcpy(&width,&args[2],4);CHECK(width>0 && width<=100);}
    else if(fn==sub_000770E0){default_row=args[1];}
    else if(fn==sub_000771D0){++finalized;}
    else if(fn==sub_001A0A60){++launches;}
    else if(fn==sub_00078980){++rebuilt;}
    else CHECK(fn==sub_00071B10 || fn==sub_00071C50);
    return 0;
}
#include "pc_story_menu.inc"
static void callback(void (*fn)(void))
{g_esp=0x1000;WORD_AT(g_esp+4)=0x2000;fn();CHECK(g_esp==0x1004);}
static uint32_t received_callback;
static void row_original(void){received_callback=WORD_AT(g_esp+20);g_esp+=4;}
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x202c)=0x3000;WORD_AT(0x2004)=0x4000;
    allocation_ok=0;callback(story_open);CHECK(!next_menu);
    allocation_ok=1;callback(story_open);
    CHECK(next_parent==0x3000 && next_menu==story_storage && WORD_AT(story_storage)==STORY_BUILD_CB);
    callback(story_build);CHECK(rows==2 && finalized==1 && default_row==0);
    CHECK(row_ids[0]==0 && row_ids[1]==1 && row_callbacks[0]==STORY_SELECT_CB && row_callbacks[1]==STORY_SELECT_CB);
    callback(story_back);CHECK(next_parent==0x3000 && next_menu==0x4000);
    WORD_AT(0x218c)=0;callback(story_select);CHECK(next_parent==0x3000 && next_menu==0x5238e0);
    next_menu=0;WORD_AT(0x218c)=1;callback(story_select);CHECK(!next_menu && !launches);
#ifdef _WIN32
    CHECK(connects==1 && disconnects==1);
    connection_ok=1;callback(story_select);CHECK(connects==2 && launches==1 && profile_captures==1);
#else
    CHECK(!connects && !disconnects && rebuilt==1 && story_online_unavailable);
#endif
    WORD_AT(0x218c)=2;callback(story_select);CHECK(next_menu==0);
    g_esp=0x1000;WORD_AT(g_esp)=0x2d0c8e;WORD_AT(g_esp+8)=0;WORD_AT(g_esp+20)=0;
    CHECK(pc_story_menu_button(row_original) && received_callback==STORY_OPEN_CB && WORD_AT(0x1014)==0 && g_esp==0x1004);
    g_esp=0x1000;WORD_AT(g_esp+8)=1;CHECK(!pc_story_menu_button(row_original) && g_esp==0x1000);
    WORD_AT(g_esp+8)=0;WORD_AT(g_esp)=0x2d0c8f;CHECK(!pc_story_menu_button(row_original));
    WORD_AT(g_esp)=0x2d0c8e;WORD_AT(g_esp+20)=123;CHECK(!pc_story_menu_button(row_original));
    puts("PASS: Story chooser ABI, Local route, typed Online, cancellation and native fallback");
    return 0;
}
