/* Production host controls against isolated guest RAM and original spies. */
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
#ifdef _WIN32
static uint32_t memory[0x800000/4],g_esp,g_eax;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
typedef struct GuestRegisters {uint32_t sp;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static PcOnlineMode online_mode=PC_ONLINE_STORY;
PcOnlineMode pc_online_mode(void){return online_mode;}
static int allocation_ok=1;
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t align)
{CHECK((n==64 || n==144) && align==4);return allocation_ok?(n==64?0x8000:0x8100):0;}
static uint32_t next_parent,next_menu,ticks,selectors;
static uint32_t header_mission,header_fallback;
void sub_00197890(void){++header_fallback;g_esp+=4;}
void sub_002CA9F0(void){CHECK(WORD_AT(WORD_AT(g_esp+4)+4)==0x5238c0);header_mission=WORD_AT(WORD_AT(g_esp+8)+0x14);g_esp+=4;}
static volatile unsigned marker;
static unsigned difficulty_rows,finalized,highlighted;
static unsigned story_factories;
static unsigned context_spawns,ready_calls;
static unsigned pad_fallbacks;
void sub_000CF060_gen(void){marker=16;}
void sub_00359EB0(void){CHECK(0);}
void sub_0003D7B0(void){marker=9;} void sub_00040F00(void){marker=10;}
void sub_00041DC0(void){marker=11;} void sub_0003FA10(void){marker=12;}
void sub_0003F8D0(void){marker=14;} void sub_0003FCF0(void){marker=15;}
void sub_00154FE0(void){marker=13;} void sub_00198E20(void){g_esp+=4;}
static unsigned briefing_draws;
void sub_002D2C70(void){marker=7;}
void sub_000D50E0(void){marker=8;}
void sub_00075B70(void){marker=1;} void sub_00071B10(void){marker=2;}
void sub_00071C50(void){marker=3;} void sub_00079D60_gen(void){marker=4;}
void sub_000771D0(void){marker=5;} void sub_000770E0(void){marker=6;}
void sub_00079B70(void){CHECK(0);}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t args[6];va_list ap;va_start(ap,count);CHECK(count<=6);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(fn==sub_000CF060_gen){CHECK(count==1 && WORD_AT(args[0])==0);++pad_fallbacks;return 77;}
    else if(fn==sub_0003D7B0){CHECK(count==1 && args[0]==22);return 0xb000;}
    else if(fn==sub_00040F00){CHECK(count==2 && args[0]==0xb000);return 0xc000;}
    else if(fn==sub_0003F8D0){CHECK(count==0);}
    else if(fn==sub_0003FCF0){CHECK(count==4 && args[2]==0xc000);float height=10.f;memcpy(&WORD_AT(args[1]),&height,4);}
    else if(fn==sub_0003FA10){CHECK(count==1);return 3;}
    else if(fn==sub_00154FE0){CHECK(count==2);return args[0];}
    else if(fn==sub_00041DC0){CHECK(count==4 && args[2]==0xc000);++briefing_draws;}
    else if(fn==sub_00079B70){CHECK(count==2);next_parent=args[0];next_menu=args[1];}
    else if(fn==sub_002D2C70){
        CHECK(count==0 && WORD_AT(0x78ad2c)==4 && WORD_AT(0x78ad48)==1 &&
            (WORD_AT(0x78ad54)&255)==2);
        CHECK(((uint8_t *)memory)[0x78adad]==10 && ((uint8_t *)memory)[0x78adae]==2);
        /* Factory initializes the whole config; adapter must retain this
           unmodified field while restoring the admitted/network counts. */
        memset((uint8_t *)memory+0x78ad58,0,0xa0);
        ((uint8_t *)memory)[0x78ad58]=1;((uint8_t *)memory)[0x78ad59]=1;
        ((uint8_t *)memory)[0x78adad]=10;((uint8_t *)memory)[0x78adae]=2;
        ((uint8_t *)memory)[0x78ad58+0x60]=0x5a;
        ++story_factories;
    }
    else if(fn==sub_00359EB0){CHECK(count==2 && args[0]==0 && args[1]==1);++ready_calls;((uint8_t *)memory)[WORD_AT(0x774028)+0x2fa]=1;}
    else if(fn==sub_000D50E0){
        CHECK(count==2 && args[1]==1 && (args[0]==0x6000 || args[0]==0x7584));
        CHECK(WORD_AT(args[0]+0x114)==16);WORD_AT(args[0]+0x114)=2;++context_spawns;
    }
    else if(fn==sub_00075B70){CHECK(count==5 && args[4]==0x1201f);difficulty_rows=0;}
    else if(fn==sub_00079D60_gen){
        static const char *names[]={"Easy","Normal","Hard"};
        CHECK(count==6 && difficulty_rows<3 && args[1]==difficulty_rows && args[4]==0x1201e);
        CHECK(!strcmp((char *)(g_xbox_mem_offset+args[0]),names[difficulty_rows]));++difficulty_rows;
    }else if(fn==sub_000771D0){CHECK(count==1);++finalized;}
    else if(fn==sub_000770E0){CHECK(count==2);highlighted=args[1];}
    else CHECK(fn==sub_00071B10 || fn==sub_00071C50);
    return 0;
}
static int disconnect_in_tick;
void sub_001A8D30(void)
{
    CHECK(!(WORD_AT(0x6b94d8)&0x50));
    ++ticks;WORD_AT(0x6b94d8)|=0x800;
    if(disconnect_in_tick) WORD_AT(0x76b1a4)=0;
    g_esp+=4;
}
void sub_001A8F30(void)
{CHECK(!(WORD_AT(0x6b94d8)&0x50));CHECK(!WORD_AT(0x21bc));WORD_AT(0x212c)=0x9999;++ticks;g_esp+=4;}
void sub_002CB090(void){CHECK(WORD_AT(0x78ad2c)==4);selectors|=1;g_esp+=4;}
void sub_002D62F0(void){CHECK(0);}
void sub_002CAFE0(void)
{
    CHECK(0); /* Story settings must not enter the original local load path. */
}
#endif
#define STORY_SELECT_CB 0x1201eu
#define STORY_BUILD_CB 0x1201du
#define STORY_BACK_CB 0x1201fu
#ifdef _WIN32
static int load_probe_enabled;
static const char *load_getenv(const char *name)
{return load_probe_enabled && (!strcmp(name,"TSFP_STORY_LOAD_PROBE") ||
    !strcmp(name,"TSFP_STORY_TRACE"))?"1":NULL;}
#endif
#include "pc_story_lobby.inc"
#ifdef _WIN32
#define getenv load_getenv
#endif
#include "pc_story_load.inc"
#ifdef _WIN32
#undef getenv
#endif
#ifdef _WIN32
static unsigned failure_original_calls;
static uint32_t pad_original_result=UINT32_MAX;
static void pad_original(void){g_eax=pad_original_result;g_esp+=4;}
static unsigned script_original_calls;
static void script_original(void)
{++script_original_calls;WORD_AT(0x7de45c)|=0x2000000;WORD_AT(0x6114)=256;g_eax=19;g_esp+=4;}
static void reset_original(void)
{++script_original_calls;WORD_AT(0x6114)=2;g_eax=33;g_esp+=4;}
static void frame_original(void){++failure_original_calls;g_esp+=4;}
static void failure_original(void)
{++failure_original_calls;WORD_AT(0x7de30c)=8;WORD_AT(0x7de45c)=0x10000;g_esp+=4;}
#endif
#ifdef _WIN32
static void row_original(void)
{CHECK(WORD_AT(g_esp+20)==STORY_SELECT_CB);g_esp+=4;}
static void header_original(void)
{CHECK(WORD_AT(g_esp+24)==STORY_BUILD_CB);g_esp+=4;}
static void rewrite_row(uint32_t caller,uint32_t target)
{
    g_esp=0x1000;WORD_AT(g_esp)=caller;WORD_AT(g_esp+20)=target;
    CHECK(pc_story_lobby_button(row_original));
    CHECK(g_esp==0x1004 && WORD_AT(0x1014)==target);
}
static void call(uint32_t address)
{
    g_esp=0x1000;WORD_AT(0x1004)=0x2000;
    void (*fn)(void)=story_lobby_lookup(address);CHECK(fn);
    fn();CHECK(g_esp==0x1004);
}
#endif
int main(void)
{
#ifdef _WIN32
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=11;
    WORD_AT(0x2004)=0x4e99c4;WORD_AT(0x202c)=0x3000;
    WORD_AT(0x2148)=1;WORD_AT(0x2158)=2;
    WORD_AT(0x78ad2c)=13;WORD_AT(0x78ae08)=7;
    WORD_AT(0x78ad48)=0xaabbccdd;
    g_esp=0x1000;WORD_AT(g_esp)=0x1a8dfc;WORD_AT(g_esp+8)=0x145;
    WORD_AT(g_esp+24)=0x197890;
    CHECK(pc_story_lobby_button(header_original) && g_esp==0x1004 && WORD_AT(0x1018)==0x197890);
    WORD_AT(0x1008)=0x4000;WORD_AT(0x4014)=0x145;story_lobby_mission=4;
    call(STORY_BUILD_CB);CHECK(header_mission==4 && WORD_AT(0x4014)==0x145);
    WORD_AT(0x4014)=9;call(STORY_BUILD_CB);CHECK(header_fallback==1);
    story_lobby_mission=0;
    online_mode=PC_ONLINE_ARCADE;CHECK(!story_lobby_lookup(0x1a8d30));
    online_mode=PC_ONLINE_STORY;
    WORD_AT(0x76b140)=2;CHECK(!story_lobby_lookup(0x1a8d30));WORD_AT(0x76b140)=1;
    CHECK(!story_lobby_lookup(0x2cb090));
    allocation_ok=0;call(0x1a8d30);CHECK(!story_lobby_captions);
    allocation_ok=1;WORD_AT(0x6b94d8)=0x41;call(0x1a8d30);
    CHECK(next_parent==0x3000 && next_menu==0x5238e0 && story_lobby_options==1);
    CHECK(WORD_AT(0x6b94d8)==0x841 && ticks==2);
    CHECK(WORD_AT(0x2148)==1 && WORD_AT(0x2158)==2);
    CHECK(!strcmp((char *)(g_xbox_mem_offset+WORD_AT(0x214c)),"Back"));
    CHECK(!strcmp((char *)(g_xbox_mem_offset+WORD_AT(0x215c)),"Player Progress"));
    call(0x2cb090);CHECK(WORD_AT(0x78ad2c)==13 && selectors==1);
    rewrite_row(0x2cb149,0x2cafe0);
    g_esp=0x1000;WORD_AT(g_esp)=0x2cb14a;
    CHECK(!pc_story_lobby_button(row_original) && g_esp==0x1000);
    WORD_AT(0x2004)=0x5238e0;
    WORD_AT(0x218c)=13;call(0x2cafe0);CHECK(story_lobby_options==1);
    WORD_AT(0x218c)=12;call(STORY_SELECT_CB);
    CHECK(story_lobby_mission==12 && WORD_AT(0x78ad48)==0xaabbccdd && WORD_AT(0x78ad2c)==13 && WORD_AT(0x78ae08)==7);
    CHECK(story_lobby_options==2 && next_menu==story_difficulty_descriptor && next_parent==0x3000);
    CHECK(WORD_AT(story_difficulty_descriptor)==STORY_BUILD_CB);
    WORD_AT(0x2004)=story_difficulty_descriptor;
    rewrite_row(0x2d6383,0x2cad80);rewrite_row(0x2d63c9,0x2cad80);
    rewrite_row(0x2d640f,0x2cad80);
    call(STORY_BUILD_CB);CHECK(WORD_AT(0x78ad2c)==13 && selectors==1);
    CHECK(difficulty_rows==3 && finalized==1 && highlighted==0);
    call(STORY_BACK_CB);CHECK(story_lobby_options==1 && next_menu==0x5238e0);
    CHECK(story_lobby_mission==12 && WORD_AT(0x78ad48)==0xaabbccdd);
    WORD_AT(0x2004)=0x5238e0;call(STORY_SELECT_CB);
    CHECK(story_lobby_options==2 && next_menu==story_difficulty_descriptor);
    WORD_AT(0x2004)=story_difficulty_descriptor;
    WORD_AT(0x78ad54)=0x12345600;
    WORD_AT(0x218c)=3;call(0x2cad80);CHECK(story_lobby_options==2);
    WORD_AT(0x218c)=2;call(STORY_SELECT_CB);
    CHECK(story_lobby_difficulty==2 && WORD_AT(0x78ad54)==0x12345600 && !story_lobby_options);
    CHECK(next_menu==0x4e99c4 && !story_lobby_lookup(0x2cad80));
    WORD_AT(0x2004)=0x4e99c4;
    WORD_AT(0x6b94d8)=0x10;call(0x1a8d30);CHECK(next_menu==0x523940);
    next_menu=0;disconnect_in_tick=1;WORD_AT(0x6b94d8)=0x40;
    call(0x1a8d30);CHECK(!next_menu && WORD_AT(0x6b94d8)==0x840);
    CHECK(!story_lobby_lookup(0x1a8d30));
    /* Disabled/Arcade/malformed configs must remain untouched. Host config
       crosses the same interface as a received guest start config. */
    {
        uint8_t *config=(uint8_t *)memory+0x78ad58;
        uint8_t before[0xa0];
        WORD_AT(0x76b140)=1;WORD_AT(0x76b108)=1;WORD_AT(0x76b1a4)=11;
        story_lobby_mission=1;story_lobby_difficulty=2;
        WORD_AT(0x4d1ac6+0x44)=3;
        memset(config,0,0xa0);config[0]=2;config[1]=1;
        memcpy(before,config,sizeof before);
        online_mode=PC_ONLINE_NONE;
        CHECK(!pc_story_network_prepare(0x78ad58) && !memcmp(config,before,sizeof before));
        load_probe_enabled=1;online_mode=PC_ONLINE_ARCADE;
        CHECK(!pc_story_network_prepare(0x78ad58) && !memcmp(config,before,sizeof before));
        online_mode=PC_ONLINE_STORY;
        WORD_AT(0x77728c)=1;WORD_AT(0x77728c+0x124)=1;
        WORD_AT(0x77728c+4*0x124)=1;
        CHECK(!pc_story_network_prepare(0x78ad58)); /* occupied ID4 exceeds bound */
        WORD_AT(0x77728c+4*0x124)=0;
        config[0]=16; /* actual paired start-builder capacity, not two humans */
        config[4]=0xa9;
        load_probe_enabled=0; /* typed Story works without a test-only gate */
        CHECK(!pc_story_network_prepare(0x7fffff));
        CHECK(pc_story_network_prepare(0x78ad58));
        CHECK(pc_story_network_player_limit()==4);
        pc_story_network_lobby_config();
        CHECK(config[0x54]==3 && config[0x55]==10 && config[0x56]==2 && config[0x57]==2);
        CHECK(config[0]==2 && config[1]==1 && config[4]==0xa9);
        CHECK(config[0]==2 && config[1]==1 && !config[2] && config[0x54]==3 && config[0x57]==2);
        CHECK(story_factories==1 && config[0x60]==0x5a && config[4]==0xa9 && WORD_AT(0x78ad2c)==13);
        WORD_AT(0x76b1a4)=10;CHECK(pc_story_network_prepare(0x78ad58));
        CHECK(story_factories==1); /* pending copy must not rebuild/reseed the serialized config */
        CHECK(config[0x14]==12 && config[0x15]==12 && WORD_AT(0x78adb0)==0x80008);
        memcpy((uint8_t *)memory+0x9000,config,0xa0);
        WORD_AT(0x76b140)=2;story_lobby_mission=0;story_lobby_difficulty=0;
        CHECK(pc_story_network_prepare(0x9000));
        CHECK(story_lobby_mission==1 && story_lobby_difficulty==2);
        ((uint8_t *)memory)[0x9055]=0;
        CHECK(!pc_story_network_prepare(0x9000)); /* Arcade rule cannot be Story */
        ((uint8_t *)memory)[0x9055]=10;
        ((uint8_t *)memory)[0x9000]=16;
        CHECK(!pc_story_network_prepare(0x9000)); /* guests never invent a roster */
        ((uint8_t *)memory)[0x9000]=2;
        WORD_AT(0x78ad20)=13;WORD_AT(0x78ad2c)=13;WORD_AT(0x76b1a4)=10;
        pc_story_network_activate();
        CHECK(WORD_AT(0x78ad20)==4 && WORD_AT(0x78ad2c)==13 && WORD_AT(0x76b1a4)==10);
        CHECK(WORD_AT(0x78ad48)==1 && (WORD_AT(0x78ad54)&255)==2);
        WORD_AT(0x7de30c)=7;WORD_AT(0x7de45c)=0;g_esp=0x1000;
        pc_story_failure_trace(failure_original,0x681c0);
        CHECK(failure_original_calls==1 && g_esp==0x1004 &&
            WORD_AT(0x7de30c)==8 && WORD_AT(0x7de45c)==0x10000);
        load_probe_enabled=0;g_esp=0x1000;
        pc_story_failure_trace(failure_original,0x67450);
        CHECK(failure_original_calls==2 && g_esp==0x1004);
        load_probe_enabled=1;WORD_AT(0x76b1a4)=15;
        WORD_AT(0x7de30c)=7;WORD_AT(0x7de45c)=0;
        WORD_AT(0x790950)=2;WORD_AT(0x7b0c48)=0x6000;
        WORD_AT(0x6114)=16;WORD_AT(0x7698)=16;
        memcpy((uint8_t *)memory+0x7de400,config,0xa0);
        g_esp=0x1000;pc_story_network_frame(frame_original);
        CHECK(context_spawns==2 && failure_original_calls==3 && g_esp==0x1004);
        CHECK(WORD_AT(0x6114)==2 && WORD_AT(0x7698)==2);
        WORD_AT(0x6114)=16;WORD_AT(0x7698)=16;
        g_esp=0x1000;pc_story_network_frame(frame_original);
        CHECK(context_spawns==2 && failure_original_calls==4); /* no resurrection */
        /* Remote replication may activate slot0 before guest startup. This
           must not suppress the single original spawn attempt for slot1. */
        WORD_AT(0x76b1a4)=10;pc_story_network_activate();WORD_AT(0x76b1a4)=15;
        WORD_AT(0x6114)=2;WORD_AT(0x7698)=16;g_esp=0x1000;
        pc_story_network_frame(frame_original);
        CHECK(context_spawns==3 && WORD_AT(0x6114)==2 && WORD_AT(0x7698)==2);
        WORD_AT(0x7698)=16;g_esp=0x1000;pc_story_network_frame(frame_original);
        CHECK(context_spawns==3); /* no retry after later remote inactivity */

        /* Missing extra authored start reuses original slot0 pad lookup,
           without changing slot/ownership or replacing a valid network pad. */
        WORD_AT(0x790950)=4;const uint32_t extra=0x6000+3*0x1584;
        WORD_AT(extra)=3;WORD_AT(extra+4)=0x55;
        g_esp=0x1800;WORD_AT(g_esp+4)=extra;
        pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==77 && g_esp==0x1804 && pad_fallbacks==1);
        CHECK(WORD_AT(extra)==3 && WORD_AT(extra+4)==0x55);
        pad_original_result=88;g_esp=0x1800;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==88 && pad_fallbacks==1);
        pad_original_result=UINT32_MAX;WORD_AT(0x76b1a4)=12;
        g_esp=0x1800;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==UINT32_MAX && pad_fallbacks==1);
        WORD_AT(0x76b1a4)=15;online_mode=PC_ONLINE_ARCADE;
        g_esp=0x1800;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==UINT32_MAX && pad_fallbacks==1);
        online_mode=PC_ONLINE_STORY;WORD_AT(0x790950)=2;
        WORD_AT(0x79094c)=4;g_esp=0x1800;WORD_AT(g_esp)=0x123456;
        pc_story_network_script_trace(script_original,0xd4b00);
        CHECK(script_original_calls==1 && g_esp==0x1804 && g_eax==19);
        CHECK(WORD_AT(0x6114)==256 && (WORD_AT(0x7de45c)&0x2000000));
        /* Tracing must preserve the original suspension, never clear it. */
        g_esp=0x1800;pc_story_failure_trace(reset_original,0xd50e0);
        CHECK(script_original_calls==2 && g_esp==0x1804 && g_eax==33);
        CHECK(WORD_AT(0x6114)==2 && (WORD_AT(0x7de45c)&0x2000000));

    }
    {
        /* Guest X opens only its own Progress; Y cannot reach host settings. */
        WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=12;
        WORD_AT(0x2004)=0x4e99d4;WORD_AT(0x202c)=0x3000;
        story_lobby_mission=0;story_lobby_difficulty=0;
        WORD_AT(0x774028)=0x5000;
        next_menu=0;WORD_AT(0x6b94d8)=0x40;call(0x1a8f30);
        CHECK(ready_calls==1);
        CHECK(!next_menu && !story_lobby_lookup(0x2cb090));
        WORD_AT(0x6b94d8)=0x10;call(0x1a8f30);
        CHECK(next_menu==0x523940 && WORD_AT(0x6b94d8)==0x10 && ready_calls==1);
        CHECK(story_lobby_mission==1 && story_lobby_difficulty==2);
        CHECK(WORD_AT(0x2128)==1 && WORD_AT(0x2138)==2 && WORD_AT(0x2148)==0xffffffffu);
        CHECK(!strcmp((char *)(g_xbox_mem_offset+WORD_AT(0x212c)),"Back"));
        CHECK(!strcmp((char *)(g_xbox_mem_offset+WORD_AT(0x213c)),"Player Progress"));
        g_esp=0x1000;WORD_AT(g_esp)=0x1a8ffc;WORD_AT(g_esp+8)=0x14b;
        WORD_AT(g_esp+24)=0x197890;
        CHECK(pc_story_lobby_button(header_original));
        WORD_AT(0x1008)=0x4000;WORD_AT(0x4014)=0x14b;story_lobby_mission=4;
        call(STORY_BUILD_CB);CHECK(header_mission==4 && WORD_AT(0x4014)==0x14b);

    }
#endif
#ifdef _WIN32
    {
        const float bounds[4]={100,100,500,400};
        memcpy((uint8_t *)memory+0x7000,bounds,sizeof bounds);
        story_body_menu=0x2000;story_body_rect=0x7000;story_lobby_mission=1;
        WORD_AT(0x4d1ad0+0x44)=22;g_esp=0x1800;WORD_AT(g_esp)=0x199650;
        const unsigned old_calls=failure_original_calls;
        pc_story_lobby_details(frame_original);
        CHECK(briefing_draws==1 && g_esp==0x1804 && failure_original_calls==old_calls);
        g_esp=0x1800;WORD_AT(g_esp)=0x199700;pc_story_lobby_details(frame_original);
        CHECK(briefing_draws==1 && failure_original_calls==old_calls+1);
    }
#endif
    puts("PASS: Story host controls, network-mode preservation and difficulty return");
    return 0;
}
