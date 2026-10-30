/* Shared ABI check: native fallback must not reference the Windows transport. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Select only the guest adapter after including the host's standard headers. */
#ifdef TSFP_TEST_NATIVE_FALLBACK
#undef _WIN32
#elif defined(TSFP_TEST_NATIVE_NETWORK)
#undef _WIN32
#define TSFP_NATIVE_NETWORK 1
#elif !defined(_WIN32)
#define _WIN32 1
#endif

static uint32_t memory[0x800000 / 4],g_eax,g_ecx,g_esp,g_ebx,g_edi,g_esi;
static ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
#include "../src/gameplay/pc_native_weapon_guest.h"
static int originals,enabled;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)

#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
static unsigned network_checks;
int pc_network_enabled(void) { network_checks++; return enabled; }
int pc_network_latency(double *milliseconds,unsigned *count)
{ *milliseconds=-1;*count=0;return 1; }
void pc_debug_latency(int mode,double milliseconds,unsigned count)
{ (void)mode;(void)milliseconds;(void)count; }
unsigned pc_network_arguments(uint32_t address) { return address==0x1234u?2u:0u; }
uint32_t pc_network_call(uint32_t address,const uint32_t *args,ptrdiff_t offset)
{
    return address==0x1234u && args[0]==17 && args[1]==29 && offset==g_xbox_mem_offset
        ? 0x55667788u : 0;
}
#endif

/* Exercise the simulated Windows transport branch on any C host without
   importing native settings-window headers; telemetry is stubbed above. */
#define TSFP_PC_DEBUG_H
#include "../src/network/pc_network_guest.inc"
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
static int bot_test_enabled;
static double bot_time=1.;
double pc_network_bots_time(void) {return bot_time;}
static const char *bot_test_getenv(const char *name)
{ return bot_test_enabled && !strcmp(name,"TSFP_NETWORK_BOTS")?"1":NULL; }
#define getenv bot_test_getenv
#endif
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
#include "../src/network/pc_network_bots_state.h"
uint32_t pc_network_bots_revive(unsigned slot);
void pc_network_bots_entity_refresh(uint32_t entity);
void pc_network_bots_present(uint32_t entity,const PcNetworkBotState *state);
void pc_network_bots_pose_weapons(uint32_t entity);
void pc_network_bots_present_shot(uint32_t entity,const PcNetworkBotShot *shot);
void pc_network_bots_request_hit(uint32_t context);
int pc_network_bots_kill_notice(unsigned victim,unsigned killer);
#endif
#include "../src/network/pc_network_bots_guest.inc"
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
#undef getenv
#endif

#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
static unsigned bot_sends,bot_refreshes,bot_shots;
static unsigned bot_hit_requests,bot_notices;
static unsigned bot_notice_victim,bot_notice_killer,bot_weapon_poses;
static int bot_notice_blocked;
void pc_network_bots_request_hit(uint32_t context) {(void)context;bot_hit_requests++;}
int pc_network_bots_kill_notice(unsigned victim,unsigned killer)
{ if(bot_notice_blocked)return 0;bot_notice_victim=victim;bot_notice_killer=killer;bot_notices++;return 1; }
void pc_network_bots_pose_weapons(uint32_t entity) {(void)entity;bot_weapon_poses++;}
static uint32_t bot_host_ip;
static PcNetworkBotState last_bot;
static int bot_incoming;
void pc_network_bots_context(unsigned role,uint32_t ip,uint32_t match,unsigned humans,unsigned bots)
{ bot_host_ip=ip;(void)role;(void)match;(void)humans;(void)bots; }
void pc_network_bots_send(const PcNetworkBotState *state) {last_bot=*state;bot_sends++;}
void pc_network_bots_targets(const uint32_t *addresses,unsigned count) {(void)addresses;(void)count;}
int pc_network_bots_receive(unsigned slot,PcNetworkBotState *state)
{ if(bot_incoming && slot==last_bot.slot) {*state=last_bot;bot_incoming=0;return 1;}return 0; }
uint32_t pc_network_bots_revive(unsigned slot) {memory[(0x1000+slot*0xea0)/4]=0x7000;return 0x7000;}
void pc_network_bots_entity_refresh(uint32_t entity)
{memset((uint8_t *)memory+entity+0x1a8,0,36);bot_refreshes++;}
void pc_network_bots_present(uint32_t entity,const PcNetworkBotState *state)
{(void)entity;(void)state;}
void pc_network_bots_present_shot(uint32_t entity,const PcNetworkBotShot *shot)
{(void)entity;(void)shot;bot_shots++;}
#endif
static void original(void) { originals++;g_eax=0x11223344u;g_esp+=4; }
static void original_firing(void)
{
    float delta;
    memcpy(&delta,(uint8_t *)memory+0x4e794c,4);
    assert(delta==(float)memory[0x4e7944/4]);
    g_eax=memory[0x4e7944/4]>0;g_esp+=4;
}
static void native_firing(void) { pc_native_weapon_fire_guest(original_firing); }
static int composed_firing(unsigned whole,float delta)
{
    float restored;
    memory[0x4e7944/4]=whole;
    memcpy((uint8_t *)memory+0x4e794c,&delta,4);
    g_tsfp_native_timing_active=1;g_esp=16;
    pc_network_bots_guest_shot(native_firing);
    memcpy(&restored,(uint8_t *)memory+0x4e794c,4);
    CHECK(g_esp==20 && memory[0x4e7944/4]==whole && restored==delta);
    g_tsfp_native_timing_active=0;
    return 0;
}
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
static uint8_t broadcast_config[PC_NETWORK_MATCH_BYTES];
static void original_broadcast(void)
{
    memcpy(broadcast_config,(uint8_t *)memory+memory[(g_esp+4)/4],sizeof broadcast_config);
    original();
}
static unsigned pose_advance;
static void original_pose(void) {pose_advance=memory[(g_esp+8)/4];original();}
#endif
static unsigned movement_sends;
static void original_pump(void)
{
    uint32_t epoch=memory[0x76B0F0/4];
    originals++;
    if ((int32_t)epoch>(int32_t)memory[0x76B0EC/4]) {
        movement_sends++;
        memory[0x76B0EC/4]=epoch;
    }
    g_eax=0x11223344u;g_esp+=4;
}

int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    g_esp=16;memory[5]=17;memory[6]=29;
    pc_network_guest(0x1234u,original);
    CHECK(originals==1 && g_eax==0x11223344u && g_esp==20);
    enabled=1;g_esp=16;
    pc_network_guest(0x1234u,original);
#ifdef TSFP_TEST_NATIVE_FALLBACK
    CHECK(originals==2 && g_eax==0x11223344u && g_esp==20);
    CHECK(!composed_firing(1,.416666667f) && g_eax==1);
    CHECK(!composed_firing(0,.416666667f) && g_eax==0);
    CHECK(!composed_firing(1,0) && g_eax==0);
    puts("PASS: native fallback retains original call/register/stack behavior without host networking");
#else
    CHECK(originals==1 && g_eax==0x55667788u && g_esp==28);
    puts("PASS: Windows transport dispatch and disabled fallback retain the guest ABI");
#endif
    {
        int before=originals;
        g_esp=16;pc_network_bots_guest_config(original);
        CHECK(originals==before+1 && g_eax==0x11223344u && g_esp==20);
    }
#ifdef TSFP_TEST_NATIVE_FALLBACK
    {
        void (*hooks[])(void (*)(void))={pc_network_bots_guest_lobby_send,
            pc_network_bots_guest_capacity,pc_network_bots_guest_start,
            pc_network_bots_guest_lobby_draw,pc_network_bots_guest_lobby_text};
        unsigned i;
        for(i=0;i<sizeof hooks/sizeof hooks[0];i++) {
            int before=originals;
            g_esp=16;hooks[i](original);
            CHECK(originals==before+1 && g_eax==0x11223344u && g_esp==20);
        }
    }
#endif
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    {
        /* Offline animation/combat calls must retain the original ABI without
           reading host network settings for every actor or bone update. */
        void (*hooks[])(void (*)(void))={pc_network_bots_guest_active,
            pc_network_bots_guest_pose,pc_network_bots_guest_bounds,
            pc_network_bots_guest_shot,pc_network_bots_guest_frame,
            pc_network_bots_guest_damage};
        uint8_t *config=(uint8_t *)memory+0x7de400;
        unsigned i;
        memset(memory,0,sizeof memory); network_checks=0;
        enabled=1; /* A LAN launch still has ordinary single-player callbacks. */
        config[0]=1;config[2]=1;memory[0x7356d8/4]=0x1000;
        for(i=0;i<sizeof hooks/sizeof hooks[0];i++) {
            int before=originals;
            memory[4]=0x64743;memory[5]=0x3000;memory[6]=1;
            g_esp=16;g_esi=0x3000;hooks[i](original);
            CHECK(originals==before+1 && g_eax==0x11223344u && g_esp==20);
            CHECK(memory[6]==1 && network_checks==0);
        }
        CHECK(!pc_network_bots_guest_actor(1) && network_checks==0);
        CHECK(!pc_network_bots_lobby_network() && network_checks==0);
        memory[0x76b108/4]=1;enabled=0;
        CHECK(!pc_network_bots_guest_actor(1) && network_checks==1);
        enabled=1;
        CHECK(pc_network_bots_guest_actor(1)==0x1ea0 && network_checks==2);
        memset(memory,0,sizeof memory);g_esi=0;
    }
    {
        uint8_t *config=(uint8_t *)memory+0x78ad58;
        PcNetworkBot *bot=(PcNetworkBot *)((uint8_t *)memory+0x75fd90);
        PcNetworkBot chosen={42,0,2,1,-1,0,-1};
        unsigned i;
        for(i=0;i<10;i++) bot[i]=chosen;
        config[1]=1;
        memory[0x77728c/4]=memory[(0x77728c+0x124)/4]=1;
        memory[0x76b108/4]=1;memory[0x76b140/4]=1;memory[0x76b1a4/4]=11;
        memory[0x78ad2c/4]=13;memory[0x78ad4c/4]=2;bot_test_enabled=1;
        memory[(0x4d239c+(2*15+3)*4)/4]=8;
        memory[(0x4d239c+(2*15+2)*4)/4]=12;
        memory[(0x4d239c+2*15*4)/4]=14;
        CHECK(pc_network_bots_lobby_limit()==8);
        memory[0x78ad2c/4]=11;CHECK(pc_network_bots_lobby_limit()==12);
        memory[0x78ad2c/4]=12;CHECK(pc_network_bots_lobby_limit()==14);
        memory[0x78ad2c/4]=13;
        g_esp=16;pc_network_bots_guest_lobby_send(original);
        CHECK(config[0]==2 && config[2]==6 && g_esp==20);
        CHECK(pc_network_bots_lobby_prepare(0).waiting==4);
        g_esp=16;pc_network_bots_guest_capacity(original);
        CHECK(g_eax==8 && g_esp==20);
        /* Before measuring the stock label, include the authoritative bots. */
        memory[4]=0x197707;memory[5]=0x2000;pc_network_bots_lobby_drawing=1;
        g_esp=16;pc_network_bots_guest_lobby_text(original);
        CHECK(!strcmp((char *)memory+0x2000,"2 players + 6 bots / 8 (4 waiting)"));
        memory[0x76b140/4]=2;
        g_esp=16;pc_network_bots_guest_lobby_text(original);
        CHECK(!strcmp((char *)memory+0x2000,"2 players + 6 bots / 8"));
        memory[4]=0x123456;strcpy((char *)memory+0x2000,"Unrelated");
        g_esp=16;pc_network_bots_guest_lobby_text(original);
        CHECK(!strcmp((char *)memory+0x2000,"Unrelated"));
        memory[0x76b140/4]=1;
        for(i=2;i<8;i++) memory[(0x77728c+i*0x124)/4]=1;
        g_esp=16;pc_network_bots_guest_lobby_send(original);
        CHECK(config[0]==8 && config[2]==0);
        memory[(0x77728c+8*0x124)/4]=1;
        i=originals;g_esp=16;pc_network_bots_guest_start(original);
        CHECK(originals==i && g_esp==20); /* Mode shrank below human roster. */
        memory[(0x77728c+8*0x124)/4]=0;
        g_esp=16;pc_network_bots_guest_start(original);
        CHECK(originals==i+1 && g_esp==20);
        memory[(0x77728c+7*0x124)/4]=0;
        g_esp=16;pc_network_bots_guest_lobby_send(original);
        CHECK(config[2]==1 && bot[9].enabled==1);
        memory[0x78ad4c/4]=17;CHECK(pc_network_bots_lobby_limit()==0);
        memset(memory,0,sizeof memory);pc_network_bots_lobby_drawing=0;
    }
    {
        uint8_t *config=(uint8_t *)memory+0x78ad58;
        PcNetworkBot *bot=(PcNetworkBot *)((uint8_t *)memory+0x75fd90);
        PcNetworkBot chosen={42,0,2,1,-1,0,-1};
        memory[(0x4d239c+3*4)/4]=16;
        bot[0]=chosen;config[0]=16;config[1]=1;config[2]=0;
        memory[0x77728c/4]=1;memory[0x76b108/4]=1;
        memory[0x76b140/4]=1;memory[0x78ad2c/4]=13;bot_test_enabled=1;
        memory[5]=0x78ad58;g_esp=16;pc_network_bots_guest_config(original);
        CHECK(config[0]==1 && config[2]==1 && g_eax==0x11223344u && g_esp==20);
        config[0]=16;config[2]=0;memory[5]=0x774044;
        g_esp=16;pc_network_bots_guest_config(original);
        CHECK(config[0]==16 && config[2]==0 && g_eax==0x11223344u && g_esp==20);
        memory[5]=0x78ad58;memory[0x76b140/4]=2;
        g_esp=16;pc_network_bots_guest_config(original);
        CHECK(config[0]==16 && config[2]==0 && g_eax==0x11223344u && g_esp==20);
        /* Reproduce the real caller: its post-builder zero must be repaired
           BEFORE serialization, and again before the local pending-load copy.
           The receiver uses the host's bytes without rebuilding its own roster. */
        memory[0x76b140/4]=1;memory[5]=0x78ad58;
        config[0]=16;config[2]=0;
        g_esp=16;pc_network_bots_guest_config(original_broadcast);
        CHECK(broadcast_config[0]==1 && broadcast_config[2]==1 && broadcast_config[0x15]==42);
        CHECK(g_eax==0x11223344u && g_esp==20);
        config[2]=0;g_esp=16;
        pc_network_bots_guest_config(original_broadcast);
        CHECK(broadcast_config[2]==1);
        bot_test_enabled=0;
    }
#endif
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    {
        uint8_t *live=(uint8_t *)memory+0x7de400;
        live[0]=2;live[2]=6;memory[0x7356d8/4]=0x1000;
        memory[0x76b108/4]=1;
        for(unsigned slot=0;slot<17;slot++) {
            int before=originals;g_esp=16;memory[4]=0x13e0bb;memory[5]=slot;
            pc_network_bots_guest_active(original);
            if(slot>=2 && slot<8) CHECK(originals==before && g_eax==1 && g_esp==20);
            else CHECK(originals==before+1 && g_eax==0x11223344u && g_esp==20);
        }
        {
            const uint32_t callers[]={0x133bc9,0x64743,0x64856,0x64a06,0x6562d,0x13f82a,0x2e72b6,0x2e744b};
            for(unsigned i=0;i<sizeof callers/sizeof *callers;i++) {
                g_esp=16;memory[4]=callers[i];memory[5]=2;
                pc_network_bots_guest_active(original);CHECK(g_eax==1 && g_esp==20);
            }
        }
        for(unsigned i=0;i<3;i++) {
            g_esp=16;memory[4]=i==0?0x6486e:i==1?0x64b18:0x64ba5;
            g_ebx=g_edi=2;pc_network_bots_guest_score_network(original);
            /* Score eligibility survives a dead/missing body. */
            CHECK(g_eax==0 && g_esp==20);
        }
        /* Results skip controller membership only for configured AI slots.
           Preserve human/disconnected slots and the later offset-copy call. */
        for(unsigned slot=0;slot<17;slot++) {
            g_esp=16;memory[4]=0x63f05;g_esi=slot;
            pc_network_bots_guest_score_network(original);
            CHECK(g_eax==(slot>=2 && slot<8?0u:0x11223344u) && g_esp==20);
        }
        g_esp=16;memory[4]=0x63f93;g_esi=2;
        pc_network_bots_guest_score_network(original);
        CHECK(g_eax==0x11223344u && g_esp==20);
        for(unsigned slot=0;slot<17;slot++) {
            g_esp=16;memory[4]=0x67868;g_edi=slot;g_esi=0xffffffffu;
            pc_network_bots_guest_score_network(original);
            CHECK(g_eax==(slot>=2 && slot<8?0u:0x11223344u) && g_esp==20);
        }
        for(unsigned guard=0;guard<4;guard++) {
            g_esp=16;memory[4]=0x67868;g_edi=2;
            enabled=guard!=0;memory[0x76b108/4]=guard!=1;
            memory[0x7356d8/4]=guard==2?0:0x1000;live[2]=guard==3?0:6;
            pc_network_bots_guest_score_network(original);
            CHECK(g_eax==0x11223344u && g_esp==20);
        }
        enabled=1;memory[0x76b108/4]=1;memory[0x7356d8/4]=0x1000;live[2]=6;
        /* In particular, do not turn AI into local controller records. */
        g_esp=16;memory[4]=0x15c5a3;memory[5]=2;
        pc_network_bots_guest_active(original);CHECK(g_eax==0x11223344u && g_esp==20);
        g_esp=16;memory[4]=0x13e0bb;enabled=0;
        pc_network_bots_guest_active(original);CHECK(g_eax==0x11223344u && g_esp==20);
        enabled=1;memory[0x7356d8/4]=0;g_esp=16;
        pc_network_bots_guest_active(original);CHECK(g_eax==0x11223344u && g_esp==20);
    }
#else
    g_esp=16;pc_network_bots_guest_active(original);
    CHECK(g_eax==0x11223344u && g_esp==20);
#endif
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    {
        uint8_t *live=(uint8_t *)memory+0x7de400;
        live[0]=2;live[2]=6;memory[0x7356d8/4]=0x1000;
        memory[(0x1000+2*0xea0)/4]=0x7000;
        memory[0x76b108/4]=1;
        g_esp=16;memory[5]=0x7000;pc_network_bots_guest_serializable(original);
        CHECK(g_eax==1 && g_esp==20);
        g_esp=16;memory[5]=0x7100;pc_network_bots_guest_serializable(original);
        CHECK(g_eax==0x11223344u && g_esp==20);
        g_esp=16;g_eax=0x200;g_ecx=0x7000;memory[0x200/4]=0x300;
        pc_network_bots_guest_encode(original);
        CHECK(memory[0x200/4]==0x302 && ((uint8_t *)memory)[0x300]==4 && ((uint8_t *)memory)[0x301]==2 && g_esp==20);
        g_esp=16;g_ecx=0x200;memory[0x200/4]=0x300;
        pc_network_bots_guest_decode(original);
        CHECK(g_eax==0x7000 && g_esp==20 && memory[0x200/4]==0x302);
        ((uint8_t *)memory)[0x301]=16;memory[0x200/4]=0x300;g_esp=16;
        pc_network_bots_guest_decode(original);
        CHECK(g_eax==0 && g_esp==20 && memory[0x200/4]==0x302);
        ((uint8_t *)memory)[0x301]=1;memory[0x200/4]=0x300;g_esp=16;
        pc_network_bots_guest_decode(original);CHECK(g_eax==0 && g_esp==20);
        ((uint8_t *)memory)[0x300]=1;memory[0x200/4]=0x300;g_esp=16;
        pc_network_bots_guest_decode(original);CHECK(g_eax==0x11223344u && g_esp==20);
    }
#else
    g_esp=16;pc_network_bots_guest_serializable(original);CHECK(g_eax==0x11223344u && g_esp==20);
    g_esp=16;pc_network_bots_guest_encode(original);CHECK(g_eax==0x11223344u && g_esp==20);
    g_esp=16;pc_network_bots_guest_decode(original);CHECK(g_eax==0x11223344u && g_esp==20);
#endif
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    {
        uint8_t *live=(uint8_t *)memory+0x7de400;
        uint8_t *actor=(uint8_t *)memory+0x1000+2*0xea0;
        float health=1.f;uint32_t life=8;
        live[0]=2;live[2]=1;memory[0x76b140/4]=1;memory[0x76b1a4/4]=15;
        memory[0x7356d8/4]=0x1000;memory[(0x1000+2*0xea0)/4]=0x7000;
        memory[(0x7000+0x7c)/4]=0x1000+2*0xea0;
        memory[(0x1000+2*0xea0+4)/4]=0x7200;memory[0x1000/4]=0x7200;
        memory[(0x1000+2*0xea0+0xd4)/4]=7;
        memory[(0x7000+0x258)/4]=7;
        g_esp=16;memory[5]=0x7000;memory[6]=0;memory[7]=0;memory[8]=1;
        pc_network_bots_guest_shot(original);
        CHECK(g_eax==0x11223344u && g_esp==20 && pc_network_bot_shots[2].serial==1);
        CHECK(pc_network_bot_shots[2].weapon==7 && pc_network_bot_shots[2].target_slot==0);
        CHECK(!composed_firing(1,.416666667f) && g_eax==1);
        CHECK(pc_network_bot_shots[2].serial==2);
        CHECK(!composed_firing(0,.416666667f) && g_eax==0);
        CHECK(pc_network_bot_shots[2].serial==2);
        CHECK(!composed_firing(1,0) && g_eax==0);
        CHECK(pc_network_bot_shots[2].serial==2);
        memcpy((uint8_t *)memory+0x71cc,&health,4);memcpy(actor+0x18,&life,4);
        memory[(0x7000+0x28)/4]=0;g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(g_eax==0x11223344u && g_esp==20 && bot_sends==1 && bot_refreshes==1);
        CHECK((memory[(0x7000+0x28)/4]&0x61u)==0x61u && last_bot.slot==2);
        memory[(0x7000+0x28)/4]=0;life=2;memcpy(actor+0x18,&life,4);
        memory[0x7de318/4]+=3;g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(!(memory[(0x7000+0x28)/4]&1));
        life=8;memcpy(actor+0x18,&life,4);memory[(0x1000+2*0xea0+0x2c)/4]=4;
        g_esp=16;pc_network_bots_guest_frame(original);CHECK(!(memory[(0x7000+0x28)/4]&1));
        memory[0x76b140/4]=2;last_bot.slot=2;last_bot.position[0]=25.f;
        memory[(0x1000+2*0xea0+0x2c)/4]=0x10;
        last_bot.body_present=1;last_bot.spawn=7;last_bot.heading=37.f;last_bot.muzzle[0]=26.f;last_bot.bounds_radius[0]=.5f;last_bot.health=.75f;last_bot.max_health=1.1f;last_bot.entity_flags=0x71u;last_bot.flags[0]=4;
        memory[(0x7000+0x150)/4]=0x1234;bot_incoming=1;
        memory[0x774040/4]=3;
        memory[(0x66648c+3*16)/4]=0x0200007fu;
        memory[(0x774184+3*0x30c)/4]=99; /* connection handle is not an IP */
        g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(bot_host_ip==0x0200007fu);
        CHECK(memory[(0x1000+2*0xea0+0x2c)/4]==(last_bot.flags[2]|0x10));
        /* Received poses evaluate at the normal frame boundary, without
           advancing the host's clock; humans and host retain their path. */
        memory[5]=0x7000;memory[6]=1;g_esp=16;
        pc_network_bots_guest_pose(original_pose);
        CHECK(pose_advance==0 && memory[6]==1 && bot_weapon_poses==1);
        CHECK(g_esp==20 && g_eax==0x11223344u);
        memory[5]=0x7200;g_esp=16;pc_network_bots_guest_pose(original_pose);
        CHECK(pose_advance==1 && bot_weapon_poses==1);
        memory[5]=0x7000;memory[0x76b140/4]=1;g_esp=16;
        pc_network_bots_guest_pose(original_pose);
        CHECK(pose_advance==1 && bot_weapon_poses==1);memory[0x76b140/4]=2;
        CHECK(memory[(0x7000+0x258)/4]==7);
        CHECK(!memcmp((uint8_t *)memory+0x7070,&last_bot.heading,4));
        CHECK(!memcmp(actor+0xbb4,last_bot.muzzle,12));
        CHECK(!memcmp((uint8_t *)memory+0x71c0,last_bot.bounds_radius,12));
        memset((uint8_t *)memory+0x71c0,0,12);g_esi=0x7000;g_esp=16;
        pc_network_bots_guest_bounds(original);
        CHECK(g_eax==0x11223344u && g_esp==20 && g_esi==0x7000);
        CHECK(!memcmp((uint8_t *)memory+0x71c0,last_bot.bounds_radius,12));
        /* The cache belongs to one life/body, never another respawn. */
        memory[(0x7000+0x258)/4]=8;memset((uint8_t *)memory+0x71c0,0,12);g_esp=16;
        pc_network_bots_guest_bounds(original);CHECK(memory[0x71c0/4]==0);
        memory[(0x7000+0x258)/4]=7;
        memory[0x76b140/4]=1;g_esp=16;pc_network_bots_guest_bounds(original);
        CHECK(memory[0x71c0/4]==0);memory[0x76b140/4]=2;
        CHECK(g_eax==0x11223344u && g_esp==20 && bot_refreshes==2);
        CHECK(!memcmp((uint8_t *)memory+0x7064,last_bot.position,12));
        CHECK(!memcmp((uint8_t *)memory+0x71cc,&last_bot.health,4) && !memcmp((uint8_t *)memory+0x71d8,&last_bot.max_health,4));
        CHECK(memory[(0x7000+0x150)/4]==0x1234 && memory[(0x1000+2*0xea0)/4]==0x7000);
        CHECK((memory[(0x7000+0x28)/4]&0x61u)==0x61u && memory[(0x1000+2*0xea0+0x24)/4]==4);
        CHECK(bot_shots==1);
        bot_incoming=1;g_esp=16;pc_network_bots_guest_frame(original);CHECK(bot_shots==1);
        /* Cosmetic shots cannot apply local NPC damage; incoming host events
           and human-sourced attacks still go through the original function. */
        memory[5]=0x400;memory[(0x400+4)/4]=0x7000;memory[0x76b170/4]=0;
        g_esp=16;pc_network_bots_guest_damage(original);CHECK(g_eax==0 && g_esp==20);
        memory[(0x400+4)/4]=0x7400;memory[(0x7400+0x20)/4]=8;
        memory[(0x7400+0x7c)/4]=0x1000+2*0xea0;g_esp=16;
        pc_network_bots_guest_damage(original);CHECK(g_eax==0 && g_esp==20);
        memory[0x76b170/4]=1;g_esp=16;pc_network_bots_guest_damage(original);
        CHECK(g_eax==0x11223344u && g_esp==20);
        memory[0x76b170/4]=0;memory[(0x400+4)/4]=0x7200;g_esp=16;pc_network_bots_guest_damage(original);
        CHECK(g_eax==0x11223344u && g_esp==20);
        memory[(0x1000+2*0xea0)/4]=0;bot_incoming=1;
        g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(memory[(0x1000+2*0xea0)/4]==0x7000 && bot_refreshes==4);
        for(unsigned i=0;i<3;i++) {
            const unsigned calls=originals;
            g_esp=16;memory[4]=i==0?0x6486e:i==1?0x64b18:0x64ba5;
            g_ebx=g_edi=2;pc_network_bots_guest_score_network(original);
            CHECK(g_eax==0 && g_esp==20 && originals==calls);
            g_esp=16;g_ebx=g_edi=1;pc_network_bots_guest_score_network(original);
            CHECK(g_eax==0x11223344u && g_esp==20 && originals==calls+1);
        }
        g_esp=16;memory[4]=0xd9005;g_ebx=g_edi=2;
        pc_network_bots_guest_score_network(original);
        CHECK(g_eax==0x11223344u && g_esp==20);
        /* A guest human's lethal hit is a request, never a speculative death.
           The echoed damage event must not subtract health a second time. */
        memory[0x400/4]=0x7000;memory[5]=0x400;memory[(0x400+4)/4]=0x7200;
        {
            const unsigned before=originals;
            g_esp=16;pc_network_bots_guest_damage(original);
            CHECK(bot_hit_requests==1 && originals==before && g_eax==0 && g_esp==20);
            CHECK(!memcmp((uint8_t *)memory+0x71cc,&last_bot.health,4));
            memory[0x76b170/4]=1;g_esp=16;pc_network_bots_guest_damage(original);
            CHECK(bot_hit_requests==1 && originals==before);
            memory[0x76b170/4]=0;
            g_esp=16;pc_network_bots_guest_death_gate(original);
            CHECK(g_eax==0 && originals==before && g_esp==20);
            memory[0x76b140/4]=1;g_esp=16;pc_network_bots_guest_death_gate(original);
            CHECK(g_eax==1 && originals==before && g_esp==20);
            /* The same remote shooter is allowed to finish the HOST NPC death.
               Human victims and native/offline behavior retain the original. */
            memory[0x400/4]=0x7200;g_esp=16;pc_network_bots_guest_death_gate(original);
            CHECK(g_eax==0x11223344u && originals==before+1 && g_esp==20);
            memory[0x76b140/4]=2;
        }
        /* Lost intermediate death snapshots still converge: the host's score
           column and missing-body tombstone clear a stale live replica. */
        last_bot.body_present=0;last_bot.killed_by[1]=1;bot_incoming=1;
        g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(bot_notices==1 && !(memory[(0x7000+0x28)/4]&0x71u));
        CHECK(*(int16_t *)((uint8_t *)memory+0x1000+0xea0+0xa10+4)==1);
        bot_incoming=1;g_esp=16;pc_network_bots_guest_frame(original);CHECK(bot_notices==1);
        last_bot.body_present=1;last_bot.spawn++;last_bot.entity_flags=0x71;bot_incoming=1;
        g_esp=16;pc_network_bots_guest_frame(original);
        CHECK((memory[(0x7000+0x28)/4]&0x71u)==0x71u && bot_notices==1);
        CHECK(!memcmp((uint8_t *)memory+0x71cc,&last_bot.health,4));
        last_bot.position[0]=NAN;bot_incoming=1;
        /* An invalid packet must not replace the valid cached host state;
           presentation still refreshes that state on every guest frame. */
        { float before;memcpy(&before,(uint8_t *)memory+0x7064,4);
          g_esp=16;pc_network_bots_guest_frame(original);CHECK(bot_refreshes==6);
          CHECK(!memcmp(&before,(uint8_t *)memory+0x7064,4)); }
        memory[0x76b140/4]=1;memory[(0x1000+2*0xea0)/4]=0;
        memory[0x7de318/4]+=3;g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(last_bot.body_present==0 && last_bot.killed_by[1]==1);
        CHECK(pc_network_bot_state_valid(&last_bot,2,1));
        /* Confirmations can arrive after corpse removal or while the local
           view is absent. Retry without duplicates, including a skipped packet
           that contains more than one newly confirmed kill. */
        last_bot.killed_by[1]=3;bot_incoming=1;bot_notice_blocked=1;
        memory[0x76b140/4]=2;g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(bot_notices==1);
        bot_notice_blocked=0;bot_incoming=1;g_esp=16;pc_network_bots_guest_frame(original);
        CHECK(bot_notices==3 && bot_notice_victim==2 && bot_notice_killer==1);
        bot_incoming=1;g_esp=16;pc_network_bots_guest_frame(original);CHECK(bot_notices==3);

        /* Exercise the actual guest hook between fresh packets, including its
           body/actor write and collision-bound refresh, not only the helper. */
        last_bot.body_present=1;last_bot.health=100;last_bot.entity_flags=0x71;
        last_bot.spawn=100;last_bot.position[0]=0;last_bot.bounds_center[0]=10;
        bot_incoming=1;g_esp=16;pc_network_bots_guest_frame(original);
        bot_time=1.05;last_bot.position[0]=3;last_bot.bounds_center[0]=13;
        bot_incoming=1;g_esp=16;pc_network_bots_guest_frame(original);
        { float x;memcpy(&x,(uint8_t *)memory+0x7064,4);CHECK(x==0); }
        bot_time=1.075;g_esp=16;pc_network_bots_guest_frame(original);
        { float x,actor_x,center;
          memcpy(&x,(uint8_t *)memory+0x7064,4);
          memcpy(&actor_x,(uint8_t *)memory+0x1000+2*0xea0+0x30,4);
          memcpy(&center,(uint8_t *)memory+0x71a8,4);
          CHECK(fabsf(x-1.5f)<.001f && x==actor_x && fabsf(center-11.5f)<.001f);
          CHECK(!bot_incoming && bot_notices==3); }

    }
#else
    g_esp=16;pc_network_bots_guest_frame(original);CHECK(g_eax==0x11223344u && g_esp==20);
    g_esp=16;g_esi=0x7000;pc_network_bots_guest_bounds(original);
    CHECK(g_eax==0x11223344u && g_esp==20 && g_esi==0x7000);
    g_esp=16;pc_network_bots_guest_death_gate(original);CHECK(g_eax==0x11223344u && g_esp==20);
    g_esp=16;pc_network_bots_guest_pose(original);CHECK(g_eax==0x11223344u && g_esp==20);
#endif
    /* Same epoch, different guest frames: movement must be sampled immediately
       rather than waiting seven ticks. Duplicate pump calls must not resend. */
    memory[0x76B1A4/4]=15;memory[0x76B0F0/4]=20;memory[0x76B0EC/4]=20;
    for(unsigned frame=140;frame<147;frame++) {
        memory[0x7497A8/4]=frame;g_esp=16;
        pc_network_guest_tick(original_pump);
        CHECK(g_eax==0x11223344u && g_esp==20);
        CHECK(memory[0x76B0F0/4]==20 && memory[0x7497A8/4]==frame);
        pc_network_guest_tick(original_pump);
    }
#ifdef TSFP_TEST_NATIVE_FALLBACK
    CHECK(movement_sends==0);
#else
    CHECK(movement_sends==7);
#endif
    /* No override for offline play or any lobby/loading/disconnect state. */
    for(unsigned state=0;state<18;state++) {
        if(state==15)continue;
        memory[0x76B1A4/4]=state;memory[0x76B0EC/4]=20;
        memory[0x7497A8/4]++;pc_network_guest_tick(original_pump);
        CHECK(memory[0x76B0EC/4]==20);
    }
    enabled=0;memory[0x76B1A4/4]=15;memory[0x7497A8/4]++;
    pc_network_guest_tick(original_pump);CHECK(memory[0x76B0EC/4]==20);
    /* Held keys and mouse-only look must not leave full movement state stale.
       Three ticks bound drift, including clock resets and a sender stall. */
    enabled=1;memory[0x76B1A4/4]=15;memory[5]=0x1000;
    memory[0x1020/4]=4;
    for(unsigned i=0;i<4;i++) memory[(0x100C+i*4)/4]=0x2000+i*0x200;
    for(unsigned age=0;age<9;age++) {
        memory[0x7DE318/4]=100+age;
        for(unsigned i=0;i<4;i++) {
            memory[(0x2100+i*0x200)/4]=0;
            memory[(0x210C+i*0x200)/4]=100;
        }
        g_esp=16;pc_network_guest_sample(original);
        CHECK(g_eax==0x11223344u && g_esp==20);
        for(unsigned i=0;i<4;i++) {
#ifdef TSFP_TEST_NATIVE_FALLBACK
            CHECK(memory[(0x2100+i*0x200)/4]==0);
#else
            CHECK(memory[(0x2100+i*0x200)/4]==(age>=3));
#endif
            CHECK(memory[(0x210C+i*0x200)/4]==100);
        }
    }
    enabled=0;g_esp=16;memory[0x2100/4]=0;
    pc_network_guest_sample(original);CHECK(memory[0x2100/4]==0);
    enabled=1;memory[0x76B1A4/4]=14;g_esp=16;
    pc_network_guest_sample(original);CHECK(memory[0x2100/4]==0);
    memory[0x76B1A4/4]=15;memory[0x7DE318/4]=90;g_esp=16;
    pc_network_guest_sample(original);
#ifndef TSFP_TEST_NATIVE_FALLBACK
    CHECK(memory[0x2100/4]==1);
#endif
    puts("PASS: movement gate, duplicate calls, offline/native fallback and unchanged guest clocks");
    return 0;
}
