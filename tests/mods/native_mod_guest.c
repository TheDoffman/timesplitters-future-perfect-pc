/* Shared title adapter with controlled guest memory and the real isolated match
   parent. Stock functions below are mocks, not original-routine execution. */
#include "pc_mod_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <time.h>
#endif
static unsigned char *memory;
static uintptr_t g_xbox_mem_offset;
static uint32_t g_eax,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_ecx,g_edx;
#define WORD_AT(a) (*(uint32_t *)(memory+(uint32_t)(a)))
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Guest check %d: %s\n",__LINE__,#x);return 3;}}while(0)
typedef struct GuestRegisters {uint32_t values[8];} GuestRegisters;
static void save_registers(GuestRegisters *r){uint32_t values[]={g_eax,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_ecx,g_edx};memcpy(r->values,values,sizeof(values));}
static void restore_registers(const GuestRegisters *r){g_eax=r->values[0];g_esp=r->values[1];g_ebx=r->values[2];g_esi=r->values[3];g_edi=r->values[4];g_ebp=r->values[5];g_ecx=r->values[6];g_edx=r->values[7];}
static int guest_writable(uint32_t address,size_t bytes){return address>=0x1000 && address<=0x800000 && bytes<=0x800000-address;}
static unsigned finished,original_scores,original_limits,original_frames;
static uint32_t guest_call(void (*function)(void),unsigned count,uint32_t arg) {
    uint32_t saved=g_esp;g_esp-=4;WORD_AT(g_esp)=arg;g_esp-=4;WORD_AT(g_esp)=0x12345678;
    function();g_esp+=count*4;if(g_esp!=saved)abort();return g_eax;
}
static void stock_score(void){++original_scores;uint32_t i=WORD_AT(g_esp+4);g_eax=WORD_AT(0x10000+i*0xea0+0xa6c);g_esp+=4;}
static void stock_ranking(void){++original_scores;for(unsigned i=0;i<2;++i){WORD_AT(0x10000+i*0xea0+0xa6c)=17;WORD_AT(0x10000+i*0xea0+0xa68)=1;}g_esp+=4;}
static void stock_limits(void){++original_limits;g_esp+=4;}
static void stock_frame(void){++original_frames;++WORD_AT(0x7de318);g_eax=0x45;g_esp+=4;}
static void stock_death(void){uint32_t hit=WORD_AT(g_esp+4),victim=WORD_AT(hit);++WORD_AT(victim+0x258);WORD_AT(victim+0x1cc)=0;g_esp+=4;}
void sub_00064810(void){pc_mod_game_ranking(stock_ranking);}
void sub_00067450(void){++finished;WORD_AT(0x7de30c)=8;g_eax=0x9911;g_esp+=4;}
#include "pc_mod_guest.inc"
static void pause_short(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t={0,1000000};nanosleep(&t,NULL);
#endif
}
static void initialize(void) {
    memset(memory,0,0x800000);WORD_AT(0x7de30c)=7;WORD_AT(0x79094c)=1;WORD_AT(0x7356d8)=0x10000;
    WORD_AT(0x7de454)=9u<<8;WORD_AT(0x790950)=1;WORD_AT(0x7a2958)=1;WORD_AT(0x4e8cec)=60;
    for(unsigned i=0;i<2;++i){uint32_t actor=0x10000+i*0xea0,body=0x20000+i*0x1000;
        WORD_AT(actor)=body;WORD_AT(actor+8)=i;WORD_AT(actor+0x70)=0x30000+i*0x100;
        WORD_AT(body+0x7c)=actor;float health=100.f;memcpy(memory+body+0x1cc,&health,4);}
    g_esp=0x70000;g_ebx=11;g_esi=22;g_edi=33;g_ebp=44;g_ecx=55;g_edx=66;
}
static int run(const char *worker,const char *cache,const char *scenario,const char *worker_hash,const char *guardian_hash) {
    memory=calloc(1,0x800000);CHECK(memory);g_xbox_mem_offset=(uintptr_t)memory;initialize();
    TsfpModWorld world;CHECK(pc_mod_game_capture(&world));uint64_t epoch=world.epoch;
    CHECK(world.player_count==2 && world.players[0].health==100);
    if(!strcmp(scenario,"guards")) {
        WORD_AT(0x10000+0x84)=UINT32_MAX;
        CHECK(pc_mod_game_capture(&world) && world.players[0].team==0);
        WORD_AT(0x76b108)=1;CHECK(!pc_mod_game_capture(&world));WORD_AT(0x76b108)=0;
        WORD_AT(0x7de458)=1;CHECK(!pc_mod_game_capture(&world));WORD_AT(0x7de458)=0;
        WORD_AT(0x7de454)=10u<<8;CHECK(!pc_mod_game_capture(&world));WORD_AT(0x7de454)=9u<<8;
        WORD_AT(0x7356d8)=0xfffffff0;CHECK(!pc_mod_game_capture(&world));WORD_AT(0x7356d8)=0x10000;
        CHECK(pc_mod_game_capture(&world) && world.epoch==epoch);
        WORD_AT(0x7de30c)=8;CHECK(pc_mod_game_capture(&world) && world.epoch==epoch);
        WORD_AT(0x7de30c)=7;CHECK(pc_mod_game_capture(&world) && world.epoch>epoch);epoch=world.epoch;
        WORD_AT(0x10000+8)=4;CHECK(!pc_mod_game_capture(&world));WORD_AT(0x10000+8)=0;
        CHECK(pc_mod_game_capture(&world) && world.epoch==epoch);
        WORD_AT(0x7de30c)=6;CHECK(!pc_mod_game_capture(&world));WORD_AT(0x7de30c)=7;
        CHECK(pc_mod_game_capture(&world) && world.epoch>epoch);free(memory);puts("PASS guards");return 0;
    }
    static char source[1048577];size_t size=fread(source,1,sizeof(source),stdin);CHECK(size && size<=1048576);
    TsfpModMatchConfig config={worker,cache,source,size,123,15,worker_hash,strcmp(guardian_hash,"-")?guardian_hash:""};TsfpModMatch *match=tsfp_mod_match_open(&config,&world);CHECK(match);
    int outcome=0;
    if(!pc_mod_game_bind(match)){outcome=3;goto cleanup;}
    if(!strcmp(scenario,"stale")) {
        ++WORD_AT(0x79094c);CHECK(pc_mod_game_capture(&world));
        g_esp=0x70000;WORD_AT(g_esp+4)=0;pc_mod_game_score(stock_score);
        if(original_scores!=1 || g_esp!=0x70004)outcome=3;
        goto cleanup;
    }
    if(!strcmp(scenario,"respawn")) {
        uint64_t identity=world.identities[0];uint32_t actor=0x10000,body=0x24000;
        WORD_AT(actor)=body;WORD_AT(body+0x7c)=actor;
        CHECK(pc_mod_game_capture(&world) && world.identities[0]==identity && tsfp_mod_match_matches(match,&world));
        goto cleanup;
    }
    /* Three independently accepted deaths, not a sampled score delta. */
    WORD_AT(0x50000)=0x21000;WORD_AT(0x50004)=0x20000;
    for(unsigned i=0;i<3;++i){g_esp=0x70000;WORD_AT(g_esp+4)=0x50000;pc_mod_game_death(stock_death);CHECK(g_esp==0x70004);}
    g_esp=0x70000;pc_mod_game_limits(stock_limits);CHECK(!original_limits && g_esp==0x70004);
    for(unsigned i=0;i<1000 && !finished;++i) {
        g_esp=0x70000;pc_mod_game_frame(stock_frame);
        if(g_esp!=0x70004 || g_eax!=0x45 || g_ebx!=11 || g_esi!=22 || g_edi!=33 || g_ebp!=44){outcome=3;break;}
        if(tsfp_mod_match_faulted(match)){fprintf(stderr,"%s\n",tsfp_mod_match_error(match));outcome=3;break;}
        pause_short();
    }
    if(finished!=1 || WORD_AT(0x10000+0xa6c)!=3 || WORD_AT(0x10000+0xa68)!=1 || WORD_AT(0x10000+0xea0+0xa68)!=2 ||
       memory[0x10000+0xa10+2]!=0 || memory[0x10000+0xa10+3]!=0 || WORD_AT(0x21000+0x258)!=3 || !original_frames)outcome=3;
cleanup:
    pc_mod_game_bind(NULL);tsfp_mod_match_close(match);
    for(unsigned i=0;i<2000 && !tsfp_mod_match_stopped(match);++i)pause_short();
    if(!tsfp_mod_match_stopped(match))outcome=3;
    tsfp_mod_match_destroy(match);free(memory);if(!outcome)puts("PASS guest bridge");return outcome;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=6 || _setmode(_fileno(stdin),_O_BINARY)<0)return 2;
    char args[5][4096];for(int i=0;i<5;++i)if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,args[i],sizeof(args[i]),NULL,NULL))return 2;
    return run(args[0],args[1],args[2],args[3],args[4]);
}
#else
int main(int argc,char **argv){return argc==6?run(argv[1],argv[2],argv[3],argv[4],argv[5]):2;}
#endif
