#include "pc_controls.h"
#include "pc_developer.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static unsigned char memory[0x800000];
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_esp=0xc000;
#define WORD_AT(a) (*(uint32_t *)(memory+(uint32_t)(a)))
static PcDeveloperState options;
static int original_count,teleports,refreshes,views,combines,inverses,called;
static uint32_t arguments[12];static unsigned argc;
static double last_elapsed;
static unsigned captures,reloads,endings,body_draws;
static char report[160];
void sub_000A2740(void) {called=9;}
void sub_000D2DE0(void) {called=1;}
void sub_001519E0(void) {called=2;}
void sub_00153270(void) {called=3;}
void sub_00152260(void) {called=4;}
void sub_0006BB90(void) {called=5;}
void sub_0025EF80(void) {called=6;}
void sub_000653B0(void) {called=7;}
void sub_00067450(void) {called=8;}
void pc_developer_report(const char *message) {snprintf(report,sizeof report,"%s",message);}
void pc_developer_close(void) {}
int pc_developer_enabled(void) {return 1;}
void pc_developer_lobby_guest(void) {}
void pc_controls_apply_mouse(uint32_t player,uint32_t camera,ptrdiff_t offset)
{assert(player==0x4000&&camera==0x2000&&offset==g_xbox_mem_offset);}
static int guest_writable(uint32_t address,size_t bytes) {return address>=0x1000&&address<sizeof memory&&bytes<=sizeof memory-address;}
void pc_developer_get(PcDeveloperState *out) {*out=options;}
int pc_developer_flight_step(int mode,const float origin[3],const float direction[3],double dt,float position[3],float forward[3]) {
    (void)origin;(void)direction;last_elapsed=dt;
    for(int i=0;i<3;i++){position[i]=(float)(10*(i+1));forward[i]=i==0?1.f:0.f;}
    return mode!=0;
}
void pc_network_bots_entity_refresh(uint32_t entity) {assert(entity==0x6000);refreshes++;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...) {
    va_list ap;va_start(ap,count);argc=count;
    for(unsigned i=0;i<count;i++)arguments[i]=va_arg(ap,uint32_t);va_end(ap);
    fn();
    if(called==1){assert(argc==1&&arguments[0]==0x4000);memcpy(memory+0x4000+0xb4,memory+0x4000+0xa8,12);teleports++;}
    if(called==2){assert(argc==10&&arguments[0]==0x8100);assert(arguments[1]==0x41200000&&arguments[2]==0x41a00000&&arguments[3]==0x41f00000);views++;}
    if(called==3){assert(argc==3&&arguments[0]==0x8200&&arguments[1]==0x8100&&arguments[2]==0x8000);combines++;}
    if(called==4){assert(argc==2&&arguments[0]==0x8300&&arguments[1]==0x8100);inverses++;}
    if(called==5){assert(argc==2&&arguments[1]==0);return arguments[0]<2?0x9000:0;}
    if(called==6){assert(argc==1&&arguments[0]<2);WORD_AT(0xb000)=arguments[0];WORD_AT(0xb004)|=1u<<arguments[0];captures++;}
    if(called==7){assert(argc==1&&arguments[0]==WORD_AT(0xb004));reloads++;}
    if(called==9){assert(argc==2&&arguments[0]==0x6000&&arguments[1]==body_draws%3);assert(!(WORD_AT(0x6014)&1u));assert(!(WORD_AT(0xac6c)&2u));assert((WORD_AT(0x101f8)&0xffffff)==0x7fff00);body_draws++;}
    if(called==8){assert(argc==1&&arguments[0]==0&&(WORD_AT(0x7de45c)&0x8000));endings++;}
    return 0;
}
#include "pc_developer_guest.inc"
static unsigned ammo_calls;
static uint32_t expected_amount;
static void ammo_original(void) {assert(WORD_AT(g_esp+16)==expected_amount);ammo_calls++;g_esp+=4;}
static void grenade_original(void) {(*(int16_t *)(memory+0xa620))--;g_esp+=4;}
static void original(void) {
    original_count++;
    float eye[3]={1,2,3},direction[3]={0,0,-1};
    developer_vector(0x2000+0x27c,eye,1);developer_vector(0x2000+0x334,direction,1);
}
static void cutscene_original(void)
{
    assert(WORD_AT(0x6d60f0u)==1);
    float time=2.f;memcpy(memory+0x12008,&time,4);g_esp+=4;
}
int main(void) {
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    WORD_AT(0x7B0C7C)=0x4000;WORD_AT(0x7B0CC0)=0x2000;
    WORD_AT(0x4000+0x14)=0x6000;WORD_AT(0x6000+0x150)=0x4000;
    WORD_AT(0x7DE30C)=7;
    for(unsigned i=0;i<4;i++)WORD_AT(0x2000+0x204+i*4)=0x8000+i*0x100;
    original();original_count=0;
    pc_developer_camera_guest(original);assert(original_count==1&&!views&&!teleports);
    options.freecam=1;
    pc_developer_camera_guest(original);assert(original_count==2&&views==1&&combines==1&&inverses==1&&!teleports);
    assert(WORD_AT(0x2000+0x27c)==0x41200000&&WORD_AT(0x4000+0xa8)==0);
    WORD_AT(0x2000+0x210)=0xfffffff0;
    pc_developer_camera_guest(original);assert(views==1); /* Reject bad inverse before camera writes. */
    assert(WORD_AT(0x2000+0x27c)==0x3f800000);
    WORD_AT(0x2000+0x210)=0x8300;
    options.freecam=0;options.noclip=1;
    pc_developer_camera_guest(original);assert(teleports==1&&refreshes==1&&WORD_AT(0x4000+0xa8)==0x41200000);
    assert(views==2&&WORD_AT(0x2000+0x27c)==WORD_AT(0x4000+0xb4));
    WORD_AT(0x7497B4)=1;pc_developer_camera_guest(original);assert(last_elapsed==0);
    WORD_AT(0x4000+4)=1;pc_developer_camera_guest(original);assert(teleports==2);
    WORD_AT(0x4000+4)=0;WORD_AT(0x6000+0x150)=0;pc_developer_camera_guest(original);assert(teleports==2);
    assert(original_count==7);
    WORD_AT(0x6000+0x150)=0x4000;WORD_AT(0x6000+0x7c)=0xa000;
    WORD_AT(0x7b0c48)=0x4000;WORD_AT(0x75f5d0)=0xb000;
    WORD_AT(0x790950)=1;WORD_AT(0x78ad20)=4;
    options.checkpoint=1;pc_developer_guest_frame();assert(captures==1&&reloads==1&&WORD_AT(0xb000)==0);
    pc_developer_guest_frame();assert(captures==1&&reloads==1); /* consume once */
    options.checkpoint=2;pc_developer_guest_frame();assert(captures==2&&reloads==2&&WORD_AT(0xb000)==1);
    options.checkpoint=3;pc_developer_guest_frame();assert(endings==1&&captures==2);
    WORD_AT(0x78ad20)=5;options.checkpoint=4;pc_developer_guest_frame();assert(endings==1&&captures==2);
    assert(strstr(report,"offline Story"));
    WORD_AT(0xa000)=0x6000;
    WORD_AT(0xc004)=0xd000;WORD_AT(0xd000)=0x6000;
    options.god=1;assert(pc_developer_damage_blocked());
    WORD_AT(0xd000)=0x6100;assert(!pc_developer_damage_blocked());WORD_AT(0xd000)=0x6000;
    WORD_AT(0x76b0fc)=WORD_AT(0x76b108)=1;assert(!pc_developer_damage_blocked());
    WORD_AT(0x76b0fc)=WORD_AT(0x76b108)=0;options.god=0;assert(!pc_developer_damage_blocked());
    options.infinite_ammo=1;WORD_AT(0xc004)=0xa000;WORD_AT(0xc010)=2;expected_amount=0;
    pc_developer_ammo_guest(ammo_original);assert(ammo_calls==1&&g_esp==0xc004&&WORD_AT(0xc010)==2);
    g_esp=0xc000;WORD_AT(0xc004)=0xa100;expected_amount=2;pc_developer_ammo_guest(ammo_original);assert(ammo_calls==2);
    g_esp=0xc000;WORD_AT(0x4f9bac)=0xe000;
    WORD_AT(0xe014)=1;WORD_AT(0xe018)=6;WORD_AT(0x4ea4d8)=24;
    pc_developer_guest_frame();assert(*(int16_t *)(memory+0xa0de)==6&&*(int16_t *)(memory+0xa620)==24);
    WORD_AT(0xc004)=0x6000;pc_developer_grenade_guest(grenade_original);assert(*(int16_t *)(memory+0xa620)==24);g_esp=0xc000;
    options.freecam=1;WORD_AT(0x4008)=0x2000;WORD_AT(0x2214)=0;WORD_AT(0x6010)=0xe0000000;WORD_AT(0x60a0)=1;
    pc_developer_world_visibility();assert(WORD_AT(0x6010)==0xe0000001&&WORD_AT(0x60a0)==1&&WORD_AT(0xa02c)==0);
    g_esp=0xc000;unsigned previous_calls=original_count;
    pc_developer_viewmodel_draw(original);assert(g_esp==0xc004&&original_count==previous_calls);
    WORD_AT(0x6004)=0x10000;WORD_AT(0x6014)=0xabcd0001;WORD_AT(0xac6c)=0x12;WORD_AT(0x101f8)=0x0010040f;
    pc_developer_draw_body();assert(body_draws==3&&WORD_AT(0x6014)==0xabcd0000&&WORD_AT(0xac6c)==0x12&&WORD_AT(0x101f8)==0x0010040f);
    WORD_AT(0x7b0c7c)=0x5000;pc_developer_draw_body();assert(body_draws==3);
    WORD_AT(0x7b0c7c)=0x4000;WORD_AT(0x76b0fc)=WORD_AT(0x76b108)=1;
    pc_developer_draw_body();assert(body_draws==3);WORD_AT(0x76b0fc)=WORD_AT(0x76b108)=0;
    options.freecam=0;pc_developer_draw_body();assert(body_draws==3);
    pc_developer_world_visibility();assert(WORD_AT(0x60a0)==1);pc_developer_viewmodel_draw(original);assert(original_count==previous_calls+1);
    g_esp=0xc000;WORD_AT(g_esp+4)=0x12000;WORD_AT(0x7de458)=0x20000000;
    WORD_AT(0x4e8cec)=60;WORD_AT(0x7497a8)=1000;
    pc_developer_cutscene_guest(cutscene_original);
    assert(g_esp==0xc004&&WORD_AT(0x6d60f0)==0&&WORD_AT(0x12004)==880);
    g_esp=0xc000;WORD_AT(0x7de458)=0;
    previous_calls=original_count;pc_developer_cutscene_guest(original);assert(original_count==previous_calls+1);
    puts("Developer camera: ordinary fallback, detached view ABI, player position, pause and mapping guards passed");return 0;
}
