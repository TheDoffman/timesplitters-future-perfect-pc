/* Production hub callbacks, native child returns and removal of desktop exit. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define PAUSE_SETTINGS_OPEN 0x12018u
#define PAUSE_SETTINGS_BUILD 0x12019u
#define PAUSE_SETTINGS_BACK 0x1201bu
#define PC_DISPLAY_MENU_CALLBACK 0x12008u
static uint32_t memory[0x800000/4],g_esp;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)(g_xbox_mem_offset+(uint32_t)(a)))
typedef struct {uint32_t sp;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000&&p<sizeof memory&&n<=sizeof memory-p;}
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t a){CHECK(n==80&&a==4);return 0x8000;}
static uint32_t audio_options_caption(void){return 0x8100;}
static uint32_t controls_caption(void){return 0x8200;}
static volatile int stub;
void sub_00079B70(void){stub=1;}
void sub_00075B70(void){stub=2;}
void sub_00071B10(void){stub=3;}
void sub_00071C50(void){stub=4;}
void sub_00079D60_gen(void){stub=5;}
void sub_000771D0(void){stub=6;}
static unsigned rows,layouts,notices;
#ifdef _WIN32
static unsigned story_rows,story_layouts;
#endif
static uint32_t callbacks[4],captions[4],next_parent,next_menu;
static void main_update_notice_open(uint32_t m){CHECK(m==0x2000);notices++;}
#ifdef _WIN32
static void pc_story_network_pause_rows(void){story_rows++;}
static void pc_story_network_pause_layout(uint32_t m){CHECK(m==0x2000);story_layouts++;}
#endif
static uint32_t guest_call(void (*fn)(void),unsigned n,...)
{
    uint32_t a[6];va_list ap;va_start(ap,n);for(unsigned i=0;i<n;i++)a[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_00079B70){CHECK(n==2);next_parent=a[0];next_menu=a[1];}
    else if(fn==sub_00075B70){CHECK(n==5&&a[0]==0xa000&&a[4]==PAUSE_SETTINGS_BACK);rows=0;}
    else if(fn==sub_00079D60_gen){CHECK(n==6&&rows<4);captions[rows]=a[0];callbacks[rows++]=a[4];}
    else if(fn==sub_000771D0){CHECK(a[0]==0x2000);layouts++;}
    return 0;
}
#include "pc_pause_settings.inc"
static void original(void){layouts++;g_esp+=4;}
static void call(void (*fn)(void)){g_esp=0x1000;WORD_AT(g_esp+4)=0x2000;fn();CHECK(g_esp==0x1004);}
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;WORD_AT(0x79094c)=2;
    WORD_AT(0x202c)=0x3000;WORD_AT(0x2004)=0x524700;
    for(unsigned port=0;port<4;port++) {
        WORD_AT(0x2038)=port;WORD_AT(0x761c48+port*4)=0x9000;WORD_AT(0x9000)=0x21;
        call(pause_settings_open);CHECK(next_parent==0x3000&&next_menu==pause_settings_storage);
        call(pause_settings_build);CHECK(rows==3);
        CHECK(callbacks[0]==0x2ec1f0&&callbacks[1]==PC_DISPLAY_MENU_CALLBACK&&callbacks[2]==0x2ec1a0);
        CHECK(!strcmp((char *)(g_xbox_mem_offset+captions[1]),"Video Options"));
        CHECK(pause_settings_return(0x3000,0x524700)==pause_settings_storage);
        CHECK(pause_settings_return(0x3000,0x52377c)==0x52377c); /* Keep child confirmations. */
        CHECK(pause_settings_return(0x4000,0x524700)==0x524700); /* Other player's context. */
        call(pause_settings_back);CHECK(next_parent==0x3000&&next_menu==0x524700&&!pause_settings_active);
        call(pause_settings_open);WORD_AT(0x9000)=0x81; /* Fresh Start opens root. */
        CHECK(pause_settings_return(0x3000,0x524700)==0x524700&&!pause_settings_active);
    }
    call(pause_settings_open);WORD_AT(0x79094c)=101;
    CHECK(pause_settings_return(0x3000,0x524700)==0x524700&&!pause_settings_active);
    const uint32_t callers[]={0x2d0edf,0x2f582f,0x2f5879,0x12008};
    for(unsigned i=0;i<4;i++) {
        g_esp=0x1000;WORD_AT(g_esp)=callers[i];WORD_AT(g_esp+4)=0x2000;
        rows=layouts=0;pc_display_menu_finish(original);CHECK(!rows&&layouts==1&&g_esp==0x1004);
    }
    CHECK(notices==1);
#ifdef _WIN32
    CHECK(story_rows==2&&story_layouts==2);
#endif
    puts("PASS: pause Settings hub, child returns, fresh pause, native hooks and no desktop exit rows");
    return 0;
}
