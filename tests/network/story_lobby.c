/* Production host controls against isolated guest RAM and original spies. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pc_online.h"
#include "pc_character_pose.h"
#include "pc_network_bots_state.h"
#ifdef TSFP_NATIVE_DISPLAY
#undef _WIN32
#elif !defined(_WIN32)
#define _WIN32 1
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#ifdef _WIN32
static unsigned bomb_shakes;
static uint32_t memory[0x800000/4],g_esp,g_eax,g_ebx,g_ecx,g_edx,g_esi;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
typedef struct GuestRegisters {uint32_t sp,ax,bx,cx,dx;} GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_esp,g_eax,g_ebx,g_ecx,g_edx};}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->ax;g_ebx=r->bx;g_ecx=r->cx;g_edx=r->dx;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static PcOnlineMode online_mode=PC_ONLINE_STORY;
PcOnlineMode pc_online_mode(void){return online_mode;}
static int allocation_ok=1;
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t align)
{CHECK((n==32 || n==64 || n==96 || n==128 || n==144) && align==4);return allocation_ok?(n==32?0x140000:n<=96?0x8000:n==128?0x8200:0x8100):0;}
static uint32_t next_parent,next_menu,ticks,selectors;
static uint32_t header_mission,header_fallback;
void sub_00197890(void){++header_fallback;g_esp+=4;}
void sub_002CA9F0(void){CHECK(WORD_AT(WORD_AT(g_esp+4)+4)==0x5238c0);header_mission=WORD_AT(WORD_AT(g_esp+8)+0x14);g_esp+=4;}
static volatile unsigned marker;
static unsigned difficulty_rows,finalized,highlighted;
static unsigned difficulty_draws;
void sub_002CAD80(void)
{CHECK(WORD_AT(0x78ad48)==12 && WORD_AT(0x5236c4)==2);++difficulty_draws;g_esp+=4;}
static unsigned story_factories,story_broadcasts,story_generation;
static int factory_intro;
static int factory_reward_go,reward_wait_build;
static unsigned reward_wait_rows,reward_wait_panels,reward_count,reward_refreshes,reward_records;
static unsigned context_spawns,ready_calls;
void sub_000D2CE0(void){marker=82;}
void sub_0015BCC0_gen(void){marker=83;}
static unsigned retired_bodies;
static unsigned pad_fallbacks;
void sub_000CF060_gen(void){marker=16;}
void sub_0009A420_gen(void){marker=27;}
void sub_0025EE60(void){marker=31;}
static unsigned story_checkpoint;
static unsigned objective_popups;
void sub_00227430(void){marker=32;}
static unsigned restart_calls,restart_rows;
static unsigned checkpoint_captures,checkpoint_extras,checkpoint_restores,checkpoint_extra_restores;
static unsigned checkpoint_inventory_restores;
static int checkpoint_final_restore;
void pc_story_network_checkpoint_capture(void (*original)(void));
static void checkpoint_capture_original(void)
{
    const uint32_t bit=1u<<WORD_AT(g_esp+4),bookmarks=WORD_AT(0x75f5d0);
    WORD_AT(bookmarks+4)|=bit;WORD_AT(bookmarks+0x238+4)|=bit;
    ++checkpoint_captures;g_esp+=4;
}
void sub_0025EF80(void){pc_story_network_checkpoint_capture(checkpoint_capture_original);}
void sub_0025EAD0(void){marker=33;}void sub_0025EC70(void){marker=34;}
void sub_000FFDA0(void){marker=35;}void sub_000FFE40(void){marker=36;}
void sub_000D1EC0(void){marker=137;}
void sub_000EFBD0(void){marker=37;}void sub_00101660(void){marker=38;}
void sub_000FF420(void){marker=39;}void sub_000FF490(void){marker=40;}
void sub_000FF200(void){marker=41;}
void sub_000C6100(void){marker=42;}void sub_000D4B00(void){marker=43;}
void sub_000D4BE0(void){marker=44;}
void sub_00164C40(void){marker=46;}void sub_001CB260(void){marker=47;}
void sub_000CC170(void){marker=48;}void sub_000C9C90(void){marker=49;}
static unsigned feed_starts,feed_stops;
void sub_001B67E0(void){marker=50;}
void sub_001128C0(void){marker=51;}void sub_00080F20(void){marker=52;}
static unsigned blood_decals;
static unsigned ammo_loaded,world_removed;
static unsigned dialogue_plays,dialogue_lips;
static unsigned cine_begins,cine_ends;
static int story_barrel_scope(void);
static unsigned effect_barrels;
static unsigned effect_emitters,effect_blasts,effect_trails,effect_moves,effect_deletes;
static void checkpoint_restore_original(void)
{CHECK(WORD_AT(0x7b0c7c)==WORD_AT(0x7b0c48));++checkpoint_restores;g_esp+=4;}
static void restart_original(void){++restart_calls;g_eax=73;g_esp+=4;}
void sub_000653B0(void){restart_original();}
void sub_00359EB0(void){CHECK(0);}
void sub_003544B0(void){marker=77;g_esp+=4;}
void sub_003535C0(void){marker=80;g_esp+=4;}
void sub_0003D7B0(void){marker=9;} void sub_00040F00(void){marker=10;}
void sub_00041DC0(void){marker=11;} void sub_0003FA10(void){marker=12;}
void sub_0003F8D0(void){marker=14;} void sub_0003FCF0(void){marker=15;}
void sub_00154FE0(void){marker=13;} void sub_00198E20(void){g_esp+=4;}
static unsigned briefing_draws,status_draws;
void sub_00065940(void){marker=7;}
void sub_002EB0A0(void){marker=61;g_esp+=4;}
void pc_story_network_result_action(void (*original)(void),unsigned action);
void sub_00065470_gen(void){marker=62;g_esp+=4;}
void sub_002EBE60(void){marker=76;g_esp+=4;}
void sub_00065420(void){marker=74;g_esp+=4;}
void sub_001CC360(void){marker=75;g_esp+=4;}
static unsigned leave_disconnects,lobby_return_dispatches,lobby_return_resets;
void pc_online_disconnect(void){++leave_disconnects;online_mode=PC_ONLINE_NONE;}

void sub_002EAF60_gen(void){marker=84;g_esp+=4;}
void sub_002EAFC0_gen(void){pc_story_network_result_action(sub_00065470_gen,0u);}
void sub_002EB030_gen(void){pc_story_network_result_action(sub_00065470_gen,0u);}
void sub_002EC090_gen(void){pc_story_network_result_action(sub_00065470_gen,0u);}
void sub_002EC650_gen(void){pc_story_network_result_action(sub_00065470_gen,0u);}
void sub_00065C70(void){marker=63;g_esp+=4;}
void sub_002EB030(void){marker=45;}
void sub_002CBBA0(void){marker=64;g_esp+=4;}
void pc_story_network_rewards_page(void (*original)(void));
static void reward_page_original(void){marker=65;g_esp+=4;}
static uint32_t exit_factory_target;
static void exit_factory_original(void){
    exit_factory_target=WORD_AT(g_esp+8);
    if(exit_factory_target==0x4e99d4) {CHECK(story_lobby_member());WORD_AT(0x76b1a4)=12;}
    g_esp+=4;
}

void sub_002CBAF0(void){pc_story_network_rewards_page(reward_page_original);}
void sub_002CB550(void){CHECK(0);}
void sub_00078980(void){++reward_refreshes;g_esp+=4;}
void sub_000668F0(void){CHECK(0);}
void sub_00069A60(void){CHECK(0);}
void sub_002CB210(void){CHECK(0);}
void sub_00356870(void){CHECK(0);}
void sub_002CBB60(void){marker=66;g_esp+=4;}
void sub_00352640(void){marker=17;}void sub_00352650(void){marker=18;}void sub_00356490(void){marker=19;}
void sub_000D50E0(void){marker=8;}
void sub_00075B70(void){marker=1;} void sub_00071B10(void){marker=2;}
void sub_00071BE0(void){marker=71;}
void sub_00071C50(void){marker=3;} void sub_00079D60_gen(void){marker=4;}
void sub_000771D0(void){marker=5;} void sub_000770E0(void){marker=6;}
void sub_00079B70(void){CHECK(0);}
void sub_000DAC30(void){marker=20;}void sub_000D7550(void){marker=21;}
void sub_000C1950(void){marker=54;}
void sub_000B3630(void){marker=53;}
void sub_000A9070(void){marker=22;}void sub_0015BCA0(void){marker=23;}
void sub_0015BCC0(void){marker=24;}void sub_002402B0(void){marker=25;}
void sub_00353A90(void){marker=26;}
void sub_001BA2B0(void){marker=28;}void sub_001BFCA0(void){marker=29;}
void sub_001BA130(void){marker=30;}
static unsigned loadout_ammo,loadout_grants,loadout_selections;
static unsigned npc_ctor_calls,npc_refreshes,npc_removed,npc_sent_size,npc_send_calls,npc_weapons_removed;
static unsigned char npc_sent[512],npc_sent_base[256],npc_sent_pose[256];
static unsigned npc_base_size,npc_pose_size;
static PcCharacterPose npc_last_pose;
static unsigned npc_pose_calls,npc_corpse_copies;
static unsigned story_shot_replays;
void pc_network_bots_present_shot(uint32_t entity,const PcNetworkBotShot *shot)
{CHECK(entity==0x72000 && (shot->serial==1 || (shot->serial==2 && shot->weapon==61)));++story_shot_replays;}
static uint32_t npc_constructor_position[4];
static int32_t npc_constructor_variant;
static unsigned story_hit_requests;
void pc_network_bots_request_hit(uint32_t context){CHECK(context==0x79000);++story_hit_requests;}
void pc_character_pose_present(uint32_t entity,const PcCharacterPose *pose)
{CHECK(entity==0x72000 || entity==0x74400);npc_last_pose=*pose;++npc_pose_calls;}
static uint32_t npc_primary_room;
void pc_network_bots_entity_room(uint32_t entity,uint32_t room)
{CHECK(entity==0x72000);npc_primary_room=room;WORD_AT(entity+0x34)=room;}
static float npc_refresh_center,npc_refresh_radius;
void pc_network_bots_entity_refresh(uint32_t entity)
{
    CHECK(entity==0x72000);++npc_refreshes;
    memcpy(&npc_refresh_center,&WORD_AT(entity+0x1a8),4);
    memcpy(&npc_refresh_radius,&WORD_AT(entity+0x1c0),4);
}
#include "pc_physical_state.h"
void pc_rigid_body_capture(PcRigidBodyState *s,uint32_t e){(void)e;memset(s,0,sizeof *s);}
void pc_rigid_body_remove(uint32_t e){(void)e;}
int pc_rigid_body_present(uint32_t e,const PcRigidBodyState *s){(void)e;(void)s;return 1;}
void pc_character_ragdoll_capture(PcRagdollState *s,uint32_t e){(void)e;memset(s,0,sizeof *s);}
static unsigned npc_rag_applies;
void pc_character_ragdoll_present(uint32_t e,const PcRagdollState *s){(void)e;if(s->count)npc_rag_applies++;}
void sub_0034FF30(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    uint32_t args[10];va_list ap;va_start(ap,count);CHECK(count<=10);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);
    va_end(ap);
    if(fn==sub_0015BCC0_gen) {CHECK(count==1 && args[0]==0x90000);++retired_bodies;return 0;}
    if(fn==sub_000D2CE0) {CHECK(count==2 && args[1]==16);WORD_AT(args[0]+0x114)=16;return 0;}
    if(fn==sub_002EC650_gen) {CHECK(!count);uint32_t sp=g_esp;sub_002EC650_gen();g_esp=sp;return 0;}
    if(fn==sub_003535C0) {CHECK(!count);++lobby_return_resets;return 0;}
    if(fn==sub_003544B0) {CHECK(!count);++lobby_return_dispatches;WORD_AT(0x76b1a4)=WORD_AT(0x76b140)==1?11:12;return 0;}
    if(fn==sub_00065470_gen || fn==sub_00065420 || fn==sub_001CC360) {
        CHECK(!count);marker=fn==sub_00065470_gen?62:fn==sub_00065420?74:75;return 0;
    }
    if(fn==sub_00069A60) {CHECK(count==1 && args[0]==1);return 0;}
    if(fn==sub_002CB210) {CHECK(!count);return 0;}
    if(fn==sub_002CB550) {CHECK(!count);return reward_count;}
    if(fn==sub_00356870) {CHECK(count==1 && !args[0]);return 1;}
    if(fn==sub_000668F0) {CHECK(!count && (WORD_AT(0x7de45c)&0x8000));++reward_records;return 0;}
    if(fn==sub_00080F20) {CHECK(count==1 && args[0]==957);return 777;}
    if(fn==sub_001128C0) {
        CHECK(count==9 && args[0]==777 && args[1]==1 && args[2]==0x7f1e1e32 && args[3]==4 &&
              args[4]==0x3f800000 && args[5]==1 && args[6]==3);
        CHECK(WORD_AT(args[7])==0x3f800000 && WORD_AT(args[8]+4)==0xbf800000);
        ++blood_decals;return 1;
    }
    if(fn==sub_001B67E0) {CHECK(count==2 && args[0]==0x50000 && args[1]==3);return ammo_loaded;}
    if(fn==sub_000A9070 && args[0]>=0x210000) {CHECK(count==1);WORD_AT(args[0]+0x28)|=0x80000000u;++world_removed;return 0;}
    if(fn==sub_000CC170) {CHECK(count==3 && args[0]==0x180000 && args[1]==0x40a00000 && args[2]==0x74400);++feed_starts;return 0;}
    if(fn==sub_000C9C90) {CHECK(count==0);++feed_stops;return 0;}
    if(fn==sub_00164C40) {CHECK(count==4 && args[1]==0x1234 && args[2]==0x4000 && !args[3]);++dialogue_plays;return 0;}
    if(fn==sub_001CB260) {CHECK(count==1);++dialogue_lips;return 0;}
    if(fn==sub_000C6100) {CHECK(count==1);return args[0]==0x4352434d?0x180000:0;}
    if(fn==sub_000D4B00 || fn==sub_000D4BE0) {
        const uint32_t context=0x6000+0x1584;
        CHECK(WORD_AT(0x7b0c48)==context && WORD_AT(0x790950)==1 && WORD_AT(0x7b0c7c)==context && args[0]==context);
        if(fn==sub_000D4B00) {
            CHECK(count==6 && args[1]==0x180000 && !args[2] && args[3]==4 && args[4]==11 && !args[5]);
            ++cine_begins;WORD_AT(context+0x114)=256;
        } else {CHECK(count==1 && !WORD_AT(context+0x1580));++cine_ends;WORD_AT(context+0x114)=2;}
        return 0;
    }
    if(fn==sub_000653B0) {CHECK(count==1);++restart_calls;return 0;}
    if(fn==sub_00227430) {CHECK(count==2 && args[1]==0);++objective_popups;return 0;}
    if(fn==sub_000FFDA0) {
        CHECK(count==6 && args[0]==0 && args[1]==2 && args[5]==65535);
        CHECK(WORD_AT(args[2])==WORD_AT(0x170000) && WORD_AT(args[3])==WORD_AT(0x170010));
        ++effect_emitters;return 0x150000;
    }
    if(fn==sub_000FFE40) {CHECK(count==2 && args[0]==0x150000);WORD_AT(0x150098)=args[1];return 0;}
    if(fn==sub_000D1EC0) {
        CHECK(count==4 && args[0]==0x6000+0x1584 && args[2]==0x3f6aaaabu);
        float amplitude,decay;memcpy(&amplitude,&args[1],4);memcpy(&decay,&args[3],4);
        CHECK(fabsf(amplitude-.45833334f)<.00001f && fabsf(decay-.18333334f)<.00001f);
        ++bomb_shakes;return 0;
    }
    if(fn==sub_000EFBD0) {
        if(args[0]==1 || args[0]==40 || args[0]==47) {
            CHECK(count==8 && args[1]==UINT32_MAX && !args[4] && !args[5] && !args[6] && args[7]==1);
            if(args[0]==1) CHECK(!args[3]);
            else CHECK(args[3] && WORD_AT(args[3])==0x3f800000u);
            CHECK(story_barrel_scope());++effect_barrels;return 0;
        }
        CHECK(count==8 && args[0]==39 && args[1]==UINT32_MAX && !args[4] && !args[5] && !args[6] && args[7]==1);
        ++effect_blasts;return 0;
    }
    if(fn==sub_00101660) {CHECK(count==5 && args[1]==0x7e && !args[2] && args[4]==65535);++effect_trails;return 0x160000;}
    if(fn==sub_000FF420) {CHECK(count==2 && args[0]==0x160000 && args[1]==0x42200000);return 0;}
    if(fn==sub_000FF490) {CHECK(count==2 && args[0]==0x160000);++effect_moves;return 0;}
    if(fn==sub_000FF200) {CHECK(count==1 && args[0]==0x160000);++effect_deletes;return 0;}
    if(fn==sub_0025EF80) {
        CHECK(count==1);const uint32_t before=g_esp;g_esp-=16;
        WORD_AT(g_esp)=0x1234;WORD_AT(g_esp+4)=args[0];sub_0025EF80();g_esp=before;return 0;
    }
    if(fn==sub_0025EAD0) {
        CHECK(count==1 && g_edx==(1u<<g_ecx));
        const unsigned slot=(g_ebx-WORD_AT(0x75f5d0))/0x238;
        CHECK(slot>=2 && slot<4 && args[0]==WORD_AT(0x7b0c48)+slot*0x1584);
        WORD_AT(g_ebx)=g_ecx;WORD_AT(g_ebx+4)|=g_edx;++checkpoint_extras;return 0;
    }
    if(fn==sub_0025EC70) {
        CHECK(count==0);const unsigned slot=(g_ebx-WORD_AT(0x75f5d0))/0x238;
        CHECK(slot<4 && g_eax==WORD_AT(0x7b0c48)+slot*0x1584);
        if(checkpoint_final_restore) {
            WORD_AT(WORD_AT(WORD_AT(g_eax+0x14)+0x7c)+0x54c+25)=1;
            ++checkpoint_inventory_restores;
        } else {CHECK(slot>=2);++checkpoint_extra_restores;}
        return 0;
    }
    if(fn==sub_001BA2B0) {
        CHECK(count==3 && ((args[1]==25 && args[2]==8) || (args[1]==32 && args[2]==6)));loadout_ammo+=args[2];return 8;
    }
    if(fn==sub_001BFCA0) {
        CHECK(count==2 && (args[1]==25 || args[1]==32));((uint8_t *)memory)[args[0]+0x54c+args[1]]=1;
        ++loadout_grants;return 0;
    }
    if(fn==sub_001BA130) {CHECK(count==2 && (args[1]==25 || args[1]==32));++loadout_selections;return 0;}
    if(fn==sub_00353A90) {
        CHECK(count==5 && args[0]==0x30000 && args[1]==0x1e && args[3]<=256u);
        CHECK(args[4]==1 || WORD_AT(args[2]+4)==0x37504e53u || WORD_AT(args[2]+4)==0x37505053u);
        ++npc_send_calls;npc_sent_size=args[3];memcpy(npc_sent,(uint8_t *)memory+args[2],args[3]);
        if(WORD_AT(args[2]+4)==0x37504e53u){npc_base_size=args[3];memcpy(npc_sent_base,npc_sent,args[3]);}
        if(WORD_AT(args[2]+4)==0x37505053u){npc_pose_size=args[3];memcpy(npc_sent_pose,npc_sent,args[3]);}
        return 0;
    }
    if(fn==sub_000B3630) {
        CHECK(count==1 && args[0]==0x72000 && npc_weapons_removed==2);
        const uint32_t actor=WORD_AT(0x7207c);
        CHECK(!WORD_AT(actor+0x74) && !WORD_AT(actor+0x10) && !WORD_AT(actor+0x14));
        CHECK(guest_writable(WORD_AT(actor+0xbf0),0x60));
        WORD_AT(0x75020)=0x1000;++npc_corpse_copies;return 0x75000;
    }
    if(fn==sub_000DAC30) {
        CHECK(count==10 && args[0]==12 && args[1]==12 && args[2]==2);
        memcpy(npc_constructor_position,args+3,sizeof npc_constructor_position);
        npc_constructor_variant=(int32_t)args[9];
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
    if(fn==sub_000A9070){CHECK(count==1);
        if(args[0]==0x73000 || args[0]==0x73400) {++npc_weapons_removed;WORD_AT(args[0]+0x28)|=0x80000000u;}
        else ++npc_removed;
        return 0;}
    if(fn==sub_0025EE60){CHECK(count==1 && args[0]==0);return story_checkpoint;}
    if(fn==sub_0009A420_gen){CHECK(count==1 && args[0]==0x1f40);return 0x78000;}
    if(fn==sub_000CF060_gen){CHECK(count==1 && WORD_AT(args[0])==0);++pad_fallbacks;return 77;}
    else if(fn==sub_0003D7B0 && count==1 && args[0]==0x17a8){return 0x33000;}
    else if(fn==sub_0003D7B0 && count==1 && args[0]==0x17a7){return 0x33020;}
    else if(fn==sub_0003D7B0){
        if(count==1 && args[0]>=0x3d5 && args[0]<=0x3d7) return 0x8100+48+(args[0]-0x3d5)*32;
        CHECK(count==1 && (args[0]==22 || args[0]==33));return args[0]==33?0xd000:0xb000;
    }
    else if(fn==sub_00040F00){CHECK(count==2 && args[0]==0xb000);return 0xc000;}
    else if(fn==sub_0003F8D0){CHECK(count==0);}
    else if(fn==sub_0003FCF0){CHECK(count==4);float height=10.f,width=100.f;memcpy(&WORD_AT(args[1]),&height,4);memcpy(&WORD_AT(args[0]),&width,4);}
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
        if(factory_reward_go) {
            CHECK(count==1 && args[0]==0x78ad58 && WORD_AT(0x78ad48)==0 && WORD_AT(0x78ad50)==3 && WORD_AT(0x78ad54)==1);
            memset((uint8_t *)memory+0x78ad58,0,0xa0);
            ((uint8_t *)memory)[0x78ad58]=1;((uint8_t *)memory)[0x78ad59]=1;
            ((uint8_t *)memory)[0x78adac]=3;((uint8_t *)memory)[0x78adad]=10;((uint8_t *)memory)[0x78adae]=2;
            WORD_AT(0x78adb0)=0x80008;WORD_AT(0x78ad48)=1;WORD_AT(0x78ad50)=1;
            ++story_factories;return 0;
        }
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
    else if(fn==sub_00075B70){
        if(reward_wait_build){CHECK(count==5 && !args[3] && !args[4]);CHECK(!strcmp((char *)(g_xbox_mem_offset+args[1]),"Mission complete"));++reward_wait_panels;return 0;}
        CHECK(count==5 && args[4]==0x1237e);difficulty_rows=0;}
    else if(fn==sub_00079D60_gen){
        if(reward_wait_build){CHECK(count==6 && args[2]==1 && !args[4] && !args[5]);CHECK(!strcmp((char *)(g_xbox_mem_offset+args[0]),"Waiting for host to start the next mission."));++reward_wait_rows;return 0;}
        if(count==6 && args[4]==0x2ebc30){++restart_rows;return 0;}
        if(count==6 && args[4]==0x2ebd50){CHECK(args[2]<=1u);return 0;}
        static const char *names[]={"Easy","Normal","Hard"};
        CHECK(count==6 && difficulty_rows<3 && args[1]==difficulty_rows && args[4]==0x1237d && args[5]==0x12020);
        CHECK(!strcmp((char *)(g_xbox_mem_offset+args[0]),names[difficulty_rows]));++difficulty_rows;
    }else if(fn==sub_000771D0){CHECK(count==1);++finalized;}
    else if(fn==sub_000770E0){CHECK(count==2);highlighted=args[1];}
    else if(fn==sub_00071BE0) CHECK(count==3 && args[0]==1 && args[1]==0x41100000u && args[2]==0);
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
#ifdef _WIN32
int pc_network_bots_preserve_corpse(uint32_t body,uint32_t spawn){(void)body;(void)spawn;return 0;}
#endif
#include "pc_story_lobby.inc"
#ifdef _WIN32
#define getenv load_getenv
#endif
#ifdef _WIN32
static int story_online_return_menu(void (*original)(void)){(void)original;return 0;}
#endif
#ifdef _WIN32
void pc_character_ragdoll_preserve(uint32_t e,uint32_t c){(void)e;(void)c;}
#include "pc_character_corpses.inc"
#endif
#include "pc_story_load.inc"
#ifdef _WIN32
static unsigned reward_rows;
static void reward_row_original(void)
{CHECK(WORD_AT(g_esp+12)==0x21 && WORD_AT(g_esp+20)==0x2eb0a0);++reward_rows;g_esp+=4;}
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
static void team_init_original(void)
{const uint32_t actor=WORD_AT(g_esp+4);WORD_AT(actor+0x84)=WORD_AT(actor+8)+1;g_eax=91;g_esp+=4;}
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
static void stage_original(void)
{
    CHECK(WORD_AT(0x78ad50)==2 && WORD_AT(0x78ad48)==0 && WORD_AT(0x7de458)==0x80008);
    CHECK(WORD_AT(0x7de45c)&0x8000);CHECK(WORD_AT(0x79094c)==2 || WORD_AT(0x79094c)==106);
    g_eax=77;g_esp+=4;
}
static void visibility_original(void)
{WORD_AT(0x74010)=0x1234ffffu;WORD_AT(0x74410)=0x5678ffffu;WORD_AT(0x1c0010)=0x170000u;
 WORD_AT(0x7b4cc0)=0x74000;WORD_AT(0x7b54d8)=4;
 WORD_AT(0x74000)=0x74400;WORD_AT(0x74400)=0x1d0000;WORD_AT(0x1d0000)=0x1e0000;WORD_AT(0x1e0000)=0;
 WORD_AT(0x1d0010)=0x1234ffffu;WORD_AT(0x1d000c)=0xffffu;g_esp+=4;}
static void result_menu_original(void)
{CHECK(WORD_AT(0x7ba9e0)==0 && WORD_AT(0x7b0c7c)==WORD_AT(0x7b0c48));g_eax=77;g_esp+=4;}
static void world_remove_original(void)
{WORD_AT(WORD_AT(g_esp+4)+0x28)|=0x80000000u;g_eax=37;g_esp+=4;}
static void blood_original(void){g_eax=1;g_esp+=4;}
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
static int story_barrel_scope(void){return story_effect_barrel_replay!=0;}
static unsigned effect_decisions;
static void effect_factory_original(void){g_eax=0x150000;WORD_AT(0x150098)=0;g_esp+=4;}
static void effect_decision_original(void)
{
    const uint32_t outer=g_esp;++effect_decisions;g_esp=0x2000;
    WORD_AT(g_esp+4)=0;WORD_AT(g_esp+8)=2;WORD_AT(g_esp+12)=0x170000;
    WORD_AT(g_esp+16)=0x170010;WORD_AT(g_esp+20)=0;WORD_AT(g_esp+24)=65535;
    pc_story_network_effect_create(effect_factory_original,0);
    WORD_AT(0x150098)=0x7f123456;g_esp=outer+4;g_eax=99;
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
    CHECK(!story_lobby_lookup(0x2cad80)); /* The trophy drawer is not a Select action. */
    call(STORY_BUILD_CB);CHECK(WORD_AT(0x78ad2c)==13 && selectors==1);
    CHECK(difficulty_rows==3 && finalized==1 && highlighted==0);
    WORD_AT(0x5236c4)=1;const uint32_t original_mission=WORD_AT(0x78ad48);
    call(STORY_BODY_CB);CHECK(difficulty_draws==1 && story_lobby_options==2 && WORD_AT(0x5236c4)==1 && WORD_AT(0x78ad48)==original_mission);
    call(STORY_BACK_CB);CHECK(story_lobby_options==1 && next_menu==0x5238e0);
    CHECK(story_lobby_mission==12 && WORD_AT(0x78ad48)==0xaabbccdd);
    WORD_AT(0x2004)=0x5238e0;call(STORY_SELECT_CB);
    CHECK(story_lobby_options==2 && next_menu==story_difficulty_descriptor);
    WORD_AT(0x2004)=story_difficulty_descriptor;
    WORD_AT(0x78ad54)=0x12345600;
    WORD_AT(0x218c)=3;call(STORY_SELECT_CB);CHECK(story_lobby_options==2);
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
        /* Outgoing phase10 still renders Story, but owns no lobby controls.
           A collapsed transition rectangle must never call Arcade layout. */
        story_lobby_difficulty=2;story_lobby_mission=1;
        WORD_AT(0x79094c)=101;WORD_AT(0x76b1a4)=10;
        for(unsigned role=1;role<=2;role++) {
            WORD_AT(0x76b140)=role;
            CHECK(story_lobby_rendering() && !story_lobby_member());
            g_esp=0x1800;WORD_AT(g_esp+4)=0x4000;
            WORD_AT(0x4004)=role==1?0x4e99c4:0x4e99d4;
            CHECK(story_lobby_lookup(STORY_BUILD_CB)==story_lobby_header);
            CHECK(story_lobby_lookup(STORY_BODY_CB)==story_lobby_body);
            CHECK(!story_lobby_lookup(0x1a8d30) && !story_lobby_lookup(0x1a8f30));
            WORD_AT(g_esp)=0x199650;pc_story_lobby_details(frame_original);
            CHECK(g_esp==0x1804 && failure_original_calls==old_calls+1);
        }
        const float collapsed[4]={100,100,110,110};
        memcpy((uint8_t *)memory+0x7000,collapsed,sizeof collapsed);
        g_esp=0x1800;WORD_AT(g_esp)=0x199650;
        pc_story_lobby_details(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==old_calls+1);
        online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;pc_story_lobby_details(frame_original);
        CHECK(failure_original_calls==old_calls+2);
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b1a4)=12;
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
        for(unsigned slot=0;slot<2;slot++) {
            CHECK(WORD_AT(0x777288+slot*0x124)==1 && ((uint8_t *)memory)[0x7de424+slot]==1);
        }
        const unsigned spawns_before=context_spawns;
        g_esp=0x1800;pc_story_network_frame(frame_original);
        CHECK(context_spawns==spawns_before+2 && WORD_AT(0x6114)==2 && WORD_AT(0x7698)==2);
        WORD_AT(0x6114)=4;g_esp=0x1800;pc_story_network_frame(frame_original);
        CHECK(context_spawns==spawns_before+2); /* initial load only */
    }
    {
        /* Every remote body starts beside the primary hanging pad, with
           distinct16bit tokens and no change to checkpoint selection. */
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
        for(unsigned slot=1;slot<4;slot++) {
            g_esp=0x1800;WORD_AT(g_esp+4)=0x7f00+slot;
            pc_story_network_start_record(frame_original);
            const uint32_t target=0xa000+(slot-1)*32;
            float p[3];memcpy(p,(uint8_t *)memory+target+12,12);
            CHECK(g_eax==target && g_esp==0x1804 && p[0]==(slot==2?8.f:slot==1?12.f:14.f) &&
                  p[1]==20.f && p[2]==30.f && WORD_AT(target+4)==0x5a5a5a5a);
        }
        story_rejoin_pose_valid=1;story_rejoin_position[0]=100.f;
        story_rejoin_position[1]=200.f;story_rejoin_position[2]=300.f;story_rejoin_heading=1.5f;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x7f01;pc_story_network_start_record(frame_original);
        float joined_pad[4];memcpy(joined_pad,(uint8_t *)memory+0xa00c,sizeof joined_pad);
        CHECK(joined_pad[0]==102.f && joined_pad[1]==200.f && joined_pad[2]==300.f && joined_pad[3]==1.5f);
        story_rejoin_pose_valid=0;
        float original_x;memcpy(&original_x,(uint8_t *)memory+0x7800c,4);CHECK(original_x==10.f);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x7f03;
        pc_story_network_start_tag(start_tag_original);
        CHECK(g_eax==1001 && g_esp==0x1804 && WORD_AT(0x1804)==0x7f03);
        g_esp=0x1800;WORD_AT(g_esp+4)=0xa040;
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
            CHECK(g_eax==(slot?0x7f01u:0x1f40u));
        }
        story_checkpoint=1;g_esp=0x1800;WORD_AT(g_esp+4)=extra;
        pc_story_network_spawn_pad(pad_original);CHECK(g_eax==88); /* checkpoint owns selection */
        const unsigned checkpoint_fallbacks=pad_fallbacks;
        pad_original_result=UINT32_MAX;g_esp=0x1800;pc_story_network_spawn_pad(pad_original);
        CHECK(g_eax==77 && pad_fallbacks==checkpoint_fallbacks+1 && WORD_AT(extra)==2);
        pad_original_result=88;story_checkpoint=0;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x7f02;pc_story_network_start_record(frame_original);
        CHECK(g_eax==0xa020);
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
        WORD_AT(0x70034)=5;
        const unsigned wire_before=npc_send_calls;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x70000;pc_story_network_npc_spawn(npc_spawn_original);
        CHECK(g_esp==0x1804 && g_eax==0x55 && npc_send_calls==wire_before+2);
        CHECK(npc_base_size==sizeof(StoryNpcBaseMessage)+4 && npc_pose_size==sizeof(StoryNpcPoseMessage)+4);
        StoryNpcSpawn packet=npc_captured_spawn();
        CHECK(packet.primary_room==5);
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
        story_npc_pending_frame();CHECK(npc_send_calls==wire_before+4);
        WORD_AT(0x7de318)=11;story_npc_pending_frame();CHECK(npc_send_calls==wire_before+6);
        WORD_AT(0x74c37c)=2;WORD_AT(0x7de318)=16;story_npc_pending_frame();state=npc_captured_spawn();
        CHECK(npc_send_calls==wire_before+8 && state.present && !state.in_roster);
        WORD_AT(actor)=0;WORD_AT(0x7de318)=22;story_npc_pending_frame();state=npc_captured_spawn();
        CHECK(npc_send_calls==wire_before+9 && !state.present && !state.pose_present);
        WORD_AT(0x76b1a4)=10;WORD_AT(0x7de318)=28;story_npc_pending_frame();
        CHECK(npc_send_calls==wire_before+9);WORD_AT(0x76b1a4)=15;
        WORD_AT(actor)=0;WORD_AT(0x74c37c)=2;WORD_AT(0x7a3580)=0x50000;WORD_AT(0x7a3584)=0x50ea0;
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=0;WORD_AT(0x31000)=0;
        npc_deliver(&packet,2);CHECK(npc_ctor_calls==0 && !story_npc_fresh[2]);
        npc_deliver(&packet,1);
        CHECK(g_esp==0x1804 && g_eax==0 && npc_ctor_calls==1 && npc_refreshes==1);
        CHECK(!memcmp(npc_constructor_position,packet.position,sizeof packet.position) &&
              !memcmp(npc_constructor_position+3,&packet.heading,sizeof packet.heading) &&
              npc_constructor_variant==packet.variant && packet.variant>=0);
        CHECK(WORD_AT(actor)==0x72000 && WORD_AT(0x72230)==59 && WORD_AT(0x72258)==7 &&
              WORD_AT(actor+0x84)==5 && WORD_AT(0x74c37c)==3);
        float health,pos;memcpy(&health,&WORD_AT(0x721cc),4);memcpy(&pos,&WORD_AT(0x72064),4);
        CHECK(health==20.f && pos==3.f && npc_primary_room==5 && WORD_AT(0x72034)==5 && npc_pose_calls==1 && npc_last_pose.clip==42);
        CHECK(pc_story_network_replica(0x72000));WORD_AT(0x721a8)=0;
        /* Frozen replicas follow remote-human presentation eligibility across
           room boundaries, before the engine allocates current-frame bones. */
        WORD_AT(0x7b0ce0)=1;WORD_AT(0x7200c)=0x80000000;WORD_AT(0x72010)=0x40000000;
        story_npc_visibility_prepare();CHECK(WORD_AT(0x7200c)==0x80000001 && WORD_AT(0x72010)==0x40000001);
        WORD_AT(0x72258)=8;WORD_AT(0x7200c)=0;story_npc_visibility_prepare();CHECK(!WORD_AT(0x7200c));
        WORD_AT(0x72258)=7;WORD_AT(0x76b140)=1;story_npc_visibility_prepare();CHECK(!WORD_AT(0x7200c));
        WORD_AT(0x76b140)=2;
        /* Unactivated guest-only scene placeholders must not collide or take
           hits. Keep their object/model so later host activation can reuse it. */
        const uint32_t dormant=0x50000+3*0xea0;
        WORD_AT(dormant)=0x74800;WORD_AT(0x7487c)=dormant;
        WORD_AT(0x74820)=8;WORD_AT(0x74828)=0x40100078;
        story_npc_pending_frame();CHECK(WORD_AT(0x74828)==0x40100008 && WORD_AT(dormant)==0x74800);
        WORD_AT(0x74828)=0x40100079;story_npc_pending_frame();CHECK(WORD_AT(0x74828)==0x40100079);
        WORD_AT(dormant)=0;
        CHECK(pc_story_network_replica_bounds(0x72000));float bound;memcpy(&bound,&WORD_AT(0x721a8),4);CHECK(bound==.25f);
        WORD_AT(0x72258)=8;CHECK(!pc_story_network_replica(0x72000));WORD_AT(0x72258)=7;
        WORD_AT(0x72150)=0x6000;CHECK(!pc_story_network_replica(0x72000));WORD_AT(0x72150)=0;
        npc_deliver(&packet,3);CHECK(npc_ctor_calls==1 && npc_refreshes==1 && WORD_AT(0x74c37c)==3);
        StoryNpcSpawn bad=packet;bad.serial=2;bad.primary_room=214;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        bad=packet;bad.serial=2;bad.health=NAN;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        bad=packet;bad.serial=2;bad.pose.clip_channels[0]=NAN;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        bad=packet;bad.serial=2;bad.bounds_radius[0]=-1.f;npc_deliver(&bad,3);CHECK(npc_refreshes==1);
        WORD_AT(0x31000)=1;npc_deliver(&packet,3);CHECK(npc_refreshes==1);WORD_AT(0x31000)=0;
        StoryNpcSpawn queued=packet;queued.serial=2;queued.position[0]=7.f;
        queued.bounds_center[0]=1.25f;queued.bounds_radius[0]=2.5f;
        WORD_AT(0x76b1a4)=10;npc_deliver(&queued,3);CHECK(story_npc_fresh[2] && npc_refreshes==1);
        WORD_AT(0x76b1a4)=15;story_npc_pending_frame();memcpy(&pos,&WORD_AT(0x72064),4);
        CHECK(!story_npc_fresh[2] && npc_refreshes==2 && pos==7.f);
        CHECK(npc_refresh_center==1.25f && npc_refresh_radius==2.5f);
        npc_deliver(&packet,3);CHECK(npc_refreshes==2);
        StoryNpcSpawn other=queued;queued.serial=3;other.serial=3;other.position[0]=11.f;
        npc_deliver(&queued,1);npc_deliver(&other,2);CHECK(npc_refreshes==2); /* no mixed snapshots */
        npc_deliver(&other,1);CHECK(npc_refreshes==3);
        /* Reusing the same actor kind for a new host body must discard stale
           model/room state. A serial-only refresh of the same body must not. */
        other.serial=4;other.spawn=8;npc_deliver(&other,3);
        CHECK(npc_ctor_calls==2 && npc_removed==1 && npc_refreshes==4 && WORD_AT(0x72258)==8);
        npc_deliver(&packet,3);CHECK(npc_ctor_calls==2 && WORD_AT(0x72258)==8);
        queued=other;queued.serial=5;queued.health=0;queued.in_roster=0;
        npc_deliver(&queued,3);CHECK(WORD_AT(actor)==0x72000 && WORD_AT(0x74c37c)==2 && npc_removed==1);
        CHECK(pc_story_network_replica(0x72000));npc_deliver(&queued,3);CHECK(npc_removed==1);
        /* Roster removal is not world removal: retain the host's corpse until
           its explicit absence, even across subsequent dead-body snapshots. */
        queued.serial=6;npc_deliver(&queued,3);CHECK(WORD_AT(actor)==0x72000 && npc_ctor_calls==2 && npc_removed==1);
        for(unsigned hand=0;hand<2;hand++) {
            const uint32_t record=actor+0xa4+hand*0x130,model=0x73000+hand*0x400;
            WORD_AT(record+0xe8)=model;WORD_AT(record+0xec)=actor;
            WORD_AT(model+0x7c)=record;WORD_AT(model+0x28)=1;
        }
        WORD_AT(0x732a10)=8;WORD_AT(0x732d18)=0;
        WORD_AT(0x72020)=8;WORD_AT(0x72004)=0x76000;WORD_AT(0x760d4)=74;
        WORD_AT(actor+0xbf0)=0;WORD_AT(actor+0x74)=0x12345;
        queued.serial=7;queued.present=0;queued.pose_present=0;memset(&queued.pose,0,sizeof queued.pose);
        npc_deliver(&queued,3);CHECK(!WORD_AT(actor) && npc_removed==2 && !pc_story_network_replica(0x72000));
        CHECK(npc_weapons_removed==2 && !WORD_AT(actor+0x18c) && !WORD_AT(actor+0x2bc));
        CHECK(npc_corpse_copies==1 && WORD_AT(0x75020)==0x1000);
        CHECK(!WORD_AT(actor+0xbf0) && WORD_AT(actor+0x74)==0x12345);
        npc_deliver(&queued,3);CHECK(npc_corpse_copies==1); /* reliable replay is idempotent */
        WORD_AT(0x732a10)=0;
        /* Two UDP ticks interleave: neither half may erase the other tick.
           A still newer incomplete pose must not discard queued complete state. */
        StoryNpcSpawn interleaved=other;interleaved.serial=8;
        StoryNpcSpawn newer=interleaved;newer.serial=9;newer.position[0]=19.f;
        npc_deliver(&interleaved,1);npc_deliver(&newer,1);
        npc_deliver(&interleaved,2);CHECK(story_npc_seen_serial[2]==8);
        npc_deliver(&newer,2);CHECK(story_npc_seen_serial[2]==9);
        WORD_AT(0x76b1a4)=10;interleaved.serial=10;npc_deliver(&interleaved,3);
        newer.serial=11;npc_deliver(&newer,2);
        CHECK(story_npc_fresh[2] && story_npc_pending[2].serial==10);
        WORD_AT(0x76b1a4)=15;story_npc_pending_frame();CHECK(story_npc_seen_serial[2]==10);
        npc_deliver(&newer,1);CHECK(story_npc_seen_serial[2]==11);
        newer.camo.flags=0x01000010;newer.camo.fade_phase=1;
        newer.camo.fade_values[2]=.5f;newer.camo.cycle_phase=1;
        newer.camo.cycle_values[0]=4.f;newer.camo.cycle_values[1]=.25f;
        newer.serial=12;newer.cloak_flags=0x200000;newer.cloak_phase=2;
        newer.cloak_values[0]=.25f;newer.cloak_values[1]=.75f;newer.cloak_values[2]=2.f;
        WORD_AT(actor+0xc6c)=0x100;npc_deliver(&newer,3);
        CHECK(WORD_AT(actor+0xc6c)==0x01200110 && WORD_AT(actor+0xde4)==2 && WORD_AT(actor+0xde8)==0x3e800000);
        CHECK(WORD_AT(actor+0xc6c)==0x01200110 && WORD_AT(actor+0xc88)==1);
        CHECK(WORD_AT(actor+0xca8)==1 && WORD_AT(actor+0xcac)==0x40800000);
        newer.serial=13;newer.cloak_flags=0;memset(&newer.camo,0,sizeof newer.camo);
        npc_deliver(&newer,3);CHECK(WORD_AT(actor+0xc6c)==0x100);
        StoryNpcSpawn ragged=newer;ragged.serial=14;ragged.pose_present=3;ragged.pose.clip=229;
        ragged.health=0;ragged.ragdoll.kind=4;ragged.ragdoll.count=3;
        for(unsigned i=0;i<3;i++) {PcRigidBodyState *r=ragged.ragdoll.parts+i;
            r->present=1;r->rotation[0]=r->rotation[4]=r->rotation[8]=1;r->position[1]=(float)i;}
        npc_deliver(&ragged,3);CHECK(story_npc_seen_serial[2]==13 && !npc_rag_applies);
        for(unsigned pass=0;pass<3;pass++) {
            unsigned i=2-pass;
            StoryNpcRagMessage r={STORY_NPC_RAG_MAGIC,ragged.match,ragged.slot,ragged.serial,story_npc_checksum(&ragged),i,{0}};
            const size_t offset=i*208u,bytes=sizeof ragged.ragdoll-offset<208u?sizeof ragged.ragdoll-offset:208u;
            memcpy(r.bytes,(uint8_t *)&ragged.ragdoll+offset,bytes);
            memcpy((uint8_t *)memory+0x90000,&r,sizeof r);
            story_npc_receive_part(0x90000,sizeof r);
            if(pass<2)CHECK(!npc_rag_applies && story_npc_seen_serial[2]==13);
        }
        CHECK(npc_rag_applies==1 && story_npc_seen_serial[2]==14 && npc_last_pose.clip==229);

        /* A mesh loaded by this scene needs no earlier guest NPC instance.
           An unknown/unloaded kind remains queued; a retired recorded body is
           rebuilt without calling a destructor on reused local storage. */
        newer.serial=15;story_npc_kinds[12]=0;WORD_AT(actor)=0;
        WORD_AT(0x53a3d0+12*48)=7;*(int16_t *)((uint8_t *)memory+0x78b860+7*2)=-1;
        npc_deliver(&newer,3);CHECK(story_npc_seen_serial[2]==14 && story_npc_fresh[2]);
        *(int16_t *)((uint8_t *)memory+0x78b860+7*2)=0;WORD_AT(0x791d4c)=0x200000;
        story_npc_pending_frame();CHECK(story_npc_seen_serial[2]==15);
        const unsigned constructors=npc_ctor_calls,removed=npc_removed;
        WORD_AT(actor+0x18c)=0x73000;WORD_AT(actor+0x190)=actor;
        WORD_AT(0x7307c)=actor+0xa4;WORD_AT(0x73028)=1;
        WORD_AT(0x72028)|=0x80000000;newer.serial=16;
        npc_deliver(&newer,3);CHECK(npc_ctor_calls==constructors+1 && npc_removed==removed);
        CHECK(npc_weapons_removed==3 && !WORD_AT(actor+0x18c));
        story_npc_kinds[12]=1;
        /* Speech events survive duplicate/reordered delivery, wait for body
           creation and execute cosmetic APIs without a foreign script handle. */
        story_dialogue_reset();WORD_AT(0x7de30c)=7;
        StoryDialogue speech={STORY_DIALOGUE_MAGIC,story_npc_match(),1,2,8,0x1234,0x4000};
        memcpy((uint8_t *)memory+0x32000,&speech,sizeof speech);
        WORD_AT(0x31000)=1;story_dialogue_receive(0x31000,0x32000,0x1e,sizeof speech);
        CHECK(!story_dialogue_count);WORD_AT(0x31000)=0;
        story_dialogue_receive(0x31000,0x32000,0x1e,sizeof speech);
        story_dialogue_receive(0x31000,0x32000,0x1e,sizeof speech);CHECK(story_dialogue_count==1);
        WORD_AT(actor+0xe90)=0x180000;story_dialogue_frame();CHECK(dialogue_plays==1 && dialogue_lips==1 && !story_dialogue_count);
        speech.serial=3;speech.spawn=9;memcpy((uint8_t *)memory+0x32000,&speech,sizeof speech);
        story_dialogue_receive(0x31000,0x32000,0x1e,sizeof speech);story_dialogue_frame();CHECK(story_dialogue_count==1 && dialogue_plays==1);
        WORD_AT(0x72258)=9;story_dialogue_frame();CHECK(!story_dialogue_count && dialogue_plays==2);
        speech.serial=2;speech.spawn=8;memcpy((uint8_t *)memory+0x32000,&speech,sizeof speech);
        story_dialogue_receive(0x31000,0x32000,0x1e,sizeof speech);WORD_AT(0x7de318)+=300;
        story_dialogue_frame();CHECK(!story_dialogue_count && dialogue_plays==2);
        story_dialogue_reset();WORD_AT(actor+0xe90)=0;
        /* Host NPC shots accept targetless fire; only the host may send NPC
           events, and each event is replayed once for its current body. */
        story_shots_reset();WORD_AT(actor)=0x72000;WORD_AT(0x72258)=8;
        StoryShot shot={0};shot.magic=STORY_SHOT_MAGIC;shot.match=story_npc_match();shot.slot=2;
        shot.shot.serial=1;shot.shot.spawn=8;shot.shot.tick=WORD_AT(0x7de318);
        shot.shot.target_slot=UINT32_MAX;shot.shot.weapon=7;
        memcpy((uint8_t *)memory+0x32000,&shot,sizeof shot);WORD_AT(0x31000)=1;
        CHECK(story_shots_receive(0x31000,0x32000,0x1e,sizeof shot));story_shots_frame();CHECK(!story_shot_replays);
        WORD_AT(0x31000)=0;
        CHECK(story_shots_receive(0x31000,0x32000,0x1e,sizeof shot));story_shots_frame();CHECK(!story_shot_replays);
        WORD_AT(actor+0xd4)=7;WORD_AT(actor+0x18c)=0x180000;
        story_shots_frame();CHECK(story_shot_replays==1);
        CHECK(story_shots_receive(0x31000,0x32000,0x1e,sizeof shot));story_shots_frame();CHECK(story_shot_replays==1);
        shot.shot.serial=2;shot.shot.aim[0]=NAN;memcpy((uint8_t *)memory+0x32000,&shot,sizeof shot);
        story_shots_receive(0x31000,0x32000,0x1e,sizeof shot);CHECK(story_shots_seen[2]==1);
        shot.shot.aim[0]=0;shot.shot.weapon=61;WORD_AT(actor+0xd4)=61;WORD_AT(actor+0x18c)=0;
        memcpy((uint8_t *)memory+0x32000,&shot,sizeof shot);
        story_shots_receive(0x31000,0x32000,0x1e,sizeof shot);story_shots_frame();CHECK(story_shot_replays==2);
        WORD_AT(0x76b140)=1;shot.slot=1;shot.shot.aim[0]=0;WORD_AT(0x3103c)=0;
        memcpy((uint8_t *)memory+0x32000,&shot,sizeof shot);
        story_shots_receive(0x31000,0x32000,0x1e,sizeof shot);CHECK(!story_shots_seen[1]);
        WORD_AT(0x3103c)=1;WORD_AT(0x6000+0x1584+4)=UINT32_MAX;
        story_shots_receive(0x31000,0x32000,0x1e,sizeof shot);CHECK(story_shots_seen[1]==2);
        const unsigned before_relay=npc_send_calls;story_shots_frame();CHECK(npc_send_calls==before_relay+1);
        story_shots_frame();CHECK(npc_send_calls==before_relay+1);
        /* New pickup states traverse the same production reliable-message
           dispatcher as NPC snapshots; they never fall through stock spawn. */
        StoryDropState drop;memset(&drop,0,sizeof drop);
        drop.magic=STORY_DROP_MAGIC;drop.match=story_npc_match();drop.slot=28;drop.serial=1;
        drop.present=1;drop.id=91;drop.source=6;drop.weapon=25;drop.ammo[0]=18;drop.quantity[0]=126;
        WORD_AT(0x76b140)=2;story_drop_reset();memcpy((uint8_t *)memory+0x90000,&drop,sizeof drop);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x31000;WORD_AT(g_esp+8)=0x90000;
        WORD_AT(g_esp+12)=0x1e;WORD_AT(g_esp+16)=sizeof drop+4;
        pc_story_network_npc_receive(frame_original);
        CHECK(g_esp==0x1804 && story_drop_fresh[28] && story_drop_pending[28].quantity[0]==126);
        CHECK(failure_original_calls==originals_before);story_drop_reset();
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
        WORD_AT(0x50094)=0;g_esp=0x1800;WORD_AT(0x7de318)=10;story_loadout_frame();
        StoryLoadout grant_only;memcpy(&grant_only,npc_sent+4,sizeof grant_only);
        CHECK(!grant_only.equipped && npc_send_calls==sent_before+1);
        WORD_AT(0x50094)=25;story_loadout_frame();
        CHECK(npc_send_calls==sent_before+2 && npc_sent_size==sizeof(StoryLoadout)+4 && g_esp==0x1800);
        StoryLoadout wire;memcpy(&wire,npc_sent+4,sizeof wire);CHECK(story_loadout_valid(&wire));
        story_loadout_frame();WORD_AT(0x7de318)=69;story_loadout_frame();
        CHECK(npc_send_calls==sent_before+2);
        WORD_AT(0x7de318)=70;story_loadout_frame();CHECK(npc_send_calls==sent_before+3);
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
        CHECK(g_esp==0x1804 && story_loadout_pending[0].serial==wire.serial);
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
        story_loadout_pending[0]=grant_only;
        WORD_AT(0x76b1a4)=15;g_esp=0x1800;story_loadout_frame();CHECK(!loadout_ammo && !loadout_grants);
        story_loadout_pending[0]=wire;story_loadout_frame();
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
        StoryLoadout rifle=story_loadout_host[1];rifle.equipped=1;StoryLoadout pistol=rifle;pistol.serial=1;pistol.weapon=25;pistol.magazines=8;
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
        CHECK(WORD_AT(actor+0x84)==1 && WORD_AT(0x1808)==3); /* every human setter preserves Story's team */
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
        g_esp=0x1800;WORD_AT(g_esp+4)=actor;
        pc_story_network_team_init(team_init_original);
        CHECK(WORD_AT(actor+0x84)==1 && g_eax==91 && g_esp==0x1804);
        WORD_AT(0x74800+0x150)=0;g_esp=0x1800;
        pc_story_network_team_init(team_init_original);CHECK(WORD_AT(actor+0x84)==1 && g_esp==0x1804);
        WORD_AT(0x74800+0x150)=0x6000+2*0x1584;
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
#ifdef _WIN32
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=4;WORD_AT(0x774028)=0x30000;
        story_pending_stage=1;story_reload_epoch=0;restart_calls=0;
        g_esp=0x1800;pc_story_network_pause_rows();CHECK(restart_rows==1 && g_esp==0x1800);
        WORD_AT(0x76b140)=2;pc_story_network_pause_rows();CHECK(restart_rows==1);WORD_AT(0x76b140)=1;
        const uint32_t old_match=story_npc_match();g_esp=0x1800;WORD_AT(g_esp+4)=0;
        story_npc_assembly[5][0].parts=3;story_npc_fresh[5]=1;
        pc_story_network_restart(restart_original);
        CHECK(restart_calls==1 && g_esp==0x1804 && g_eax==73 && story_reload_epoch==1 &&
              story_context_start_pending && !story_npc_assembly[5][0].parts && !story_npc_fresh[5]);
        StoryRestart message;memcpy(&message,npc_sent+4,sizeof message);
        CHECK(message.magic==STORY_RESTART_MAGIC && message.match==old_match && message.epoch==1 && !message.checkpoint);
        CHECK(story_npc_match()!=old_match);
        WORD_AT(0x76b140)=2;g_esp=0x1800;pc_story_network_restart(restart_original);
        CHECK(restart_calls==1 && g_esp==0x1804); /* guest action cannot reload */
        story_reload_epoch=0;WORD_AT(0x774040)=9;WORD_AT(0x31000)=8;
        memcpy((uint8_t *)memory+0x32000,&message,sizeof message);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message) && restart_calls==1);
        WORD_AT(0x31000)=9;
        message.match^=1;memcpy((uint8_t *)memory+0x32000,&message,sizeof message);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message) && restart_calls==1);
        message.match^=1;message.epoch=2;memcpy((uint8_t *)memory+0x32000,&message,sizeof message);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message) && restart_calls==1);
        message.epoch=1;memcpy((uint8_t *)memory+0x32000,&message,sizeof message);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message-1)==0);
        message.checkpoint=1;memcpy((uint8_t *)memory+0x32000,&message,sizeof message);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message) && restart_calls==1);
        message.checkpoint=0;memcpy((uint8_t *)memory+0x32000,&message,sizeof message);
        g_esp=0x1800;
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message+4) && restart_calls==2 &&
              story_reload_epoch==1 && g_esp==0x1800);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof message) && restart_calls==2);
        online_mode=PC_ONLINE_ARCADE;g_esp=0x1800;pc_story_network_restart(restart_original);
        CHECK(restart_calls==3 && g_esp==0x1804 && g_eax==73);
        online_mode=PC_ONLINE_STORY;WORD_AT(0x79094c)=4;g_esp=0x1800;
        pc_story_network_restart(restart_original);CHECK(restart_calls==4 && g_esp==0x1804);
    }
#endif
#ifdef _WIN32
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=2;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=4;WORD_AT(0x7356d8)=0x50000;WORD_AT(0x7b0c48)=0x6000;
        const uint32_t actor=0x50000+11*0xea0,entity=0x78000,source=0x77000,hit=0x79000;
        WORD_AT(actor)=entity;WORD_AT(actor+8)=11;WORD_AT(entity+0x7c)=actor;
        WORD_AT(entity+0x150)=0;WORD_AT(entity+0x258)=7;
        StoryNpcReplica *r=&story_npc_replicas[11];
        r->entity=entity;r->spawn=7;r->match=story_npc_match();r->pose_present=0;
        WORD_AT(source+0x150)=0x6000;WORD_AT(0x6000)=0;WORD_AT(0x6004)=0;WORD_AT(0x6014)=source;
        WORD_AT(hit)=entity;WORD_AT(hit+4)=source;WORD_AT(hit+8)=0;
        g_esp=0x1800;WORD_AT(g_esp+4)=hit;WORD_AT(0x76b170)=0;story_hit_requests=0;
        CHECK(pc_story_network_damage() && story_hit_requests==1 && g_esp==0x1800);
        WORD_AT(0x76b170)=1;CHECK(pc_story_network_damage() && story_hit_requests==1);
        WORD_AT(0x76b170)=0;WORD_AT(0x6004)=UINT32_MAX;
        CHECK(pc_story_network_damage() && story_hit_requests==1); /* remote human replay */
        WORD_AT(0x6004)=0;WORD_AT(hit+4)=0;WORD_AT(hit+8)=source;
        CHECK(pc_story_network_damage() && story_hit_requests==2); /* projectile owner */
        WORD_AT(0x6014)=0;CHECK(pc_story_network_damage() && story_hit_requests==2);
        WORD_AT(0x6014)=source;r->spawn=6;CHECK(!pc_story_network_damage());r->spawn=7;
        r->match^=1;CHECK(!pc_story_network_damage());r->match=story_npc_match();
        WORD_AT(0x76b140)=1;CHECK(!pc_story_network_damage());
        WORD_AT(0x76b140)=2;online_mode=PC_ONLINE_ARCADE;CHECK(!pc_story_network_damage());
    }
#endif
#ifdef _WIN32
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=2;WORD_AT(0x774028)=0x30000;
        WORD_AT(0x78cec8)=2;WORD_AT(0x78cecc)=0x100000;
        uint8_t *objective=(uint8_t *)memory+0x100000;
        memset(objective,0,0x98);objective[11]=2;
        objective[0x4c+8]=2;objective[0x4c+11]=2; /* VM record must not cross the wire */
        story_objectives_reset();const unsigned sends=npc_send_calls;
        g_esp=0x1800;story_objectives_frame();CHECK(g_esp==0x1800 && npc_send_calls==sends+1);
        StoryObjective s;memcpy(&s,npc_sent+4,sizeof s);
        CHECK(s.magic==STORY_OBJECTIVE_MAGIC && s.index==0 && s.kind==0 && s.state==2);
        story_objectives_frame();CHECK(npc_send_calls==sends+1);
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=9;WORD_AT(0x31000)=8;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);objective[11]=1;
        CHECK(story_objectives_receive(0x31000,0x32000,0x1e,sizeof s));
        story_objectives_frame();CHECK(objective[11]==1);
        WORD_AT(0x31000)=9;WORD_AT(0x7b0c48)=0x6000;
        WORD_AT(0x6004)=0;WORD_AT(0x6114)=256;WORD_AT(0x6000+0x1584+4)=UINT32_MAX;
        CHECK(story_objectives_receive(0x31000,0x32000,0x1e,sizeof s));
        const unsigned popups=objective_popups;
        story_objectives_frame();CHECK(objective[11]==2 && objective_popups==popups);
        WORD_AT(0x6114)=2;story_objectives_frame();CHECK(objective_popups==popups+1);
        story_objectives_frame();CHECK(objective_popups==popups+1);
        CHECK(story_objectives_receive(0x31000,0x32000,0x1e,sizeof s));
        story_objectives_frame();CHECK(objective_popups==popups+1);
        s.serial++;s.kind=2;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_objectives_receive(0x31000,0x32000,0x1e,sizeof s));
        CHECK(story_objectives_pending[0].kind==0); /* never replay host script callbacks */
        s.kind=0;s.match^=1;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_objectives_receive(0x31000,0x32000,0x1e,sizeof s));
        CHECK(story_objectives_pending[0].serial+1==s.serial);
        WORD_AT(0x78cec8)=0;WORD_AT(0x78cecc)=0;story_objectives_reset();
        WORD_AT(0x7bb1e8)=0x120000;WORD_AT(0x120004)=0x121000;
        WORD_AT(0x121000)=0x2f5520;WORD_AT(0x7bb1e0)=0x122000;
        pc_story_network_pause_style();
        float scale;memcpy(&scale,&WORD_AT(0x122050),4);CHECK(scale==1.25f);
        const float y=240.f;memcpy(&WORD_AT(0x120064),&y,4);
        WORD_AT(0x76b140)=1;pc_story_network_pause_layout(0x120000);
        float shifted;memcpy(&shifted,&WORD_AT(0x120064),4);CHECK(shifted==229.f);
        WORD_AT(0x76b140)=2;pc_story_network_pause_layout(0x120000);
        memcpy(&shifted,&WORD_AT(0x120064),4);CHECK(shifted==229.f);
        WORD_AT(0x122050)=0;WORD_AT(0x121000)=0x2cbfc0;
        pc_story_network_pause_style();CHECK(!WORD_AT(0x122050));
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=4;WORD_AT(0x790950)=4;
        WORD_AT(0x7b0c48)=0x6000;WORD_AT(0x75f5d0)=0x130000;
        memset((uint8_t *)memory+0x130000,0,4*0x238);
        for(unsigned i=0;i<4;i++) {
            const uint32_t context=0x6000+i*0x1584,entity=0x74000+i*0x400,actor=0x50000+i*0xea0;
            WORD_AT(context)=i;WORD_AT(context+0x14)=entity;
            WORD_AT(entity+0x150)=context;WORD_AT(entity+0x7c)=actor;
        }
        story_checkpoint_reset();story_reload_epoch=0;g_esp=0x1800;WORD_AT(g_esp+4)=3;
        pc_story_network_checkpoint_capture(checkpoint_capture_original);
        CHECK(g_esp==0x1804 && checkpoint_captures==1 && checkpoint_extras==2 && story_checkpoint_serial==1);
        CHECK(WORD_AT(0x130004)==8 && WORD_AT(0x130000+3*0x238+4)==8 && story_checkpoint_available(8));
        const unsigned sends=npc_send_calls;
        g_esp=0x1800;story_checkpoint_frame();CHECK(npc_send_calls==sends+1);
        StoryCheckpoint s;memcpy(&s,npc_sent+4,sizeof s);
        CHECK(s.magic==STORY_CHECKPOINT_MAGIC && s.index==3 && s.serial==1);
        story_checkpoint_frame();CHECK(npc_send_calls==sends+1);
        story_checkpoint_reset();memset((uint8_t *)memory+0x130000,0,4*0x238);
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=9;WORD_AT(0x31000)=8;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_checkpoint_receive(0x31000,0x32000,0x1e,sizeof s) && !story_checkpoint_pending.serial);
        WORD_AT(0x31000)=9;
        CHECK(story_checkpoint_receive(0x31000,0x32000,0x1e,sizeof s) && story_checkpoint_pending.serial==1);
        story_checkpoint_frame();
        CHECK(checkpoint_captures==2 && checkpoint_extras==4 && story_checkpoint_serial==1 && !story_checkpoint_pending.serial);
        CHECK(story_checkpoint_receive(0x31000,0x32000,0x1e,sizeof s) && !story_checkpoint_pending.serial);
        g_esp=0x1800;WORD_AT(g_esp+4)=4;pc_story_network_checkpoint_capture(checkpoint_capture_original);
        CHECK(checkpoint_captures==2 && g_esp==0x1804); /* guests cannot capture independently */
        WORD_AT(0x7b0c7c)=0x6000+3*0x1584;g_esp=0x1800;
        pc_story_network_checkpoint_restore(checkpoint_restore_original);
        CHECK(checkpoint_restores==1 && checkpoint_extra_restores==2 && g_esp==0x1804);
        CHECK(WORD_AT(0x7b0c7c)==0x6000+3*0x1584);
        WORD_AT(0x7de30c)=7;
        for(unsigned i=0;i<4;i++) {
            WORD_AT(0x6000+i*0x1584+0x114)=2;
            WORD_AT(0x130000+i*0x238+0x10)=25;
        }
        WORD_AT(0x6000+3*0x1584+0x114)=4;
        checkpoint_final_restore=1;story_checkpoint_restore_inventory();CHECK(!checkpoint_inventory_restores);
        WORD_AT(0x6000+3*0x1584+0x114)=2;
        const unsigned selections=loadout_selections;
        story_checkpoint_restore_inventory();CHECK(checkpoint_inventory_restores==4 && loadout_selections==selections+4);
        CHECK(!story_checkpoint_restore_pending);
        story_checkpoint_restore_inventory();CHECK(checkpoint_inventory_restores==4);
        checkpoint_final_restore=0;
        WORD_AT(0x76b140)=1;g_esp=0x1800;WORD_AT(g_esp+4)=8;
        const unsigned reloads=restart_calls;pc_story_network_restart(restart_original);
        CHECK(restart_calls==reloads+1 && story_checkpoint_serial==1 && story_reload_epoch==1);
        StoryRestart restart;memcpy(&restart,npc_sent+4,sizeof restart);CHECK(restart.checkpoint==8);
        WORD_AT(0x76b140)=2;story_reload_epoch=0;
        memcpy((uint8_t *)memory+0x32000,&restart,sizeof restart);
        CHECK(story_restart_receive(0x31000,0x32000,0x1e,sizeof restart));
        CHECK(restart_calls==reloads+2 && story_checkpoint_serial==1 && story_reload_epoch==1);
        WORD_AT(0x130004)=0;g_esp=0x1800;
        pc_story_network_checkpoint_restore(frame_original);
        CHECK(checkpoint_extra_restores==2 && g_esp==0x1804); /* fresh loads do not restore blank extras */
        story_checkpoint_reset();
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=2;WORD_AT(0x774028)=0x30000;
        story_effect_reset();g_esp=0x1800;
        const float point=12.f,normal=1.f;
        memcpy(&WORD_AT(0x170000),&point,4);memcpy(&WORD_AT(0x170010),&normal,4);
        pc_story_network_effect_decision(effect_decision_original,0);
        CHECK(effect_decisions==1 && g_esp==0x1804 && g_eax==99);
        StoryEffect s;memcpy(&s,npc_sent+4,sizeof s);
        CHECK(story_effect_valid(&s) && s.kind==0 && s.position[0]==12.f && s.color==0x7f123456);
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=9;WORD_AT(0x31000)=8;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s) && !story_effect_count);
        WORD_AT(0x31000)=9;
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s) && story_effect_count==1);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s) && story_effect_count==1);
        WORD_AT(0x7de30c)=7;g_esp=0x1800;WORD_AT(0x150098)=0;
        story_effect_frame();CHECK(effect_emitters==1 && WORD_AT(0x150098)==0x7f123456 && g_esp==0x1800);
        story_effect_frame();CHECK(effect_emitters==1);
        g_esp=0x1800;pc_story_network_effect_decision(effect_decision_original,0);
        CHECK(effect_decisions==1 && g_esp==0x1804); /* no guest RNG/effect decision */
        s.serial++;s.kind=2;s.a=2;s.b=0;s.options=0;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();
        CHECK(effect_trails==1 && effect_moves==1 && story_effect_trail==0x160000);
        s.serial++;s.a=3;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();
        CHECK(effect_trails==1 && effect_moves==2);
        s.serial++;s.a=0;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();
        CHECK(effect_deletes==1 && !story_effect_trail);
        s.serial++;s.kind=1;s.a=39;s.b=UINT32_MAX;s.options=1;s.value=0.f;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();CHECK(effect_blasts==1);
        s.serial++;s.position[0]=NAN;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s) && !story_effect_count);
        s.position[0]=12.f;s.match^=1;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s) && !story_effect_count);
        s.match^=1;s.serial=8;s.kind=2;s.a=0;s.b=0;s.options=0;s.value=0.f;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();
        s.serial=7;s.kind=1;s.a=39;s.b=UINT32_MAX;s.options=1;
        memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();CHECK(effect_blasts==2);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();CHECK(effect_blasts==2);
        /* A near guest feels the same bomb even when slot0 is far away.
           Serial rejection also prevents repeated feedback on retransmission. */
        {
            const uint32_t pool=0x6000,local=pool+0x1584,camera=0x190000;
            WORD_AT(0x7b0c48)=pool;WORD_AT(pool)=0;WORD_AT(pool+4)=2;WORD_AT(pool+8)=0;
            WORD_AT(local)=1;WORD_AT(local+4)=0;WORD_AT(local+8)=camera;
            float eye[3]={22.f,0.f,0.f};memcpy((uint8_t *)memory+camera+0x27c,eye,sizeof eye);
            memset(s.position,0,sizeof s.position);s.position[0]=12.f;
            s.serial=20;
            memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
            CHECK(story_effect_valid(&s));story_effect_receive(0x31000,0x32000,0x1e,sizeof s);story_effect_frame();
            CHECK(bomb_shakes==1 && WORD_AT(local+0xfbc)==0x40300000u && WORD_AT(local+0xfc0)==0x3f666666u);
            story_effect_receive(0x31000,0x32000,0x1e,sizeof s);story_effect_frame();CHECK(bomb_shakes==1);
            eye[0]=100.f;memcpy((uint8_t *)memory+camera+0x27c,eye,sizeof eye);
            s.serial++;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
            story_effect_receive(0x31000,0x32000,0x1e,sizeof s);story_effect_frame();CHECK(bomb_shakes==1);
        }
        /* Captured physical barrel path, outside the ambience decision scope.
           Both owner arguments are nonzero local entities; time is arg7. */
        story_effect_reset();WORD_AT(0x76b140)=1;g_esp=0x1800;
        WORD_AT(g_esp)=0xbc70c;WORD_AT(g_esp+4)=1;WORD_AT(g_esp+8)=23;
        WORD_AT(g_esp+12)=0x170000;WORD_AT(g_esp+16)=0;
        WORD_AT(g_esp+20)=0x72000;WORD_AT(g_esp+24)=0x73000;
        WORD_AT(g_esp+28)=0;WORD_AT(g_esp+32)=1;
        const unsigned sends=npc_send_calls;
        pc_story_network_effect_create(effect_factory_original,1);
        CHECK(npc_send_calls==sends+1 && g_esp==0x1804);
        memcpy(&s,npc_sent+4,sizeof s);CHECK(s.kind==4 && s.a==1 && s.b==23 && s.value==0 && s.position[0]==12.f);
        WORD_AT(0x76b140)=2;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();
        CHECK(effect_barrels==1 && !story_effect_barrel_replay && !story_effect_cosmetic_depth);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();CHECK(effect_barrels==1);
        s.serial++;s.a=2;CHECK(!story_effect_valid(&s));s.a=1;s.value=1;CHECK(!story_effect_valid(&s));
        /* Actual recorded mesh1341 callback218650 produces authored47,
           using the guest weapon class and a non-null hit direction. */
        story_effect_reset();WORD_AT(0x76b140)=1;g_esp=0x1800;
        WORD_AT(g_esp)=0x218702;WORD_AT(g_esp+4)=47;WORD_AT(g_esp+8)=35;
        WORD_AT(g_esp+12)=0x170000;WORD_AT(g_esp+16)=0x170010;
        WORD_AT(g_esp+20)=0x72000;WORD_AT(g_esp+24)=0;
        WORD_AT(g_esp+28)=0;WORD_AT(g_esp+32)=1;
        pc_story_network_effect_create(effect_factory_original,1);
        memcpy(&s,npc_sent+4,sizeof s);CHECK(s.kind==4 && s.a==47 && s.b==23 && s.decal_flags==1 && s.direction[0]==1.f);
        WORD_AT(0x76b140)=2;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();CHECK(effect_barrels==2);
        s.serial++;s.a=40;memcpy((uint8_t *)memory+0x32000,&s,sizeof s);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof s));story_effect_frame();
        CHECK(effect_barrels==3); /* preserve the paired authored profile */
        s.decal_flags=2;CHECK(!story_effect_valid(&s));s.decal_flags=1;
        s.a=41;CHECK(!story_effect_valid(&s));s.a=40;
        s.direction[0]=NAN;CHECK(!story_effect_valid(&s));

        story_effect_reset();
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=2;
        WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=2;WORD_AT(0x790950)=2;WORD_AT(0x7de30c)=7;
        WORD_AT(0x7b0c48)=0x6000;WORD_AT(0x7b0c7c)=0x6000;
        const uint32_t own=0x6000+0x1584;
        WORD_AT(0x6000)=0;WORD_AT(0x6004)=0;WORD_AT(0x6008)=0;WORD_AT(0x6014)=0x74000;
        WORD_AT(own)=1;WORD_AT(own+4)=0;WORD_AT(own+8)=0x1b0000;WORD_AT(own+0x14)=0x74400;
        WORD_AT(0x74000+0x150)=0x6000;WORD_AT(0x74400+0x150)=own;
        WORD_AT(0x733100)=0x180000;WORD_AT(0x733104)=1;WORD_AT(0x1800bc)=0x4352434d;
        WORD_AT(0x70e164)=0x190000;WORD_AT(0x190020)=1;WORD_AT(0x1b021c)=0x112;
        WORD_AT(0x774040)=9;WORD_AT(0x31000)=9;story_cine_reset();
        StoryCinematic s;memset(&s,0,sizeof s);
        s.magic=STORY_CINE_MAGIC;s.match=story_npc_match();s.serial=1;s.active=1;s.opening=1;
        s.view_visible=1;s.pose_present=1;s.pose.clip=42;s.pose.next_clip=43;
        s.cue_serial=1;s.cue_active=1;s.cue_tag=0x4352434d;s.cue_flags=4;s.cue_mode=11;
        s.fade_present=1;s.fade[0]=0x3f000000;s.fade[2]=0x3f800000;s.fade[4]=0x3e800000;
        CHECK(story_cine_valid(&s,1));
        StoryCinematicBase base;memcpy(base.state,&s,STORY_CINE_BASE_BYTES);base.checksum=story_cine_checksum(&s);
        StoryCinematicPose pose={STORY_CINE_POSE_MAGIC,s.match,s.serial,base.checksum,s.pose};
        memcpy((uint8_t *)memory+0x32000,&base,sizeof base);memcpy((uint8_t *)memory+0x33000,&pose,sizeof pose);
        WORD_AT(0x31000)=8;
        CHECK(story_cine_receive(0x31000,0x32000,0x1e,sizeof base) && !story_cine_parts);
        WORD_AT(0x31000)=9;
        CHECK(story_cine_receive(0x31000,0x33000,0x1e,sizeof pose) && !story_cine_seen);
        CHECK(story_cine_receive(0x31000,0x32000,0x1e,sizeof base) && story_cine_seen==1);
        g_esp=0x1800;g_eax=123;story_cine_frame();
        CHECK(cine_begins==1 && WORD_AT(0x1b021c)==0x113 && !WORD_AT(0x190020) && WORD_AT(0x190008)==s.fade[0]);
        CHECK(g_esp==0x1800 && g_eax==123 && WORD_AT(0x7b0c48)==0x6000 && WORD_AT(0x790950)==2 && WORD_AT(0x7b0c7c)==0x6000);
        story_cine_frame();CHECK(cine_begins==1);
        CHECK(pc_story_network_cinematic_pose(0x74400) && npc_last_pose.clip==42);
        WORD_AT(0x76b140)=1;CHECK(!pc_story_network_cinematic_pose(0x74000));
        CHECK(pc_story_network_cinematic_pose(0x74400));WORD_AT(0x76b140)=0;
        CHECK(!pc_story_network_cinematic_pose(0x74400));WORD_AT(0x76b140)=2;
        s.view_visible=0;s.serial=2;s.cue_serial=2;s.cue_active=0;s.cue_tag=0;s.active=0;s.opening=0;s.pose_present=0;
        memset(&s.pose,0,sizeof s.pose);memcpy(base.state,&s,STORY_CINE_BASE_BYTES);base.checksum=story_cine_checksum(&s);
        memcpy((uint8_t *)memory+0x32000,&base,sizeof base);WORD_AT(own+0x1580)=0xdeadbeef;
        CHECK(story_cine_receive(0x31000,0x32000,0x1e,sizeof base));story_cine_frame();
        CHECK(cine_ends==1 && WORD_AT(0x1b021c)==0x112 && WORD_AT(own+0x114)==2 && !pc_story_network_cinematic_pose(0x74400));
        story_cine_frame();CHECK(cine_ends==1);
        s.serial=3;s.match^=1;memcpy(base.state,&s,STORY_CINE_BASE_BYTES);base.checksum=story_cine_checksum(&s);
        memcpy((uint8_t *)memory+0x32000,&base,sizeof base);
        CHECK(story_cine_receive(0x31000,0x32000,0x1e,sizeof base) && story_cine_seen==2);
        s.match^=1;s.fade[0]=0x7fc00000;CHECK(!story_cine_valid(&s,1));
        s.fade[0]=0;s.serial=4;s.feed_serial=1;s.feed_active=1;s.feed_tag=0x4352434d;s.feed_remaining=5.f;
        WORD_AT(0x733108)=0x200000;story_cine_state=s;
        story_cine_frame();story_cine_frame();CHECK(feed_starts==1 && !feed_stops);
        story_cine_state.feed_serial=2;story_cine_state.feed_active=0;
        story_cine_frame();CHECK(feed_stops==1);
        story_cine_reset();
        /* A late join's first host snapshot can already be inactive. It has
           never observed an active->inactive edge but still owns an intro view. */
        memset(&story_cine_state,0,sizeof story_cine_state);
        story_cine_state.serial=1;story_cine_state.match=story_npc_match();
        story_cine_state.view_visible=1;story_rejoin_view_pending=1;
        WORD_AT(own+0x114)=2;const unsigned ends_before_join=cine_ends;
        story_cine_state.active=1;g_esp=0x1800;story_cine_frame();
        CHECK(cine_ends==ends_before_join && story_rejoin_view_pending); /* Host still in a cinematic. */
        story_cine_state.active=0;g_esp=0x1800;story_cine_frame();
        CHECK(cine_ends==ends_before_join+1 && !story_rejoin_view_pending && g_esp==0x1800);
        CHECK(WORD_AT(0x7b0c48)==0x6000 && WORD_AT(0x790950)==2 && WORD_AT(0x7b0c7c)==0x6000);
        story_cine_frame();CHECK(cine_ends==ends_before_join+1);
        story_cine_reset();
    }
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
        story_lobby_mission=0;story_pending_stage=1;WORD_AT(0x79094c)=2;
        WORD_AT(0x7de458)=0x80008;WORD_AT(0x7de30c)=7;WORD_AT(0x7de400)=(WORD_AT(0x7de400)&~255u)|2u;
        story_checkpoint_serial=0;story_loadout_reset();WORD_AT(0x7b0c48)=0x6000;
        WORD_AT(0x7407c)=0x50000;WORD_AT(0x50000)=0x74000;
        WORD_AT(0x5018c)=0x1d0000;WORD_AT(0x1d007c)=0x500a4;WORD_AT(0x50190)=0x50000;
        for(unsigned i=0;i<2;i++) {
            const uint32_t context=0x6000+i*0x1584,entity=0x74000+i*0x400;
            WORD_AT(context+0x14)=entity;WORD_AT(entity+0x150)=context;WORD_AT(entity+4)=0x1c0000+i*0x100;
        }
        g_esp=0x1800;pc_story_network_world_visibility(visibility_original);
        CHECK(g_esp==0x1804 && !WORD_AT(0x74010) && WORD_AT(0x1c0010)==0x170000 && !WORD_AT(0x74410));
        CHECK(!WORD_AT(0x1d0010) && !WORD_AT(0x1d000c));
        CHECK(WORD_AT(0x7b4cc0)==0x1e0000 && WORD_AT(0x7b54d8)==1);
        story_loadout_host[0].equipped=1;story_loadout_host[0].match=story_npc_match();g_esp=0x1800;
        WORD_AT(0x6000+0x14f8)=3;pc_story_network_world_visibility(visibility_original);
        CHECK(WORD_AT(0x74410)==0x5678ffff && WORD_AT(0x7b54d8)==4);
        /* Mode3 can persist after the handoff. It must not replay a frozen
           opening pose over live remote player animations until the next cue. */
        WORD_AT(0x6008)=0x1b0000;WORD_AT(0x6114)=2;WORD_AT(0x7de45c)=0;
        WORD_AT(0x774028)=0x30000;story_cine_published=0;story_cine_cue_active=0;
        g_esp=0x1800;story_cine_frame();
        CHECK(!story_cine_state.opening && !story_cine_state.active);
        CHECK(!pc_story_network_cinematic_pose(0x74400));
        WORD_AT(0x6008)=0;
        WORD_AT(0x6000+0x14f8)=0;g_esp=0x1800;
        pc_story_network_world_visibility(visibility_original);
        CHECK(WORD_AT(0x74010)==0x1234ffff && WORD_AT(0x1c0010)==0x170000 && WORD_AT(0x74410)==0x5678ffff);
        CHECK(WORD_AT(0x1d0010)==0x1234ffff);
        CHECK(WORD_AT(0x7b4cc0)==0x74000 && WORD_AT(0x7b54d8)==4);
        WORD_AT(0x76b140)=2;g_esp=0x1800;pc_story_network_world_visibility(visibility_original);
        CHECK(!WORD_AT(0x74010) && WORD_AT(0x1c0010)==0x170000);story_checkpoint_serial=1;
        g_esp=0x1800;pc_story_network_world_visibility(visibility_original);CHECK(WORD_AT(0x74010)==0x1234ffff && WORD_AT(0x1c0010)==0x170000);
        WORD_AT(0x76b140)=1;
        WORD_AT(0x7de458)=0x880008;WORD_AT(0x7de45c)=0x8000;
        g_esp=0x1800;WORD_AT(g_esp)=0x69518;
        pc_story_network_stage_builder(stage_original);
        CHECK(g_esp==0x1804 && g_eax==77 && WORD_AT(0x79094c)==2 && WORD_AT(0x7de458)==0x880008);
        story_pending_stage=2;WORD_AT(0x79094c)=25;WORD_AT(0x7de458)=0x20000000;
        g_esp=0x1800;WORD_AT(g_esp)=0x65ccf;pc_story_network_stage_builder(stage_original);
        CHECK(g_esp==0x1804 && WORD_AT(0x79094c)==25 && WORD_AT(0x78ad50)==2);
        WORD_AT(0x76b140)=2;story_pending_stage=3;g_esp=0x1800;
        const unsigned calls=failure_original_calls;pc_story_network_advance(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==calls);
        g_esp=0x1800;pc_story_network_result_action(frame_original,0u);
        CHECK(g_esp==0x1804 && failure_original_calls==calls); /* guest menu stays open */
        story_pending_stage=1;WORD_AT(0x76b140)=1;WORD_AT(0x79094c)=2;
        WORD_AT(0x7de458)=0x80008;WORD_AT(0x6000)=0;WORD_AT(0x6004)=0;
        WORD_AT(0x7b0c7c)=0x7584;WORD_AT(0x7ba9e0)=1;
        g_esp=0x1800;pc_story_network_result_menu(result_menu_original);
        CHECK(g_eax==77 && g_esp==0x1804 && WORD_AT(0x7b0c7c)==0x7584 && WORD_AT(0x7ba9e0)==1);
        const unsigned restarts=restart_calls;
        g_esp=0x1800;pc_story_network_result_action(frame_original,1u);
        CHECK(g_esp==0x1804 && restart_calls==restarts+1 && failure_original_calls==calls);
        WORD_AT(0x76b140)=2;g_esp=0x1800;pc_story_network_result_action(frame_original,1u);
        CHECK(g_esp==0x1804 && restart_calls==restarts+1 && failure_original_calls==calls);
        g_esp=0x1800;WORD_AT(g_esp+20)=0x2eb0c0;CHECK(story_result_button(frame_original) && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp+20)=0x2eb290;CHECK(story_result_button(frame_original));
        g_esp=0x1800;WORD_AT(g_esp+20)=0x2eaf60;CHECK(!story_result_button(frame_original) && g_esp==0x1800);
        /* Blood uses local texture resolution and the original decal API,
           including mode4 ground pools; no health routine is replayed. */
        story_effect_reset();WORD_AT(0x7de30c)=7;WORD_AT(0x774040)=0;WORD_AT(0x31000)=0;
        StoryEffect blood={0};blood.magic=STORY_EFFECT_MAGIC;blood.match=story_npc_match();blood.serial=1;
        blood.kind=3;blood.a=957;blood.b=1;blood.color=0x7f1e1e32;blood.options=4;
        blood.value=1.f;blood.decal_flags=1;blood.room=3;blood.position[0]=1.f;blood.direction[1]=-1.f;
        memcpy((uint8_t *)memory+0x32000,&blood,sizeof blood);
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof blood));
        CHECK(story_effect_receive(0x31000,0x32000,0x1e,sizeof blood));
        g_esp=0x1800;story_effect_frame();CHECK(blood_decals==1 && g_esp==0x1800);
        WORD_AT(0x4d2d64+957*24)=777;g_esp=0x1800;
        WORD_AT(g_esp+4)=777;WORD_AT(g_esp+8)=1;WORD_AT(g_esp+12)=blood.color;WORD_AT(g_esp+16)=4;
        WORD_AT(g_esp+20)=0x3f800000;WORD_AT(g_esp+24)=1;WORD_AT(g_esp+28)=3;
        WORD_AT(g_esp+32)=0x33000;WORD_AT(g_esp+36)=0x33010;
        memcpy((uint8_t *)memory+0x33000,blood.position,12);memcpy((uint8_t *)memory+0x33010,blood.direction,12);
        pc_story_network_blood_decal(blood_original);CHECK(g_eax==1 && g_esp==0x1804 && story_local_blood_count==1);
        blood.serial=2;memcpy((uint8_t *)memory+0x32000,&blood,sizeof blood);
        story_effect_receive(0x31000,0x32000,0x1e,sizeof blood);story_effect_frame();CHECK(blood_decals==1);
        WORD_AT(0x7de318)+=31;CHECK(!story_effect_blood_seen_locally(&blood));
        blood.value=-1.f;CHECK(!story_effect_valid(&blood));
        blood.value=1.f;blood.a=964;CHECK(!story_effect_valid(&blood));
        /* Frontend and movie payloads never dereference retired human bodies;
           the shared guard also protects ordinary Online transitions. */
        online_mode=PC_ONLINE_ARCADE;WORD_AT(0x79094c)=101;g_esp=0x1800;
        pc_network_player_payload(frame_original);CHECK(failure_original_calls==calls && g_esp==0x1804);
        WORD_AT(0x79094c)=2;WORD_AT(0x790950)=2;WORD_AT(0x7b0c48)=0;
        g_esp=0x1800;pc_network_player_payload(frame_original);CHECK(failure_original_calls==calls);
        WORD_AT(0x76b108)=0;g_esp=0x1800;pc_network_player_payload(frame_original);
        CHECK(failure_original_calls==calls+1 && g_esp==0x1804);
    }
#endif
#ifdef _WIN32
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=15;
        WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;WORD_AT(0x7de30c)=7;
        WORD_AT(0x7de400)=2;((uint8_t *)memory)[0x7de455]=10;
        WORD_AT(0x72f5e8)=0x210000;WORD_AT(0x7b4fac)=2;story_world_reset();
        const uint32_t a=0x210000,b=a+0x2e8;
        memset((uint8_t *)memory+a,0,2*0x2e8);WORD_AT(a+0x20)=1;WORD_AT(a+0x2c)=11;
        WORD_AT(a+0x60)=0x42415252;WORD_AT(a+0x94)=20;WORD_AT(a+0x230)=101;
        const uint32_t key=pc_story_network_world_key(a);CHECK(key && pc_story_network_world_entity(key)==a);
        /* Local first-person models are not persistent world props. A fresh
           unnamed model can otherwise hash identically on host and guest. */
        memcpy((uint8_t *)memory+b,(uint8_t *)memory+a,0x2e8);
        WORD_AT(b+0x2c)=22;WORD_AT(b+0x28)=0x40000000;WORD_AT(b+0x34)=0xca;
        CHECK(!pc_story_network_world_key(b) && pc_story_network_world_entity(key)==a);
        for(unsigned room=0xcau;room<=0xceu;room++) {
            WORD_AT(b+0x34)=room;CHECK(!pc_story_network_world_key(b));
        }
        /* Even a previously cached prop key cannot admit a recycled FP model. */
        WORD_AT(b+0x28)=0;WORD_AT(b+0x34)=0;CHECK(pc_story_network_world_key(b)==key);
        WORD_AT(b+0x28)=0x40000000;WORD_AT(b+0x34)=0xca;
        CHECK(!pc_story_network_world_key(b));
        WORD_AT(a+0x28)=0x80000000;CHECK(!pc_story_network_world_entity(key));
        WORD_AT(a+0x28)=0;
        const unsigned before_fp_sends=npc_send_calls;
        g_esp=0x1800;WORD_AT(g_esp+4)=b;pc_story_network_world_remove(world_remove_original);
        CHECK(npc_send_calls==before_fp_sends && (WORD_AT(b+0x28)&0x80000000u));
        memset((uint8_t *)memory+b,0,0x2e8);

        g_esp=0x1800;WORD_AT(g_esp+4)=a;pc_story_network_world_remove(world_remove_original);
        StoryWorldRemove removal;memcpy(&removal,npc_sent+4,sizeof removal);
        CHECK(g_eax==37 && g_esp==0x1804 && removal.key==key && removal.magic==STORY_WORLD_REMOVE_MAGIC);
        /* The other peer constructs this prop in another pool slot and has
           another original network ID. Authored identity still resolves it. */
        memcpy((uint8_t *)memory+b,(uint8_t *)memory+a,0x2e8);
        WORD_AT(a+0x28)=0x80000000;WORD_AT(b+0x28)=0;WORD_AT(b+0x2c)=44;WORD_AT(b+0x230)=555;
        CHECK(pc_story_network_world_entity(key)==b);
        WORD_AT(0x76b140)=2;WORD_AT(0x774040)=0;WORD_AT(0x31000)=1;
        memcpy((uint8_t *)memory+0x32000,&removal,sizeof removal);
        story_world_receive(0x31000,0x32000,0x1e,sizeof removal);CHECK(!story_world_count);
        WORD_AT(0x31000)=0;story_world_receive(0x31000,0x32000,0x1e,sizeof removal);
        story_world_receive(0x31000,0x32000,0x1e,sizeof removal);CHECK(story_world_count==1);
        story_world_frame();CHECK(world_removed==1 && !pc_story_network_world_entity(key));
        /* Never choose between duplicate authored keys by local pool order. */
        WORD_AT(a+0x28)=WORD_AT(b+0x28)=0;CHECK(!pc_story_network_world_entity(key));
        online_mode=PC_ONLINE_ARCADE;CHECK(!pc_story_network_world_key(a));
        /* The shared Online pickup must clamp in32bits before writing signed16,
           including overfull dual clips and recovery from negative reserves. */
        const uint32_t reserve=0x50000+0x61e+3*2,entry=0x4ea3f8+3*0xe0;
        WORD_AT(entry)=32767;WORD_AT(entry+8)=0;ammo_loaded=0;
        *(int16_t *)((uint8_t *)memory+reserve)=32760;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x50000;WORD_AT(g_esp+8)=3;WORD_AT(g_esp+12)=100;
        pc_network_ammo_pickup(frame_original);
        CHECK(g_esp==0x1804 && g_eax==7 && *(int16_t *)((uint8_t *)memory+reserve)==32767);
        WORD_AT(entry)=16;ammo_loaded=32;g_esp=0x1800;pc_network_ammo_pickup(frame_original);
        CHECK(!g_eax && *(int16_t *)((uint8_t *)memory+reserve)==32767);
        *(int16_t *)((uint8_t *)memory+reserve)=112;
        g_esp=0x1800;pc_network_ammo_pickup(frame_original);
        CHECK(!g_eax && *(int16_t *)((uint8_t *)memory+reserve)==112);
        *(int16_t *)((uint8_t *)memory+reserve)=-16;WORD_AT(entry)=100;ammo_loaded=10;
        g_esp=0x1800;pc_network_ammo_pickup(frame_original);CHECK(g_eax==90 && *(int16_t *)((uint8_t *)memory+reserve)==90);
        WORD_AT(0x76b108)=0;g_esp=0x1800;const unsigned originals=failure_original_calls;
        pc_network_ammo_pickup(frame_original);CHECK(failure_original_calls==originals+1);
    }
#endif
#ifdef _WIN32
    {
        /* Payload readiness includes live ownership in both directions and
           stops at the accepted ending stage before scene teardown. */
        WORD_AT(0x76b108)=1;WORD_AT(0x79094c)=2;WORD_AT(0x790950)=1;
        WORD_AT(0x7b0c48)=0x6000;WORD_AT(0x6000)=0;WORD_AT(0x6014)=0x74000;
        WORD_AT(0x74150)=0x6000;WORD_AT(0x7407c)=0x50000;WORD_AT(0x50000)=0x74000;
        WORD_AT(0x74028)=0;online_mode=PC_ONLINE_ARCADE;
        unsigned calls=failure_original_calls;g_esp=0x1800;pc_network_player_payload(frame_original);
        CHECK(failure_original_calls==calls+1);
        WORD_AT(0x74028)=0x80000000;g_esp=0x1800;pc_network_player_payload(frame_original);
        CHECK(failure_original_calls==calls+1 && g_esp==0x1804);
        WORD_AT(0x74028)=0;WORD_AT(0x50000)=0;g_esp=0x1800;pc_network_player_payload(frame_original);
        CHECK(failure_original_calls==calls+1);
        WORD_AT(0x50000)=0x74000;online_mode=PC_ONLINE_STORY;story_pending_stage=2;
        g_esp=0x1800;pc_network_player_payload(frame_original);CHECK(failure_original_calls==calls+1);
    }
#endif
#ifdef _WIN32
    {
        /* Rewards must not rebuild party ownership from the frontend's16 slots. */
        uint8_t *config=(uint8_t *)memory+0x9000;
        memset(config,0,0xa0);config[0]=2;config[1]=1;config[4]=0x5a;
        config[0x54]=2;config[0x55]=10;config[0x56]=2;config[0x57]=1;
        WORD_AT(0x9058)=0x80008;
        *(int16_t *)((uint8_t *)memory+0x4d1ac6)=2;
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=10;
        g_esp=0x1800;WORD_AT(g_esp)=0;CHECK(pc_story_network_prepare(0x9000));
        ((uint8_t *)memory)[0x7de400]=16;((uint8_t *)memory)[0x7de401]=1;
        config[0]=16;config[4]=0;const unsigned prior_broadcasts=story_broadcasts;
        g_esp=0x1800;WORD_AT(g_esp)=0x65cdc;CHECK(pc_story_network_prepare(0x9000));
        CHECK(config[0]==2 && config[1]==1 && config[4]==0x5a && story_broadcasts==prior_broadcasts+1);
        story_load_header[0]=5;g_esp=0x1800;CHECK(!pc_story_network_prepare(0x9000));
        CHECK(story_broadcasts==prior_broadcasts+1);
        /* Captured unlock-page GO rebuilt16 Arcade humans and old scene24.
           The production serializer boundary must build the next Story config
           without recursively broadcasting or incrementing GO's generation. */
        story_load_header[0]=2;story_load_header[1]=1;
        story_lobby_mission=0;story_lobby_difficulty=1;story_pending_stage=3;
        WORD_AT(0x79094c)=106;WORD_AT(0x76b1a4)=15;WORD_AT(0x78ad2c)=13;
        uint8_t *go=(uint8_t *)memory+0x78ad58;memset(go,0,0xa0);
        go[0]=16;go[1]=1;go[0x54]=24;go[0x55]=10;go[0x56]=2;WORD_AT(0x78adb0)=0x20000000;
        *(int16_t *)((uint8_t *)memory+0x4d1b0a)=3;
        const unsigned factories_before=story_factories,broadcasts_before=story_broadcasts,generation_before=story_generation;
        factory_reward_go=1;g_esp=0x1800;WORD_AT(g_esp)=0x2d3980;
        CHECK(pc_story_network_prepare(0x78ad58));factory_reward_go=0;
        CHECK(go[0]==2 && go[1]==1 && go[0x54]==3 && go[0x57]==1);
        CHECK(story_lobby_mission==1 && story_pending_stage==1 && story_factories==factories_before+1);
        CHECK(story_broadcasts==broadcasts_before && story_generation==generation_before);
        WORD_AT(0x76b1a4)=10;g_esp=0x1800;WORD_AT(g_esp)=0x2d398d;
        CHECK(pc_story_network_prepare(0x78ad58) && story_factories==factories_before+1);
        WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=15;story_lobby_mission=0;story_pending_stage=3;
        CHECK(pc_story_network_prepare(0x78ad58) && story_lobby_mission==1 && story_pending_stage==1);
        /* Only a validated first-mission gameplay->ending handoff records
           this peer's local profile; retries/rewards/failure/host do not. */
        WORD_AT(0x76b140)=2;story_lobby_mission=0;story_pending_stage=1;
        WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;WORD_AT(0x7de45c)=0x2000000;
        WORD_AT(0x790950)=2;WORD_AT(0x7b0c48)=0x6000;WORD_AT(0x7356d8)=0x50000;
        WORD_AT(0x7bedc0+0x1f54)=1;const unsigned old_records=reward_records;
        g_esp=0x1800;story_rewards_record(0,2);
        CHECK(reward_records==old_records+1 && g_esp==0x1800 && WORD_AT(0x7de45c)==0x2000000);
        story_pending_stage=2;story_rewards_record(0,2);CHECK(reward_records==old_records+1);
        story_pending_stage=1;story_rewards_record(0,3);story_rewards_record(1,2);
        WORD_AT(0x76b140)=1;story_rewards_record(0,2);CHECK(reward_records==old_records+1);
        /* The previously unwrapped initial rewards Continue must not open the
           guest's empty unlock dialog. Host and offline actions still run. */
        story_pending_stage=3;story_lobby_mission=0;WORD_AT(0x79094c)=106;WORD_AT(0x76b140)=2;
        /* Show original local rewards first; final confirmation rebuilds the
           same descriptor as waiting and never calls frontend GO. */
        reward_count=7;WORD_AT(0x75fd88)=0;marker=0;g_esp=0x1800;
        story_rewards_build();CHECK(marker==64 && !story_rewards_waiting && g_esp==0x1804);
        marker=0;g_esp=0x1800;story_rewards_tick();CHECK(marker==66 && g_esp==0x1804);
        marker=0;g_esp=0x1800;story_rewards_page();CHECK(marker==65 && g_esp==0x1804);
        WORD_AT(0x75fd88)=6;marker=0;const unsigned old_refreshes=reward_refreshes;
        g_esp=0x1800;story_rewards_page();CHECK(!marker && g_esp==0x1804);
        CHECK(WORD_AT(0x75fd88)==9 && reward_refreshes==old_refreshes+1);
        reward_wait_build=1;g_esp=0x1800;WORD_AT(g_esp+4)=0x4000;
        story_rewards_build();reward_wait_build=0;
        CHECK(g_esp==0x1804 && reward_wait_rows==1 && reward_wait_panels==1 && WORD_AT(0x4218)==1);
        marker=0;g_esp=0x1800;story_rewards_page();CHECK(!marker && g_esp==0x1804);
        g_esp=0x1800;story_rewards_tick();CHECK(!marker && g_esp==0x1804);
        CHECK(reward_refreshes==old_refreshes+1);
        reward_count=0;
        WORD_AT(0x76b140)=1;marker=0;g_esp=0x1800;story_rewards_build();CHECK(marker==64 && g_esp==0x1804);
        g_esp=0x1800;story_rewards_page();CHECK(marker==65 && g_esp==0x1804);
        g_esp=0x1800;story_rewards_tick();CHECK(marker==66 && g_esp==0x1804);
        WORD_AT(0x76b140)=2;
        marker=0;g_esp=0x1800;story_rewards_continue();CHECK(!marker && g_esp==0x1804);
        g_esp=0x1800;WORD_AT(g_esp+12)=0x20;WORD_AT(g_esp+20)=0x2eb0a0;
        CHECK(story_result_button(reward_row_original) && reward_rows==1 && g_esp==0x1804);
        CHECK(WORD_AT(0x180c)==0x20);
        WORD_AT(0x76b140)=1;g_esp=0x1800;story_rewards_continue();CHECK(marker==61 && g_esp==0x1804);
        marker=0;story_load_header[0]=2;story_load_header[1]=1;g_esp=0x1800;
        pc_story_network_result_action(sub_002EC650_gen,0u);CHECK(marker==63 && g_esp==0x1804);
        marker=0;WORD_AT(0x76b140)=2;g_esp=0x1800;
        pc_story_network_result_action(sub_002EC650_gen,0u);CHECK(!marker && g_esp==0x1804);
        WORD_AT(0x76b140)=1;story_load_header[0]=16;g_esp=0x1800;
        pc_story_network_result_action(sub_002EC650_gen,0u);CHECK(!marker && g_esp==0x1804);
        story_load_header[0]=2;marker=0;g_esp=0x1800;
        pc_story_network_result_action(sub_00065470_gen,0u);CHECK(marker==62 && g_esp==0x1804);
        CHECK(!story_rewards_exit_depth);
        marker=0;online_mode=PC_ONLINE_ARCADE;WORD_AT(0x76b140)=2;
        g_esp=0x1800;story_rewards_continue();CHECK(marker==61 && g_esp==0x1804);
        g_esp=0x1800;story_rewards_build();CHECK(marker==64 && g_esp==0x1804);
        g_esp=0x1800;story_rewards_page();CHECK(marker==65 && g_esp==0x1804);
        g_esp=0x1800;story_rewards_tick();CHECK(marker==66 && g_esp==0x1804);
    }
    /* In-progress joins arrive at the native phase12 pending-copy boundary.
       They must enter Story startup and adopt the host's prior reload epoch. */
    {
        online_mode=PC_ONLINE_STORY;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=2;
        WORD_AT(0x76b1a4)=12;WORD_AT(0x79094c)=101;WORD_AT(0x4d1ac6)=2;
        uint8_t *config=(uint8_t *)memory+0x78ad58;
        config[0]=2;config[1]=1;config[2]=0;config[0x54]=2;
        config[0x55]=10;config[0x56]=2;config[0x57]=0;WORD_AT(0x78adb0)=0x80008;
        g_esp=0x1800;WORD_AT(g_esp)=0x2d398d;
        CHECK(pc_story_network_prepare(0x78ad58) && story_pending_stage==1 && story_rejoin_pending);
        pc_story_network_activate();CHECK(story_rejoin_nonce && !story_reload_epoch);
        memcpy((uint8_t *)memory+0x7de400,config,0xa0);
        WORD_AT(0x79094c)=2;WORD_AT(0x76b1a4)=15;WORD_AT(0x774028)=0x30000;
        WORD_AT(0x30000)=1;story_rejoin_sent=0;g_esp=0x1800;
        const unsigned sent=npc_send_calls;
        story_rejoin_frame();CHECK(npc_send_calls==sent+1);
        StoryRejoin join;memcpy(&join,npc_sent+4,sizeof join);
        CHECK(join.magic==STORY_REJOIN_MAGIC && join.epoch==UINT32_MAX && join.peer==1);
        story_rejoin_frame();CHECK(npc_send_calls==sent+1); /* Bounded retry. */
        WORD_AT(0x76b140)=1;WORD_AT(0x30000)=0;WORD_AT(0x31000)=1;
        WORD_AT(0x77728c+0x124)=1;WORD_AT(0x777244+0x124)=0x31000;
        WORD_AT(0x7b0c48)=0x6000;
        const float anchor[3]={10.f,20.f,30.f},heading=1.25f;
        memcpy((uint8_t *)memory+0x60b4,anchor,sizeof anchor);memcpy((uint8_t *)memory+0x6100,&heading,4);
        story_reload_epoch=7;memcpy((uint8_t *)memory+0x190000,&join,sizeof join);
        CHECK(story_rejoin_receive(0x31000,0x190000,0x1e,sizeof join));
        memcpy(&join,npc_sent+4,sizeof join);CHECK(join.epoch==7 && join.position[0]==10.f && join.heading==heading);
        WORD_AT(0x76b140)=2;WORD_AT(0x30000)=1;WORD_AT(0x31000)=0;WORD_AT(0x774040)=0;
        story_reload_epoch=0;story_rejoin_pending=1;
        const unsigned nonce=join.nonce;join.nonce=nonce+1;
        memcpy((uint8_t *)memory+0x190000,&join,sizeof join);
        CHECK(story_rejoin_receive(0x31000,0x190000,0x1e,sizeof join));
        CHECK(story_rejoin_pending && !story_reload_epoch);
        join.nonce=nonce;memcpy((uint8_t *)memory+0x190000,&join,sizeof join);
        WORD_AT(0x31000)=2;story_rejoin_receive(0x31000,0x190000,0x1e,sizeof join);
        CHECK(story_rejoin_pending && !story_reload_epoch);
        WORD_AT(0x31000)=0;story_rejoin_receive(0x31000,0x190000,0x1e,sizeof join);
        CHECK(!story_rejoin_pending && story_reload_epoch==7 && story_context_start_pending);
        CHECK(story_rejoin_pose_valid && story_rejoin_position[2]==30.f);
        join.epoch=8;memcpy((uint8_t *)memory+0x190000,&join,sizeof join);
        story_rejoin_receive(0x31000,0x190000,0x1e,sizeof join);
        CHECK(story_reload_epoch==7); /* A completed bootstrap cannot be replayed. */
        /* After bootstrap, normal host reloads still require exactly epoch+1.
           Checkpoint metadata uses the adopted identity too. */
        const StoryCheckpoint checkpoint={STORY_CHECKPOINT_MAGIC,story_npc_match(),1,3};
        story_checkpoint_reset();memcpy((uint8_t *)memory+0x190000,&checkpoint,sizeof checkpoint);
        CHECK(story_checkpoint_receive(0x31000,0x190000,0x1e,sizeof checkpoint));
        CHECK(story_checkpoint_pending.serial==1);
        StoryRestart reload={STORY_RESTART_MAGIC,story_npc_match(),8,8};
        story_checkpoint_serial=1;WORD_AT(0x75f5d0)=0x130000;WORD_AT(0x130004)=8;
        memcpy((uint8_t *)memory+0x190000,&reload,sizeof reload);
        const unsigned prior_reloads=restart_calls;g_esp=0x1800;
        CHECK(story_restart_receive(0x31000,0x190000,0x1e,sizeof reload));
        CHECK(story_reload_epoch==8 && restart_calls==prior_reloads+1 && !story_rejoin_pose_valid);
        story_checkpoint_reset();
        story_reload_epoch=0;story_rejoin_pending=0;story_rejoin_pose_valid=0;
    }
    /* The exact network departure is not a Story death. Other death callers,
       local bodies and malformed/non-human contexts still execute the title. */
    {
        online_mode=PC_ONLINE_STORY;story_lobby_mission=0;story_pending_stage=1;
        WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=15;
        WORD_AT(0x79094c)=2;WORD_AT(0x7de458)=0x80008;
        ((uint8_t *)memory)[0x7de400]=2;WORD_AT(0x7b0c48)=0x6000;
        const uint32_t remote=0x6000+0x1584;
        WORD_AT(remote)=1;WORD_AT(remote+4)=UINT32_MAX;WORD_AT(remote+0x14)=0x90000;
        WORD_AT(0x90150)=remote;WORD_AT(0x5000)=0x90000;
        g_esp=0x1800;WORD_AT(g_esp)=0x35c199;WORD_AT(g_esp+4)=0x5000;
        unsigned old_calls=failure_original_calls;
        pc_story_network_departure_death(frame_original);
        CHECK(g_esp==0x1804 && failure_original_calls==old_calls && WORD_AT(remote+0x114)==16);
        WORD_AT(0x77728c+0x124)=0;CHECK(story_departed_context(remote));
        g_esp=0x1800;WORD_AT(g_esp+4)=remote;
        pc_story_failure_trace(frame_original,0xd50e0);
        CHECK(g_esp==0x1804 && failure_original_calls==old_calls);
        /* Checkpoint/level constructors may restore state/body directly. */
        WORD_AT(0x790950)=2;WORD_AT(remote+0x114)=2;WORD_AT(0x90028)=0x161;
        story_departed_frame();
        CHECK(WORD_AT(remote+0x114)==16 && WORD_AT(0x90028)==0x100 && retired_bodies==1);
        story_departed_frame();CHECK(retired_bodies==1); /* Stable retired body. */
        /* Missing departed inventory must not block the remaining peer. */
        WORD_AT(0x790950)=2;WORD_AT(0x6000)=0;WORD_AT(0x6004)=0;
        WORD_AT(0x6014)=0x91000;WORD_AT(0x91150)=0x6000;
        WORD_AT(0x9107c)=0x92000;WORD_AT(0x92000)=0x91000;WORD_AT(0x91028)=0;
        WORD_AT(remote+0x14)=0;
        g_esp=0x1800;pc_network_player_payload(frame_original);
        CHECK(failure_original_calls==old_calls+1);++old_calls;
        WORD_AT(0x77728c+0x124)=1;CHECK(!story_departed_context(remote));
        g_esp=0x1800;pc_network_player_payload(frame_original);
        CHECK(failure_original_calls==old_calls); /* Active missing body still blocks. */
        WORD_AT(remote+0x14)=0x90000;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x5000;WORD_AT(g_esp)=0xbac05;
        pc_story_network_departure_death(frame_original);
        CHECK(failure_original_calls==old_calls+1);
        g_esp=0x1800;WORD_AT(g_esp)=0x35c199;WORD_AT(remote+4)=0;
        pc_story_network_departure_death(frame_original);
        CHECK(failure_original_calls==old_calls+2);
        g_esp=0x1800;WORD_AT(remote+4)=UINT32_MAX;WORD_AT(0x90150)=0;
        pc_story_network_departure_death(frame_original);
        CHECK(failure_original_calls==old_calls+3);
        g_esp=0x1800;WORD_AT(0x90150)=remote;online_mode=PC_ONLINE_ARCADE;
        pc_story_network_departure_death(frame_original);
        CHECK(failure_original_calls==old_calls+4);
        online_mode=PC_ONLINE_STORY;
    }
    /* Game Over Quit uses the abort destination before its stock campaign
       scheduler, for either role and independently of the current menu port. */
    {
        online_mode=PC_ONLINE_STORY;story_lobby_mission=0;story_pending_stage=1;
        WORD_AT(0x7de458)=0x80008;WORD_AT(0x7de400)=0x102;
        WORD_AT(0x76b108)=1;WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;
        WORD_AT(0x761bbc)=0x4000;WORD_AT(0x761c48)=0x4500;
        WORD_AT(0x7ba9e0)=3;WORD_AT(0x7de45c)=0;
        for(unsigned role=1;role<=2;role++) {
            WORD_AT(0x76b140)=role;WORD_AT(0x774178)=1;
            story_context_start_pending=1;story_rewards_tracking=1;
            g_esp=0x1800;marker=0;
            pc_story_network_result_action(sub_002EAF60_gen,0u);
            CHECK(g_esp==0x1804 && !g_eax && marker==(role==1?62:74));
            CHECK(story_leave_destination==(role==1?1u:2u));
            CHECK(story_lobby_return_pending==(role==1));
            CHECK(WORD_AT(0x78ad20)==(role==1?13u:2u));
            CHECK(!story_context_start_pending && !story_rewards_tracking && !story_leave_exit_depth);
            CHECK(WORD_AT(0x774178)==(role==1?0u:1u));
            story_leave_destination=0;story_lobby_return_pending=0;
        }
        /* Successful result Continue, other modes and later stages stay stock. */
        WORD_AT(0x76b140)=1;WORD_AT(0x7de45c)=0x8000;
        g_esp=0x1800;marker=0;pc_story_network_result_action(sub_002EAF60_gen,0u);
        CHECK(marker==84 && g_esp==0x1804 && !story_leave_destination);
        WORD_AT(0x7de45c)=0;online_mode=PC_ONLINE_ARCADE;
        g_esp=0x1800;marker=0;pc_story_network_result_action(sub_002EAF60_gen,0u);
        CHECK(marker==84 && g_esp==0x1804 && !story_leave_destination);
        online_mode=PC_ONLINE_STORY;story_pending_stage=2;
        g_esp=0x1800;marker=0;pc_story_network_result_action(sub_002EAF60_gen,0u);
        CHECK(marker==84 && g_esp==0x1804 && !story_leave_destination);
    }
    /* Abort never invokes the original confirmed callback (which records
       completion). Host keeps transport, guest retires only its own session. */
    {
        online_mode=PC_ONLINE_STORY;story_lobby_mission=0;story_pending_stage=1;
        WORD_AT(0x7de458)=0x80008;WORD_AT(0x7de400)=0x102;story_load_header[0]=2;
        WORD_AT(0x76b108)=1;WORD_AT(0x76b1a4)=15;WORD_AT(0x79094c)=2;
        WORD_AT(0x76b140)=1;story_context_start_pending=1;
        WORD_AT(0x761bbc)=0x4000;WORD_AT(0x761c48)=0x4500;WORD_AT(0x774178)=1;
        g_esp=0x1800;marker=0;
        WORD_AT(g_esp+4)=0x4000;WORD_AT(0x418c)=0x41;story_leave_confirm();
        CHECK(marker==76 && g_esp==0x1804 && !story_leave_destination);
        g_esp=0x1800;
        WORD_AT(g_esp+4)=0x4000;WORD_AT(0x418c)=0x40;story_leave_confirm();
        CHECK(g_esp==0x1804 && marker==62 && story_leave_destination==1);
        CHECK(WORD_AT(0x78ad20)==13 && !story_context_start_pending && !WORD_AT(0x774178));
        CHECK(!next_menu && !story_leave_exit_depth);
        WORD_AT(0x76b140)=2;g_esp=0x1800;marker=0;
        pc_story_network_return_lobby(sub_002CBBA0);
        CHECK(marker==62 && g_esp==0x1804 && story_leave_destination==1);
        /* Stock frontend constructor must finish creating its root first. */
        WORD_AT(0x79094c)=101;WORD_AT(0x7613e4)=0x8000;
        g_esp=0x1800;WORD_AT(g_esp+4)=0;WORD_AT(g_esp+8)=0x789e80;
        pc_story_network_rewards_menu(reward_page_original);
        CHECK(story_leave_destination==1 && marker==65);
        g_esp=0x1800;WORD_AT(g_esp+4)=0x8000;WORD_AT(g_esp+8)=0x4e99d4;
        pc_story_network_rewards_menu(exit_factory_original);
        CHECK(exit_factory_target==0x4e99d4 && WORD_AT(0x76b1a4)==12 && !story_lobby_return_build);
        CHECK(!story_leave_destination && !story_pending_stage && online_mode==PC_ONLINE_STORY);
        CHECK(WORD_AT(0x1808)==0x4e99d4 && !leave_disconnects);
        story_pending_stage=1;WORD_AT(0x79094c)=2;WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=15;
        g_esp=0x1800;WORD_AT(g_esp+4)=0x4000;WORD_AT(0x418c)=0x40;story_leave_confirm();
        CHECK(marker==74 && g_esp==0x1804 && story_leave_destination==2 && WORD_AT(0x78ad20)==2);
        WORD_AT(0x79094c)=101;g_esp=0x1800;
        WORD_AT(g_esp+4)=0x8000;WORD_AT(g_esp+8)=0x5238f0;
        pc_story_network_rewards_menu(exit_factory_original);
        CHECK(exit_factory_target==0x524598);
        CHECK(!story_leave_destination && !story_pending_stage && leave_disconnects==1);
        CHECK(online_mode==PC_ONLINE_NONE && g_esp==0x1804 && WORD_AT(0x1808)==0x5238f0);
        /* Unplanned host loss uses the stock full disconnect scheduler. */
        online_mode=PC_ONLINE_STORY;story_pending_stage=1;WORD_AT(0x79094c)=2;
        g_esp=0x1800;pc_story_network_disconnect_exit(reward_page_original);
        CHECK(story_leave_destination==2 && WORD_AT(0x78ad20)==2 && marker==65 && g_esp==0x1804);
        story_leave_destination=0;online_mode=PC_ONLINE_ARCADE;WORD_AT(0x78ad20)=9;
        g_esp=0x1800;pc_story_network_disconnect_exit(reward_page_original);
        CHECK(!story_leave_destination && WORD_AT(0x78ad20)==9 && g_esp==0x1804);
        g_esp=0x1800;pc_story_network_return_lobby(reward_page_original);
        CHECK(marker==65 && g_esp==0x1804);
        /* The real original17 dispatcher must complete before guest Ready.
           Normal frontend17 outside this owned return stays unmodified. */
        online_mode=PC_ONLINE_STORY;WORD_AT(0x79094c)=101;WORD_AT(0x76b140)=2;
        WORD_AT(0x76b1a4)=17;story_lobby_return_pending=0;CHECK(!story_lobby_member());
        story_lobby_return_pending=1;CHECK(story_lobby_member());
        WORD_AT(0x4004)=0x4e99d4;WORD_AT(0x402c)=0x8000;
        WORD_AT(0x774028)=0x30000;((uint8_t *)memory)[0x302fa]=1;
        WORD_AT(0x6b94d8)=0;g_esp=0x1800;WORD_AT(g_esp+4)=0x4000;
        story_lobby_run_tick(reward_page_original,0x4e99d4);
        CHECK(lobby_return_dispatches==1 && lobby_return_resets==1 && WORD_AT(0x76b1a4)==12);
        CHECK(!story_lobby_return_pending && g_esp==0x1804);
    }
#endif
    puts("PASS: Story host controls, network-mode preservation and difficulty return");
    return 0;
}
