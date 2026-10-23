/* Production host controls against isolated guest RAM and original spies. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pc_online.h"
#include "pc_character_pose.h"
#ifdef TSFP_NATIVE_DISPLAY
#undef _WIN32
#elif !defined(_WIN32)
#define _WIN32 1
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#ifdef _WIN32
static uint32_t memory[0x800000/4],g_esp,g_eax,g_ebx;
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
static unsigned story_factories,story_broadcasts,story_generation;
static int factory_intro;
static unsigned context_spawns,ready_calls;
static unsigned pad_fallbacks;
void sub_000CF060_gen(void){marker=16;}
void sub_0009A420_gen(void){marker=27;}
void sub_0025EE60(void){marker=31;}
static unsigned story_checkpoint;
void sub_00359EB0(void){CHECK(0);}
void sub_0003D7B0(void){marker=9;} void sub_00040F00(void){marker=10;}
void sub_00041DC0(void){marker=11;} void sub_0003FA10(void){marker=12;}
void sub_0003F8D0(void){marker=14;} void sub_0003FCF0(void){marker=15;}
void sub_00154FE0(void){marker=13;} void sub_00198E20(void){g_esp+=4;}
static unsigned briefing_draws,status_draws;
void sub_00065940(void){marker=7;}
void sub_00352640(void){marker=17;}void sub_00352650(void){marker=18;}void sub_00356490(void){marker=19;}
void sub_000D50E0(void){marker=8;}
void sub_00075B70(void){marker=1;} void sub_00071B10(void){marker=2;}
void sub_00071C50(void){marker=3;} void sub_00079D60_gen(void){marker=4;}
void sub_000771D0(void){marker=5;} void sub_000770E0(void){marker=6;}
void sub_00079B70(void){CHECK(0);}
void sub_000DAC30(void){marker=20;}void sub_000D7550(void){marker=21;}
void sub_000A9070(void){marker=22;}void sub_0015BCA0(void){marker=23;}
void sub_0015BCC0(void){marker=24;}void sub_002402B0(void){marker=25;}
void sub_00353A90(void){marker=26;}
void sub_001BA2B0(void){marker=28;}void sub_001BFCA0(void){marker=29;}
void sub_001BA130(void){marker=30;}
static unsigned loadout_ammo,loadout_grants,loadout_selections;
static unsigned npc_ctor_calls,npc_refreshes,npc_removed,npc_sent_size,npc_send_calls;
static unsigned char npc_sent[512],npc_sent_base[256],npc_sent_pose[256];
static unsigned npc_base_size,npc_pose_size;
static PcCharacterPose npc_last_pose;
static unsigned npc_pose_calls;
void pc_character_pose_present(uint32_t entity,const PcCharacterPose *pose)
{CHECK(entity==0x72000);npc_last_pose=*pose;++npc_pose_calls;}
void pc_network_bots_entity_refresh(uint32_t entity){CHECK(entity==0x72000);++npc_refreshes;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t args[10];va_list ap;va_start(ap,count);CHECK(count<=10);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(fn==sub_001BA2B0) {
        CHECK(count==3 && ((args[1]==25 && args[2]==8) || (args[1]==32 && args[2]==6)));loadout_ammo+=args[2];return 8;
    }
    if(fn==sub_001BFCA0) {
        CHECK(count==2 && (args[1]==25 || args[1]==32));((uint8_t *)memory)[args[0]+0x54c+args[1]]=1;
        ++loadout_grants;return 0;
    }
    if(fn==sub_001BA130) {CHECK(count==2 && (args[1]==25 || args[1]==32));++loadout_selections;return 0;}
    if(fn==sub_00353A90) {
        CHECK(count==5 && args[0]==0x30000 && args[1]==0x1e && args[3]<=256u && args[4]==1);
        ++npc_send_calls;npc_sent_size=args[3];memcpy(npc_sent,(uint8_t *)memory+args[2],args[3]);
        if(WORD_AT(args[2]+4)==0x32504e53u){npc_base_size=args[3];memcpy(npc_sent_base,npc_sent,args[3]);}
        if(WORD_AT(args[2]+4)==0x32505053u){npc_pose_size=args[3];memcpy(npc_sent_pose,npc_sent,args[3]);}
        return 0;
    }
    if(fn==sub_000DAC30) {
        CHECK(count==10 && args[0]==12 && args[1]==12 && args[2]==2 && args[9]==UINT32_MAX);
        const uint32_t actor=WORD_AT(0x7356d8)+args[2]*0xea0;
        WORD_AT(actor)=0x72000;WORD_AT(actor+8)=args[2];WORD_AT(0x7207c)=actor;
        WORD_AT(actor+0x48)=12u|(12u<<16);++npc_ctor_calls;return 0x72000;
    }
    if(fn==sub_000D7550){CHECK(count==2);WORD_AT(args[0]+0x84)=args[1];return 0;}
    if(fn==sub_002402B0){CHECK(count==2);WORD_AT(args[1]+0x60)=args[0];return 0;}
    if(fn==sub_0015BCA0) {
        CHECK(count==1);const unsigned n=WORD_AT(0x74c37c);
        WORD_AT(0x7a3580+n*4)=WORD_AT(args[0]+0x7c);WORD_AT(0x74c37c)=n+1;return 0;
    }
    if(fn==sub_0015BCC0) {
        CHECK(count==1);const uint32_t actor=WORD_AT(args[0]+0x7c);
        const unsigned n=WORD_AT(0x74c37c);
        for(unsigned i=0;i<n;i++) if(WORD_AT(0x7a3580+i*4)==actor) {
            for(unsigned j=i+1;j<n;j++)WORD_AT(0x7a3580+(j-1)*4)=WORD_AT(0x7a3580+j*4);
            WORD_AT(0x74c37c)=n-1;break;
        }
        return 0;
    }
    if(fn==sub_000A9070){CHECK(count==1);++npc_removed;return 0;}
    if(fn==sub_0025EE60){CHECK(count==1 && args[0]==0);return story_checkpoint;}
    if(fn==sub_0009A420_gen){CHECK(count==1 && args[0]==0x1f40);return 0x78000;}
    if(fn==sub_000CF060_gen){CHECK(count==1 && WORD_AT(args[0])==0);++pad_fallbacks;return 77;}
    else if(fn==sub_0003D7B0){CHECK(count==1 && (args[0]==22 || args[0]==33));return args[0]==33?0xd000:0xb000;}
    else if(fn==sub_00040F00){CHECK(count==2 && args[0]==0xb000);return 0xc000;}
    else if(fn==sub_0003F8D0){CHECK(count==0);}
    else if(fn==sub_0003FCF0){CHECK(count==4 && args[2]==0xc000);float height=10.f;memcpy(&WORD_AT(args[1]),&height,4);}
    else if(fn==sub_0003FA10){CHECK(count==1);return 3;}
    else if(fn==sub_00154FE0){CHECK(count==2);return args[0];}
    else if(fn==sub_00041DC0){
        CHECK(count==4);
        if(args[2]==0xc000) ++briefing_draws;
        else {
            CHECK(!strcmp((char *)(g_xbox_mem_offset+args[2]),"2401  Hard  2/4 Players"));
            float scale;memcpy(&scale,&WORD_AT(0x7e7270),4);CHECK(scale>.64f && scale<.66f);
            ++status_draws;
        }
    }
    else if(fn==sub_00079B70){CHECK(count==2);next_parent=args[0];next_menu=args[1];}
    else if(fn==sub_00065940){
        CHECK(count==1 && args[0]==0x78ad58 && WORD_AT(0x78ad2c)==4 && WORD_AT(0x78ad48)==1 &&
            (WORD_AT(0x78ad54)&255)==2 && WORD_AT(0x78ad50)==0);
        /* Cover original no-intro fallthrough as well as an intro config. */
        memset((uint8_t *)memory+0x78ad58,0,0xa0);
        ((uint8_t *)memory)[0x78ad58]=1;((uint8_t *)memory)[0x78ad59]=1;
        ((uint8_t *)memory)[0x78adad]=10;((uint8_t *)memory)[0x78adae]=2;
        ((uint8_t *)memory)[0x78adac]=factory_intro?24:3;
        ((uint8_t *)memory)[0x78ad58+0x9b]=7;
        WORD_AT(0x78adb0)=factory_intro?0x20000000:0x80008;
        WORD_AT(0x78ad50)=factory_intro?0:1;
        ((uint8_t *)memory)[0x78ad58+0x60]=0x5a;
        ++story_factories;
    }
    else if(fn==sub_00352640){CHECK(count==0);return 255;}
    else if(fn==sub_00352650){CHECK(count==1);story_generation=args[0];}
    else if(fn==sub_00356490){CHECK(count==1 && args[0]==0x9000);++story_broadcasts;WORD_AT(0x76b1a4)=10;}
    else if(fn==sub_00359EB0){CHECK(count==2 && args[0]==0 && args[1]==1);++ready_calls;((uint8_t *)memory)[WORD_AT(0x774028)+0x2fa]=1;}
    else if(fn==sub_000D50E0){
        CHECK(count==2 && args[1]==1 && (args[0]==0x6000 || args[0]==0x7584));
        CHECK((WORD_AT(args[0]+0x114)==16 || WORD_AT(args[0]+0x114)==4) && WORD_AT(args[0]+0x1504)==0x7fffffff);
        WORD_AT(args[0]+0x114)=2;++context_spawns;
    }
    else if(fn==sub_00075B70){CHECK(count==5 && args[4]==0x1237e);difficulty_rows=0;}
    else if(fn==sub_00079D60_gen){
        static const char *names[]={"Easy","Normal","Hard"};
        CHECK(count==6 && difficulty_rows<3 && args[1]==difficulty_rows && args[4]==0x1237d);
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
#define STORY_SELECT_CB 0x1237du
#define STORY_BUILD_CB 0x1237cu
#define STORY_BACK_CB 0x1237eu
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
static void frame_original(void);
static StoryNpcSpawn npc_captured_spawn(void)
{
    StoryNpcSpawn s;memset(&s,0,sizeof s);memcpy(&s,npc_sent_base+4,STORY_NPC_BASE_BYTES);
    if(s.pose_present){StoryNpcPoseMessage p;memcpy(&p,npc_sent_pose+4,sizeof p);s.pose=p.pose;}
    return s;
}
static void npc_deliver(const StoryNpcSpawn *s,unsigned parts)
{
    const uint32_t checksum=story_npc_checksum(s);
    StoryNpcBaseMessage base;memcpy(base.state,s,STORY_NPC_BASE_BYTES);base.checksum=checksum;
    const StoryNpcPoseMessage pose={STORY_NPC_POSE_MAGIC,s->match,s->slot,s->serial,checksum,s->pose};
    /* Deliberately deliver pose before physical state to exercise assembly. */
    for(unsigned pass=0;pass<2;pass++) {
        const unsigned part=pass?1u:2u;
        if(!(parts&part) || (part==2u && !s->pose_present))continue;
        const void *packet=part==1u?(const void *)&base:(const void *)&pose;
        const unsigned size=part==1u?sizeof base:sizeof pose;
        memcpy((uint8_t *)memory+0x90000,packet,size);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x31000;WORD_AT(g_esp+8)=0x90000;
        WORD_AT(g_esp+12)=0x1e;WORD_AT(g_esp+16)=size+4;
        pc_story_network_npc_receive(frame_original);
    }
}
#endif

#ifdef _WIN32
static void npc_constructor_original(void)
{
    const uint32_t slot=WORD_AT(g_esp+12),actor=WORD_AT(0x7356d8)+slot*0xea0;
    WORD_AT(actor)=0x70000;WORD_AT(actor+8)=slot;WORD_AT(0x7007c)=actor;
    WORD_AT(actor+0x48)=WORD_AT(g_esp+4)|(WORD_AT(g_esp+8)<<16);
    g_eax=0x70000;g_esp+=4;
}
static void npc_spawn_original(void)
{
    const uint32_t actor=WORD_AT(WORD_AT(g_esp+4)+0x7c);
    WORD_AT(actor+0x10)=2;WORD_AT(actor+0x24)=1;WORD_AT(actor+0x84)=5;WORD_AT(actor+0x94)=14;
    WORD_AT(0x70028)=0x71;WORD_AT(0x70230)=59;WORD_AT(0x70258)=7;
    WORD_AT(0x70004)=0x74000;WORD_AT(0x740d4)=42;WORD_AT(0x7412c)=43;
    const float channel=.25f,radius=.5f;memcpy(&WORD_AT(0x740dc),&channel,4);
    memcpy(&WORD_AT(0x701a8),&channel,4);memcpy(&WORD_AT(0x701c0),&radius,4);
    const float health=20.f,pos=3.f;
    memcpy(&WORD_AT(0x701cc),&health,4);memcpy(&WORD_AT(0x701d8),&health,4);
    memcpy(&WORD_AT(0x70064),&pos,4);
    WORD_AT(0x74c37c)=3;WORD_AT(0x7a3588)=actor;g_eax=0x55;g_esp+=4;
}
#endif
#ifdef _WIN32
#undef getenv
#endif
#ifdef _WIN32
static unsigned failure_original_calls;
static uint32_t pad_original_result=UINT32_MAX;
static void pad_original(void){g_eax=pad_original_result;g_esp+=4;}
static void team_original(void)
{WORD_AT(WORD_AT(g_esp+4)+0x84)=WORD_AT(g_esp+8);g_eax=77;g_esp+=4;}
static unsigned hit_original_calls;
static void hit_trace_original(void)
{
    const uint32_t entity=WORD_AT(WORD_AT(g_esp+4));float health;
    memcpy(&health,(uint8_t *)memory+entity+0x1cc,4);health-=2.5f;
    memcpy((uint8_t *)memory+entity+0x1cc,&health,4);
    ++hit_original_calls;g_eax=23;g_esp+=4;
}
static void loadout_ammo_original(void){g_eax=9;g_esp+=4;}
static void loadout_grant_original(void)
{((uint8_t *)memory)[WORD_AT(g_esp+4)+0x54c+WORD_AT(g_esp+8)]=1;g_eax=11;g_esp+=4;}
static void start_tag_original(void){CHECK(WORD_AT(g_esp+4)==0x1f40);g_eax=1001;g_esp+=4;}
static unsigned script_original_calls;
static void script_original(void)
{++script_original_calls;WORD_AT(0x7de45c)|=0x2000000;WORD_AT(0x6114)=256;g_eax=19;g_esp+=4;}
static void reset_original(void)
{++script_original_calls;WORD_AT(0x6114)=2;g_eax=33;g_esp+=4;}
static void frame_original(void){++failure_original_calls;g_esp+=4;}
static unsigned network_starts;
static void ready_original(void){
    CHECK(WORD_AT(0x790950)==0 && !(WORD_AT(0x7de458)&0x20000000));
    WORD_AT(0x76b1a4)=15;g_eax=17;g_esp+=4;++network_starts;
}
static uint32_t authored_flags;
static void asset_original(void){authored_flags=WORD_AT(g_esp+4u);g_eax=23;g_esp+=4u;}
static unsigned removed_characters;
static void remove_character_original(void){++removed_characters;--WORD_AT(0x74c37c);g_esp+=4;}
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
        CHECK(WORD_AT(0x78ad48)==1 && WORD_AT(0x78ad50)==1 && (WORD_AT(0x78ad54)&255)==2);
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
        WORD_AT(0x6114)=2;WORD_AT(0x7504)=0x1234;WORD_AT(0x7698)=16;g_esp=0x1000;
        pc_story_network_frame(frame_original);
        CHECK(context_spawns==3 && WORD_AT(0x6114)==2 && WORD_AT(0x7698)==2);
        CHECK(WORD_AT(0x7504)==0x1234); /* retain active remote's cache */
        WORD_AT(0x7698)=16;g_esp=0x1000;pc_story_network_frame(frame_original);
        CHECK(context_spawns==3); /* no retry after later remote inactivity */

        /* Missing extra authored start reuses original slot0 pad lookup,
           without changing slot/ownership or replacing a valid network pad. */
        WORD_AT(0x79094c)=4;WORD_AT(0x790950)=4;const uint32_t extra=0x6000+3*0x1584;
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
        /* The Arcade reconciliation must not delete authored Story NPCs.
           Ordinary script/death removals, non-Story and lobby paths still run. */
        WORD_AT(0x74c37c)=6;g_esp=0x1800;WORD_AT(g_esp)=0x355ea4;
        pc_story_network_character_remove(remove_character_original);
        CHECK(WORD_AT(0x74c37c)==6 && !removed_characters && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp)=0x15c490;
        pc_story_network_character_remove(remove_character_original);
        CHECK(WORD_AT(0x74c37c)==5 && removed_characters==1 && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp)=0x355ea4;online_mode=PC_ONLINE_ARCADE;
        pc_story_network_character_remove(remove_character_original);
        CHECK(WORD_AT(0x74c37c)==4 && removed_characters==2 && g_esp==0x1804);
        g_esp=0x1800;online_mode=PC_ONLINE_STORY;WORD_AT(0x76b1a4)=12;WORD_AT(0x79094c)=101;
        pc_story_network_character_remove(remove_character_original);
        CHECK(WORD_AT(0x74c37c)==3 && removed_characters==3 && g_esp==0x1804);
        /* Preserve loading characters even before the active scene/config
           changes from the lobby, and then through network phase14. */
        story_context_start_pending=1;g_esp=0x1800;
        pc_story_network_character_remove(remove_character_original);
        CHECK(WORD_AT(0x74c37c)==3 && removed_characters==3 && g_esp==0x1804);
        story_context_start_pending=0;WORD_AT(0x79094c)=4;WORD_AT(0x76b1a4)=14;g_esp=0x1800;
        pc_story_network_character_remove(remove_character_original);
        CHECK(WORD_AT(0x74c37c)==3 && removed_characters==3 && g_esp==0x1804);
        WORD_AT(0x76b1a4)=15;
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
        /* Metadata uses localized mission year and counts actual occupied slots,
           including a sparse roster; scoped font/stack state must be restored. */
        story_lobby_mission=0;story_lobby_difficulty=2;
        WORD_AT(0x4d1ac0)=33;strcpy((char *)memory+0xd000,"2401");
        for(unsigned i=0;i<16;i++)WORD_AT(0x77728c+i*0x124)=0;
        WORD_AT(0x77728c)=1;WORD_AT(0x77728c+3*0x124)=1;
        const float unit=1.f;memcpy(&WORD_AT(0x7e7270),&unit,4);memcpy(&WORD_AT(0x7e7280),&unit,4);
        g_esp=0x1804;story_lobby_status(0x2000,0x7000,0x1800);
        CHECK(status_draws==1 && g_esp==0x1804);
        CHECK(WORD_AT(0x7e7270)==0x3f800000 && WORD_AT(0x7e7280)==0x3f800000);
        story_lobby_difficulty=3;story_lobby_status(0x2000,0x7000,0x1800);
        CHECK(status_draws==1 && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp)=0x199700;pc_story_lobby_details(frame_original);
        CHECK(briefing_draws==1 && failure_original_calls==old_calls+1);
    }
    {
        /* Intro is serialized as an original cinematic config; each guest waits
           for the host's reliable gameplay config instead of advancing alone. */
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=11;
        story_lobby_mission=1;story_lobby_difficulty=2;factory_intro=1;
        for(unsigned i=0;i<16;i++)WORD_AT(0x77728c+i*0x124)=i<2?1:0;
        for(unsigned i=0;i<13;i++)WORD_AT(0x4d1ae0+i*0x44)=UINT32_MAX;
        WORD_AT(0x4d1ae0+0x44)=7;((uint8_t *)memory)[0x51fc30+7*36]=24;
        uint8_t *config=(uint8_t *)memory+0x78ad58;
        config[0]=2;config[1]=1;config[4]=0xa9;
        g_esp=0x1800;CHECK(pc_story_network_prepare(0x78ad58));
        CHECK(config[0]==2 && config[1]==1 && config[4]==0xa9 && config[0x54]==24);
        CHECK(WORD_AT(0x78adb0)==0x20000000 && story_pending_stage==0);
        pc_story_network_activate();CHECK(WORD_AT(0x78ad50)==0 && (WORD_AT(0x78ad54)&255)==2);
        memcpy((uint8_t *)memory+0x7de400,config,0xa0);
        WORD_AT(0x79094c)=24;WORD_AT(0x790950)=2;WORD_AT(0x76b1a4)=10;
        g_esp=0x1800;pc_story_network_ready(ready_original);
        CHECK(network_starts==1 && WORD_AT(0x76b1a4)==15 && g_esp==0x1804 && g_eax==17);
        CHECK(WORD_AT(0x790950)==2 && WORD_AT(0x7de458)==0x20000000);
        WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=10;
        const unsigned previous=failure_original_calls;
        g_esp=0x1800;pc_story_network_advance(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==previous);
        WORD_AT(0x76b140)=1;g_esp=0x1800;pc_story_network_advance(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==previous+1);
        memcpy((uint8_t *)memory+0x9000,config,0xa0);
        ((uint8_t *)memory)[0x9000]=1;((uint8_t *)memory)[0x9054]=3;WORD_AT(0x9058)=0x80008;
        g_esp=0x1800;WORD_AT(g_esp)=0x65cdc;
        CHECK(pc_story_network_prepare(0x9000));
        CHECK(((uint8_t *)memory)[0x9000]==2 && ((uint8_t *)memory)[0x9004]==0xa9);
        CHECK(story_broadcasts==1 && story_generation==1 && WORD_AT(0x76b1a4)==10);
        CHECK(g_esp==0x1800 && story_pending_stage==1);
        pc_story_network_activate();CHECK(WORD_AT(0x78ad50)==1);
        WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=15;
        CHECK(!pc_story_network_prepare(0x9000) && story_broadcasts==1);
        memcpy((uint8_t *)memory+0x78ad58,(uint8_t *)memory+0x9000,0xa0);
        g_esp=0x1800;WORD_AT(g_esp)=0x2d398d;
        CHECK(pc_story_network_prepare(0x78ad58));
        CHECK(story_pending_stage==1 && story_broadcasts==1);
        pc_story_network_activate();CHECK(WORD_AT(0x78ad50)==1);
        WORD_AT(0x7de458)=0x80008;WORD_AT(0x79094c)=3;
        g_esp=0x1800;pc_story_network_ready(ready_original);
        CHECK(network_starts==2 && WORD_AT(0x790950)==2 && WORD_AT(0x7de458)==0x80008);
        story_context_start_pending=0;g_esp=0x1800;
        pc_story_network_ready(frame_original);
        CHECK(failure_original_calls==previous+2 && g_esp==0x1804);
        WORD_AT(0x790950)=4;g_esp=0x1800;WORD_AT(g_esp+4)=0x80400200;
        pc_story_network_asset(asset_original);
        CHECK(authored_flags==0x200 && WORD_AT(0x1804)==0x80400200 && g_eax==23 && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x80200200;
        pc_story_network_asset(asset_original);CHECK(authored_flags==0x200200); /* keep solo-only rejection */
        WORD_AT(0x790950)=2;g_esp=0x1800;WORD_AT(g_esp+4)=0x80400100;
        pc_story_network_asset(asset_original);CHECK(authored_flags==0x400100); /* difficulty retained */
        online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;WORD_AT(g_esp+4)=0x80400200;
        pc_story_network_asset(asset_original);CHECK(authored_flags==0x80400200);
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=4;((uint8_t *)memory)[0x7de455]=10;
        WORD_AT(0x7356d8)=0x50000;
        const uint32_t npc=0x50000+4*0xea0;
        WORD_AT(npc)=0x70000;WORD_AT(npc+8)=4;WORD_AT(0x7007c)=npc;
        CHECK(pc_story_network_actor_active(0xd8e5d,4,0x70000));
        WORD_AT(npc)=0;
        CHECK(pc_story_network_actor_active(0xd8e5d,4,0x70000)); /* dormant script, no body yet */
        WORD_AT(npc)=0x71000;
        CHECK(!pc_story_network_actor_active(0xd8e5d,4,0x70000)); /* obsolete prototype */
        WORD_AT(npc)=0x70000;
        CHECK(!pc_story_network_actor_active(0x15c5a3,4,0x70000)); /* controller enumeration */
        CHECK(!pc_story_network_actor_active(0xd8e5d,0,0x70000)); /* human */
        CHECK(!pc_story_network_actor_active(0xd8e5d,32,0x70000)); /* outside actor pool */
        WORD_AT(0x70150)=0x6000;
        CHECK(!pc_story_network_actor_active(0xd8e5d,4,0x70000)); /* must be non-player */
        WORD_AT(0x70150)=0;WORD_AT(0x7007c)=0;
        CHECK(!pc_story_network_actor_active(0xd8e5d,4,0x70000)); /* stale body */
        WORD_AT(0x7007c)=npc;WORD_AT(0x76b140)=2;
        CHECK(pc_story_network_actor_active(0xd8e5d,4,0x70000)); /* later stock gate decides ownership */
        WORD_AT(0x76b1a4)=12;CHECK(!pc_story_network_actor_active(0xd8e5d,4,0x70000));
        WORD_AT(0x76b1a4)=15;online_mode=PC_ONLINE_ARCADE;
        CHECK(!pc_story_network_actor_active(0xd8e5d,4,0x70000));
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b140)=1;
        ((uint8_t *)memory)[0x7de400]=2;((uint8_t *)memory)[0x7de401]=1;
        WORD_AT(0x790950)=2;WORD_AT(0x7b0c48)=0x6000;WORD_AT(0x7de45c)=0;
        WORD_AT(0x7de30c)=7;story_pending_stage=1;pc_story_network_activate();
        WORD_AT(0x6114)=4;WORD_AT(0x7698)=16;
        const unsigned spawns_before=context_spawns;
        g_esp=0x1800;pc_story_network_frame(frame_original);
        CHECK(context_spawns==spawns_before+2 && WORD_AT(0x6114)==2 && WORD_AT(0x7698)==2);
        WORD_AT(0x6114)=4;g_esp=0x1800;pc_story_network_frame(frame_original);
        CHECK(context_spawns==spawns_before+2); /* initial load only */
    }
    {
        /* Extra initial starts must remain distinct across the16bit spawn wire,
           without changing the first two players or a later checkpoint pad. */
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de455]=10;WORD_AT(0x790950)=4;WORD_AT(0x7b0c48)=0x6000;
        story_checkpoint=0;story_extra_pad_storage=0xa000;
        const uint32_t extra=0x6000+2*0x1584;
        WORD_AT(extra)=2;g_esp=0x1800;WORD_AT(g_esp+4)=extra;
        pad_original_result=88;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==0x7f02 && g_esp==0x1804 && WORD_AT(extra)==2);
        WORD_AT(extra+0x1584)=3;g_esp=0x1800;WORD_AT(g_esp+4)=extra+0x1584;
        pc_story_network_spawn_pad(pad_original);CHECK(g_eax==0x7f03);
        const float position[3]={10.f,20.f,30.f};
        memset((uint8_t *)memory+0x78000,0x5a,32);
        memcpy((uint8_t *)memory+0x7800c,position,12);
        for(unsigned slot=2;slot<4;slot++) {
            g_esp=0x1800;WORD_AT(g_esp+4)=0x7f00+slot;
            pc_story_network_start_record(frame_original);
            const uint32_t target=0xa000+(slot-2)*32;
            float p[3];memcpy(p,(uint8_t *)memory+target+12,12);
            CHECK(g_eax==target && g_esp==0x1804 && p[0]==(slot==2?8.f:12.f) &&
                  p[1]==20.f && p[2]==30.f && WORD_AT(target+4)==0x5a5a5a5a);
        }
        float original_x;memcpy(&original_x,(uint8_t *)memory+0x7800c,4);CHECK(original_x==10.f);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x7f03;
        pc_story_network_start_tag(start_tag_original);
        CHECK(g_eax==1001 && g_esp==0x1804 && WORD_AT(0x1804)==0x7f03);
        g_esp=0x1800;WORD_AT(g_esp+4)=0xa020;
        pc_story_network_start_index(frame_original);
        CHECK(g_eax==0x7f03 && g_esp==0x1804 && (uint16_t)g_eax==0x7f03);

        /* Remote network setup can follow the first active body; keep the
           initial pad choice until the original checkpoint predicate changes. */
        WORD_AT(extra+0x114)=2;WORD_AT(extra+0x1584+0x114)=4;
        story_context_start_pending=0;g_esp=0x1800;pc_story_network_frame(frame_original);
        CHECK(story_checkpoint==0);
        g_esp=0x1800;WORD_AT(g_esp+4)=extra;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==0x7f02); /* an active body is not the end of network setup */
        for(unsigned slot=0;slot<2;slot++) {
            const uint32_t context=0x6000+slot*0x1584;WORD_AT(context)=slot;
            g_esp=0x1800;WORD_AT(g_esp+4)=context;pc_story_network_spawn_pad(pad_original);
            CHECK(g_eax==(slot?0x1f62u:0x1f40u));
        }
        story_checkpoint=1;g_esp=0x1800;WORD_AT(g_esp+4)=extra;
        pc_story_network_spawn_pad(pad_original);CHECK(g_eax==88); /* checkpoint owns selection */
        const unsigned checkpoint_fallbacks=pad_fallbacks;
        pad_original_result=UINT32_MAX;g_esp=0x1800;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==77 && pad_fallbacks==checkpoint_fallbacks+1 && WORD_AT(extra)==2);
        pad_original_result=88;story_checkpoint=0;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x7f02;pc_story_network_start_record(frame_original);
        CHECK(g_eax==0xa000);
        WORD_AT(0x79094c)=3;g_esp=0x1800;WORD_AT(g_esp+4)=extra+0x1584;
        pc_story_network_spawn_pad(pad_original);CHECK(g_eax==88);
        WORD_AT(0x79094c)=2;story_checkpoint=0;
        story_extra_pad_storage=0;allocation_ok=0;g_esp=0x1800;WORD_AT(g_esp+4)=extra;
        pc_story_network_spawn_pad(pad_original);CHECK(g_eax==88);
        allocation_ok=1;story_checkpoint=0;
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        uint8_t *config=(uint8_t *)memory+0x7de400;
        config[0]=2;config[0x54]=2;config[0x55]=10;config[0x56]=2;config[0x99]=7;
        WORD_AT(0x7356d8)=0x50000;WORD_AT(0x774028)=0x30000;story_npc_reset();
        g_esp=0x1800;for(unsigned i=1;i<=10;i++)WORD_AT(g_esp+i*4)=0;
        WORD_AT(g_esp+4)=12;WORD_AT(g_esp+8)=12;WORD_AT(g_esp+12)=2;WORD_AT(g_esp+40)=UINT32_MAX;
        pc_story_network_npc_constructor(npc_constructor_original);
        CHECK(g_esp==0x1804 && g_eax==0x70000 && story_npc_constructors[2].valid && story_npc_kinds[12]);
        const unsigned wire_before=npc_send_calls;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x70000;pc_story_network_npc_spawn(npc_spawn_original);
        CHECK(g_esp==0x1804 && g_eax==0x55 && npc_send_calls==wire_before+2);
        CHECK(npc_base_size==sizeof(StoryNpcBaseMessage)+4 && npc_pose_size==sizeof(StoryNpcPoseMessage)+4);
        StoryNpcSpawn packet=npc_captured_spawn();
        CHECK(story_npc_valid(&packet) && packet.slot==2 && packet.first==12 && packet.serial==1 &&
              packet.present && packet.in_roster && packet.network_id==59 && packet.pose_present &&
              packet.pose.clip==42 && packet.pose.next_clip==43 && packet.pose.clip_channels[0]==.25f);
        g_esp=0x1900;WORD_AT(g_esp)=0x35cfed;WORD_AT(g_esp+4)=0x1e;
        WORD_AT(g_esp+12)=0x80000;((uint8_t *)memory)[0x80004]=2;g_ebx=11;
        const unsigned originals_before=failure_original_calls;
        pc_story_network_npc_send(frame_original);
        CHECK(g_eax==1 && g_esp==0x1904 && failure_original_calls==originals_before);
        const uint32_t actor=0x50000+2*0xea0;
        WORD_AT(0x7de318)=10;g_esp=0x1800;story_npc_pending_frame();
        CHECK(npc_send_calls==wire_before+4 && g_esp==0x1800);
        StoryNpcSpawn state=npc_captured_spawn();CHECK(state.serial==2 && state.present && state.in_roster);
        story_npc_pending_frame();WORD_AT(0x7de318)=15;story_npc_pending_frame();CHECK(npc_send_calls==wire_before+4);
        WORD_AT(0x74c37c)=2;WORD_AT(0x7de318)=16;story_npc_pending_frame();state=npc_captured_spawn();
        CHECK(npc_send_calls==wire_before+6 && state.present && !state.in_roster);
        WORD_AT(actor)=0;WORD_AT(0x7de318)=22;story_npc_pending_frame();state=npc_captured_spawn();
        CHECK(npc_send_calls==wire_before+7 && !state.present && !state.pose_present);
        WORD_AT(0x76b1a4)=10;WORD_AT(0x7de318)=28;story_npc_pending_frame();
        CHECK(npc_send_calls==wire_before+7);WORD_AT(0x76b1a4)=15;
        WORD_AT(actor)=0;WORD_AT(0x74c37c)=2;WORD_AT(0x7a3580)=0x50000;WORD_AT(0x7a3584)=0x50ea0;
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=0;WORD_AT(0x31000)=0;
        npc_deliver(&packet,2);CHECK(npc_ctor_calls==0 && !story_npc_fresh[2]);
        npc_deliver(&packet,1);
        CHECK(g_esp==0x1804 && g_eax==0 && npc_ctor_calls==1 && npc_refreshes==1);
        CHECK(WORD_AT(actor)==0x72000 && WORD_AT(0x72230)==59 && WORD_AT(0x72258)==7 &&
              WORD_AT(actor+0x84)==5 && WORD_AT(0x74c37c)==3);
        float health,pos;memcpy(&health,&WORD_AT(0x721cc),4);memcpy(&pos,&WORD_AT(0x72064),4);
        CHECK(health==20.f && pos==3.f && npc_pose_calls==1 && npc_last_pose.clip==42);
        CHECK(pc_story_network_replica(0x72000));WORD_AT(0x721a8)=0;
        CHECK(pc_story_network_replica_bounds(0x72000));float bound;memcpy(&bound,&WORD_AT(0x721a8),4);CHECK(bound==.25f);
        WORD_AT(0x72258)=8;CHECK(!pc_story_network_replica(0x72000));WORD_AT(0x72258)=7;
        WORD_AT(0x72150)=0x6000;CHECK(!pc_story_network_replica(0x72000));WORD_AT(0x72150)=0;
        npc_deliver(&packet,3);CHECK(npc_ctor_calls==1 && npc_refreshes==1 && WORD_AT(0x74c37c)==3);
        StoryNpcSpawn bad=packet;bad.serial=2;bad.health=NAN;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        bad=packet;bad.serial=2;bad.pose.clip_channels[0]=NAN;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        bad=packet;bad.serial=2;bad.bounds_radius[0]=-1.f;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        WORD_AT(0x31000)=1;npc_deliver(&packet,3);CHECK(npc_refreshes==1);WORD_AT(0x31000)=0;
        StoryNpcSpawn queued=packet;queued.serial=2;queued.position[0]=7.f;
        WORD_AT(0x76b1a4)=10;npc_deliver(&queued,3);CHECK(story_npc_fresh[2] && npc_refreshes==1);
        WORD_AT(0x76b1a4)=15;story_npc_pending_frame();memcpy(&pos,&WORD_AT(0x72064),4);
        CHECK(!story_npc_fresh[2] && npc_refreshes==2 && pos==7.f);
        npc_deliver(&packet,3);CHECK(npc_refreshes==2);
        StoryNpcSpawn other=queued;queued.serial=3;other.serial=3;other.position[0]=11.f;
        npc_deliver(&queued,1);npc_deliver(&other,2);CHECK(npc_refreshes==2); /* no mixed snapshots */
        npc_deliver(&other,1);CHECK(npc_refreshes==3);
        queued.serial=4;queued.present=0;queued.pose_present=0;queued.in_roster=0;memset(&queued.pose,0,sizeof queued.pose);
        npc_deliver(&queued,1);CHECK(WORD_AT(actor)==0 && WORD_AT(0x74c37c)==2 && npc_removed==1);
        CHECK(!pc_story_network_replica(0x72000));npc_deliver(&queued,3);CHECK(npc_removed==1);
        online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;pc_story_network_npc_receive(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==originals_before+1);
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        uint8_t *config=(uint8_t *)memory+0x7de400;
        config[0]=4;config[0x54]=2;config[0x55]=10;config[0x56]=2;config[0x99]=8;
        WORD_AT(0x7356d8)=0x50000;WORD_AT(0x50008)=0;
        story_loadout_reset();WORD_AT(0x774028)=0x30000;
        g_esp=0x1800;WORD_AT(g_esp)=0x218967;WORD_AT(g_esp+4)=0x50000;
        WORD_AT(g_esp+8)=25;WORD_AT(g_esp+12)=8;
        pc_story_network_loadout_ammo(loadout_ammo_original);
        CHECK(g_eax==9 && g_esp==0x1804 && story_loadout_actor==0x50000);
        g_esp=0x1800;WORD_AT(g_esp)=0x123456;
        pc_story_network_loadout_grant(loadout_grant_original);
        CHECK(!story_loadout_host[0].serial); /* a pickup elsewhere is not this handoff */
        g_esp=0x1800;WORD_AT(g_esp)=0x21896f;
        pc_story_network_loadout_grant(loadout_grant_original);
        CHECK(g_eax==11 && g_esp==0x1804 && story_loadout_host[0].serial==1 &&
              story_loadout_host[0].mask==14 && story_loadout_host[0].magazines==8);
        const unsigned sent_before=npc_send_calls;
        g_esp=0x1800;WORD_AT(0x7de318)=10;story_loadout_frame();
        CHECK(npc_send_calls==sent_before+1 && npc_sent_size==sizeof(StoryLoadout)+4 && g_esp==0x1800);
        StoryLoadout wire;memcpy(&wire,npc_sent+4,sizeof wire);CHECK(story_loadout_valid(&wire));
        story_loadout_frame();WORD_AT(0x7de318)=69;story_loadout_frame();
        CHECK(npc_send_calls==sent_before+1);
        WORD_AT(0x7de318)=70;story_loadout_frame();CHECK(npc_send_calls==sent_before+2);
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=0;WORD_AT(0x31000)=0;
        memcpy((uint8_t *)memory+0x90000,&wire,sizeof wire);
        WORD_AT(0x31000)=1;CHECK(story_loadout_receive(0x31000,0x90000,0x1e,sizeof wire+4));
        CHECK(!story_loadout_pending[0].serial); /* guest sender rejected */
        WORD_AT(0x31000)=0;story_loadout_receive(0x31000,0x90000,0x1e,sizeof wire-1);
        CHECK(!story_loadout_pending[0].serial);
        StoryLoadout malformed=wire;malformed.magazines=0;
        memcpy((uint8_t *)memory+0x90000,&malformed,sizeof wire);
        story_loadout_receive(0x31000,0x90000,0x1e,sizeof wire+4);CHECK(!story_loadout_pending[0].serial);
        memcpy((uint8_t *)memory+0x90000,&wire,sizeof wire);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x31000;WORD_AT(g_esp+8)=0x90000;
        WORD_AT(g_esp+12)=0x1e;WORD_AT(g_esp+16)=sizeof wire+4;
        WORD_AT(0x76b1a4)=10;pc_story_network_npc_receive(frame_original);
        CHECK(g_esp==0x1804 && story_loadout_pending[0].serial==1);
        story_loadout_frame();CHECK(!loadout_grants); /* loading waits */
        const uint32_t contexts=0x6000,pool=0x50000;WORD_AT(0x7b0c48)=contexts;
        WORD_AT(contexts+0x1584+4)=UINT32_MAX;
        for(unsigned slot=2;slot<4;slot++) {
            const uint32_t context=contexts+slot*0x1584,actor=pool+slot*0xea0;
            WORD_AT(context)=slot;WORD_AT(context+4)=slot==2?0:UINT32_MAX;WORD_AT(context+0x114)=2;
            const uint32_t entity=0x73000+(slot-2)*0x300;
            WORD_AT(actor)=entity;WORD_AT(actor+8)=slot;WORD_AT(entity+0x7c)=actor;
            WORD_AT(entity+0x150)=context;WORD_AT(context+0x14)=entity;
            ((uint8_t *)memory)[actor+0x54c+25]=0;
        }
        WORD_AT(0x76b1a4)=15;g_esp=0x1800;story_loadout_frame();
        CHECK(loadout_ammo==8 && loadout_grants==1 && loadout_selections==1 && g_esp==0x1800);
        story_loadout_receive(0x31000,0x90000,0x1e,sizeof wire+4);story_loadout_frame();
        CHECK(loadout_ammo==8 && loadout_grants==1); /* no reliable replay refill */
        WORD_AT(contexts+2*0x1584+4)=UINT32_MAX;WORD_AT(contexts+3*0x1584+4)=0;
        story_loadout_frame();CHECK(loadout_ammo==16 && loadout_grants==2);
        story_loadout_reset();story_loadout_pending[0]=wire;story_loadout_pending[0].match++;
        story_loadout_frame();CHECK(loadout_ammo==16); /* stale epoch */
        story_loadout_pending[0]=wire;story_loadout_frame();
        CHECK(loadout_ammo==16 && loadout_grants==2); /* already-owned inventory preserved */
        online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;WORD_AT(g_esp+4)=pool;WORD_AT(g_esp+8)=25;
        pc_story_network_loadout_grant(loadout_grant_original);
        CHECK(g_eax==11 && g_esp==0x1804 && !story_loadout_host[0].serial);
    }
    {
        /* Both authored gifts are reliable and independent; remote animation
           callbacks may not award ammunition a second time. */
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=2;((uint8_t *)memory)[0x7de499]=9;
        WORD_AT(0x7356d8)=0x50000;WORD_AT(0x50008)=0;WORD_AT(0x7b0c48)=0x6000;
        story_loadout_reset();g_esp=0x1800;WORD_AT(g_esp)=0x218a54;
        WORD_AT(g_esp+4)=0x50000;WORD_AT(g_esp+8)=32;WORD_AT(g_esp+12)=6;
        pc_story_network_loadout_ammo(loadout_ammo_original);
        g_esp=0x1800;WORD_AT(g_esp)=0x218a5c;pc_story_network_loadout_grant(loadout_grant_original);
        CHECK(story_loadout_host[1].weapon==32 && story_loadout_host[1].magazines==6 &&
              story_loadout_host[1].mask==2 && story_loadout_host[1].serial==2);
        const unsigned original_before=failure_original_calls;
        g_esp=0x1800;pc_story_network_handoff(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==original_before+1); /* host original */
        WORD_AT(0x76b140)=2;g_esp=0x1800;pc_story_network_handoff(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==original_before+1); /* guest suppressed */
        StoryLoadout rifle=story_loadout_host[1],pistol=rifle;pistol.serial=1;pistol.weapon=25;pistol.magazines=8;
        WORD_AT(0x31000)=0;WORD_AT(0x774040)=0;
        memcpy((uint8_t *)memory+0x90000,&rifle,sizeof rifle);story_loadout_receive(0x31000,0x90000,0x1e,sizeof rifle+4);
        memcpy((uint8_t *)memory+0x90000,&pistol,sizeof pistol);story_loadout_receive(0x31000,0x90000,0x1e,sizeof pistol+4);
        const uint32_t context=0x6000+0x1584,actor=0x50000+0xea0,entity=0x76000;
        WORD_AT(context)=1;WORD_AT(context+4)=0;WORD_AT(context+0x114)=2;WORD_AT(context+0x14)=entity;
        WORD_AT(actor)=entity;WORD_AT(actor+8)=1;WORD_AT(entity+0x7c)=actor;WORD_AT(entity+0x150)=context;
        ((uint8_t *)memory)[actor+0x54c+25]=1;((uint8_t *)memory)[actor+0x54c+32]=0;
        const unsigned ammo_before=loadout_ammo,grants_before=loadout_grants;
        g_esp=0x1800;story_loadout_frame();
        CHECK(loadout_ammo==ammo_before+6 && loadout_grants==grants_before+1 &&
              ((uint8_t *)memory)[actor+0x54c+25] && ((uint8_t *)memory)[actor+0x54c+32]);
        story_loadout_frame();CHECK(loadout_ammo==ammo_before+6);
        online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;pc_story_network_handoff(frame_original);
        CHECK(failure_original_calls==original_before+2 && g_esp==0x1804);
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=2;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=4;((uint8_t *)memory)[0x7de455]=10;WORD_AT(0x7356d8)=0x50000;
        const uint32_t actor=0x50000+2*0xea0;WORD_AT(actor+8)=2;
        g_esp=0x1800;WORD_AT(g_esp)=0x33acdb;WORD_AT(g_esp+4)=actor;WORD_AT(g_esp+8)=3;
        pc_story_network_team(team_original);
        CHECK(WORD_AT(actor+0x84)==1 && WORD_AT(0x1808)==3 && g_eax==77 && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp)=0x25a44e;pc_story_network_team(team_original);
        CHECK(WORD_AT(actor+0x84)==3); /* other callers keep their arguments */
        WORD_AT(0x790950)=4;WORD_AT(0x7b0c48)=0x6000;
        for(unsigned slot=0;slot<4;slot++) {
            const uint32_t human=0x50000+slot*0xea0,context=0x6000+slot*0x1584,body=0x74000+slot*0x400;
            WORD_AT(human)=body;WORD_AT(human+8)=slot;WORD_AT(human+0x84)=slot+1;
            WORD_AT(context)=slot;WORD_AT(context+0x14)=body;
            WORD_AT(body+0x7c)=human;WORD_AT(body+0x150)=context;
        }
        for(unsigned role=1;role<=2;role++) for(unsigned phase=10;phase<=15;phase++) {
            if(phase!=10 && phase!=14 && phase!=15)continue;
            WORD_AT(0x76b140)=role;WORD_AT(0x76b1a4)=phase;
            for(unsigned slot=0;slot<4;slot++)WORD_AT(0x50000+slot*0xea0+0x84)=slot+1;
            g_esp=0x1800;story_human_teams_frame();CHECK(g_esp==0x1800);
            for(unsigned slot=0;slot<4;slot++)CHECK(WORD_AT(0x50000+slot*0xea0+0x84)==1);
        }
        WORD_AT(actor+0x84)=3;((uint8_t *)memory)[0x7de455]=1;
        story_human_teams_frame();CHECK(WORD_AT(actor+0x84)==3);
        ((uint8_t *)memory)[0x7de455]=10;
        WORD_AT(0x74800+0x150)=0;story_human_teams_frame();CHECK(WORD_AT(actor+0x84)==3);
        WORD_AT(0x74800+0x150)=0x6000+2*0x1584;
        g_esp=0x1800;WORD_AT(g_esp+4)=actor;WORD_AT(g_esp+8)=3;
        g_esp=0x1800;WORD_AT(g_esp)=0x33acdb;WORD_AT(actor+8)=4;
        pc_story_network_team(team_original);CHECK(WORD_AT(actor+0x84)==3); /* NPC rejected */
        WORD_AT(actor+8)=2;online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;
        pc_story_network_team(team_original);CHECK(WORD_AT(actor+0x84)==3);
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x76b170)=1;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=4;WORD_AT(0x7356d8)=0x50000;WORD_AT(0x7b0c48)=0x6000;
        const uint32_t actor=0x50000+4*0xea0,victim=0x78000,source=0x77000,hit=0x79000;
        WORD_AT(actor)=victim;WORD_AT(actor+8)=4;WORD_AT(victim+0x7c)=actor;
        WORD_AT(victim+0x150)=0;WORD_AT(victim+0x230)=81;
        WORD_AT(source+0x150)=0x6000;WORD_AT(0x6000)=0;WORD_AT(0x6014)=source;
        WORD_AT(hit)=victim;WORD_AT(hit+4)=source;
        const float health=10.f,amount=2.5f;
        memcpy((uint8_t *)memory+victim+0x1cc,&health,4);memcpy((uint8_t *)memory+hit+0x20,&amount,4);
        g_esp=0x1800;WORD_AT(g_esp+4)=hit;load_probe_enabled=1;
        CHECK(pc_story_network_hit_trace(hit_trace_original));
        float after;memcpy(&after,(uint8_t *)memory+victim+0x1cc,4);
        CHECK(hit_original_calls==1 && g_esp==0x1804 && g_eax==23 && after==7.5f);
        g_esp=0x1800;load_probe_enabled=0;CHECK(!pc_story_network_hit_trace(hit_trace_original));
        CHECK(hit_original_calls==1 && g_esp==0x1800);
        load_probe_enabled=1;WORD_AT(actor)=0;
        CHECK(!pc_story_network_hit_trace(hit_trace_original) && hit_original_calls==1);
        WORD_AT(actor)=victim;online_mode=PC_ONLINE_ARCADE;
        CHECK(!pc_story_network_hit_trace(hit_trace_original) && hit_original_calls==1);
        load_probe_enabled=0;
    }
#endif
    puts("PASS: Story host controls, network-mode preservation and difficulty return");
    return 0;
}
