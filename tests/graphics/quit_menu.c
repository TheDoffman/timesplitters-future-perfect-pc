/* Exercise the production guest confirmation adapter without a game image. */
#define TSFP_QUIT_MENU_TEST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static uint32_t memory[0x10000/4],g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
typedef struct GuestRegisters { uint32_t sp; } GuestRegisters;
static void save_registers(GuestRegisters *r) {r->sp=g_esp;}
static void restore_registers(const GuestRegisters *r) {g_esp=r->sp;}
static int guest_writable(uint32_t p,size_t n) {return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static uint32_t xbox_HeapAlloc(uint32_t bytes,uint32_t align) {CHECK(bytes==112 && align==4);return 0x8000;}
#define WM_CLOSE 16
static int close_calls,post_ok=1;
#ifdef TSFP_NATIVE_DISPLAY
int tsfp_native_request_quit(void) {++close_calls;return post_ok;}
#else
static uintptr_t xbox_D3D8Window(void) {return 123;}
static int PostMessage(uintptr_t window,unsigned msg,int w,int l)
{CHECK(window==123 && msg==WM_CLOSE && !w && !l);++close_calls;return post_ok;}
#endif
static volatile int stub;
void sub_00079B70(void) {stub=0;}
void sub_00075B70(void) {stub=1;}
void sub_00071B10(void) {stub=2;}
void sub_00071C50(void) {stub=3;}
void sub_00079D60_gen(void) {stub=4;}
void sub_000771D0(void) {stub=5;}
void sub_000770E0(void) {stub=6;}
static unsigned row_count,finalized;
static uint32_t row_ids[4],row_callbacks[4],next_parent,next_menu,default_row;
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t args[6];va_list ap;va_start(ap,count);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(fn==sub_00079D60_gen) {
        CHECK(count==6 && row_count<4);
        row_ids[row_count]=args[1];row_callbacks[row_count++]=args[4];
    } else if(fn==sub_00079B70) {next_parent=args[0];next_menu=args[1];}
    else if(fn==sub_000770E0) default_row=args[1];
    else if(fn==sub_000771D0) ++finalized;
    else if(fn!=sub_00075B70 && fn!=sub_00071B10 && fn!=sub_00071C50) {
        uint32_t sp=g_esp;g_esp=0x1800;WORD_AT(g_esp+4)=args[0];fn();CHECK(g_esp==0x1804);g_esp=sp;
    }
    return 0;
}
static void unlock_all_append(uint32_t menu) {(void)menu;CHECK(0);}
static unsigned extras_added;
static void extras_append(void) {++extras_added;}
#include "pc_quit_menu.inc"
static void original(void) {++finalized;g_esp+=4;}
static void setup(uint32_t caller)
{g_esp=0x1000;WORD_AT(g_esp)=caller;WORD_AT(g_esp+4)=0x2000;row_count=finalized=0;}
static void callback(void (*fn)(void)) {g_esp=0x1000;fn();CHECK(g_esp==0x1004);}
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x202c)=0x3000;WORD_AT(0x2004)=0x4000;
    const uint32_t sites[]={0x2d0edf,0x2f582f,0x2f5879};
    for(unsigned i=0;i<3;i++) {
        setup(sites[i]);pc_display_menu_finish(original);
        CHECK(finalized==1 && g_esp==0x1004);
        if(i==0)CHECK(row_count==1 && row_ids[0]==8 && row_callbacks[0]==QUIT_OPEN_CB);
        else CHECK(row_count==0);
    }
    setup(0x12008);pc_display_menu_finish(original);CHECK(!row_count && finalized==1);
    setup(0x2d095b);pc_display_menu_finish(original);CHECK(extras_added==1 && finalized==1);
    setup(sites[0]);WORD_AT(0x2038)=1;pc_display_menu_finish(original);CHECK(!row_count);
    WORD_AT(0x2038)=0;
    setup(sites[0]);callback(quit_open);
    CHECK(next_parent==0x3000 && next_menu==quit_storage && !close_calls);
    row_count=0;default_row=999;callback(quit_build);
    CHECK(row_count==2 && row_ids[0]==0 && row_ids[1]==1 && default_row==0 && !close_calls);
    WORD_AT(0x218c)=0;callback(quit_select);CHECK(next_menu==0x4000 && !close_calls);
    callback(quit_open);callback(quit_back);CHECK(next_menu==0x4000 && !close_calls);
    callback(quit_open);WORD_AT(0x218c)=42;callback(quit_select);CHECK(!close_calls);
    WORD_AT(0x218c)=1;post_ok=0;callback(quit_select);CHECK(close_calls==1 && !quit_requested);
    post_ok=1;callback(quit_select);callback(quit_select);CHECK(close_calls==2 && quit_requested);
    puts("PASS: menu insertion, Cancel default, Back, confirmation-only exit and retry");
    return 0;
}
