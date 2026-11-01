/* Authoritative item snapshots reproduce captured NPC drops independently of
   the stock Story throw path and its optional0x16 event. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <stdarg.h>
#ifndef _WIN32
#define _WIN32 1
#endif
static uint32_t memory[0x800000/4],g_esp,g_eax;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<sizeof memory && n<=sizeof memory-a;}
typedef struct GuestRegisters{uint32_t sp,ax;} GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_esp,g_eax};}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->ax;}
static int active=1;static uint32_t match=0x10203040;
static int story_npc_session(void){return active;}
static unsigned story_npc_humans(void){return 2;}
static uint32_t story_npc_match(void){return match;}
static volatile unsigned marker;
void sub_000C1950(void){marker=1;} void sub_000A9070(void){marker=2;} void sub_00353A90(void){marker=3;}
#include "pc_physical_state.h"
static PcRigidBodyState host_physical,guest_physical;static unsigned physics_inits,physical_applies;
void pc_rigid_body_capture(PcRigidBodyState *s,uint32_t e){(void)e;*s=host_physical;}
void pc_rigid_body_remove(uint32_t e){(void)e;}
int pc_rigid_body_present(uint32_t e,const PcRigidBodyState *s){assert(e==0x72000 && WORD_AT(e+0x80)==0x90001);guest_physical=*s;physical_applies++;return 1;}
void pc_character_ragdoll_capture(PcRagdollState *s,uint32_t e){(void)e;memset(s,0,sizeof *s);}
void pc_character_ragdoll_present(uint32_t e,const PcRagdollState *s){(void)e;(void)s;}
void sub_0034FF30(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,...);
static unsigned refreshes;
void pc_network_bots_entity_refresh(uint32_t entity){assert(entity==0x72000);++refreshes;}
#include "pc_story_drops.inc"
static StoryDropState sent[64];static unsigned sent_reliable[64],sends,creates,removes,fail_create;
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
 uint32_t a[10];va_list ap;va_start(ap,count);assert(count<=10);
 for(unsigned i=0;i<count;i++)a[i]=va_arg(ap,uint32_t);va_end(ap);
 if(fn==sub_00353A90){assert(count==5 && a[1]==0x1e && a[3]==sizeof(StoryDropState)+4 && sends<64);memcpy(&sent[sends],(void *)(g_xbox_mem_offset+a[2]+4),sizeof(StoryDropState));sent_reliable[sends++]=a[4];return 1;}
 if(fn==sub_000C1950){assert(count==10 && a[0]==1 && a[1]==2 && a[2]==25 && a[5]==0x4db9a8 && a[6]==18 && a[7]==126 && !a[8] && !a[9]);
  if(fail_create)return 0;
  assert(WORD_AT(0x541ea4)==91 && WORD_AT(0x541e9c)==1);assert(a[4]>g_esp);
  ++creates;WORD_AT(0x72020)=0x40;WORD_AT(0x72028)=0x40000000;WORD_AT(0x7202c)=100+creates;
  WORD_AT(0x7207c)=0x73000;WORD_AT(0x72230)=WORD_AT(0x541ea4);WORD_AT(0x73000)=0x72000;WORD_AT(0x7300c)=2;return 0x72000;}
 if(fn==sub_0034FF30){assert(count==1 && a[0]==0x72000);physics_inits++;WORD_AT(0x72080)=0x90001;return 0;}
 if(fn==sub_000A9070){assert(count==1 && a[0]==0x72000);++removes;WORD_AT(0x72028)|=0x80000000;return 0;}
 assert(0);return 0;
}
static void receive(const StoryDropState *s)
{memcpy((void *)(g_xbox_mem_offset+0x90000),s,sizeof *s);assert(story_drop_receive(0x31000,0x90000,0x1e,sizeof *s));}
int main(void)
{
 g_xbox_mem_offset=(uintptr_t)memory;g_esp=0x20000;WORD_AT(0x79094c)=2;WORD_AT(0x76b1a4)=15;
 WORD_AT(0x76b140)=1;WORD_AT(0x774028)=0x32000;WORD_AT(0x774040)=0;WORD_AT(0x31000)=0;
 const uint32_t owner=0x7b0d20+28*0xd8,entity=0x70000;
 WORD_AT(owner)=entity;WORD_AT(owner+0xc)=2;WORD_AT(owner+0x10)=9;WORD_AT(owner+0x14)=25;
 WORD_AT(owner+0x40)=18;WORD_AT(owner+0x44)=126;WORD_AT(owner+0xa4)=6;
 WORD_AT(entity+0x20)=0x40;WORD_AT(entity+0x7c)=owner;WORD_AT(entity+0x230)=91;
 float pos[3]={60.25885f,-.807326f,-1.695261f};memcpy((void *)(g_xbox_mem_offset+entity+0x64),pos,sizeof pos);
 story_drop_frame();assert(sends==1 && sent_reliable[0] && sent[0].quantity[0]==126 && sent[0].id==91 && sent[0].source==6);
 StoryDropState spawn=sent[0];story_drop_frame();assert(sends==1);
 WORD_AT(0x7de318)=6;pos[1]=-2.28f;memcpy((void *)(g_xbox_mem_offset+entity+0x64),pos,sizeof pos);
 story_drop_frame();assert(sends==2 && !sent_reliable[1]);StoryDropState landed=sent[1];
 WORD_AT(0x7de318)=66;story_drop_frame();assert(sends==3 && sent_reliable[2]);
 WORD_AT(entity+0x28)=0x80000000;WORD_AT(0x7de318)=72;story_drop_frame();
 assert(sends==4 && !sent[3].present && sent_reliable[3]);StoryDropState gone=sent[3];
 story_drop_reset();WORD_AT(0x76b140)=2;WORD_AT(0x541ea4)=777;WORD_AT(0x541e9c)=1;
 receive(&spawn);fail_create=1;story_drop_frame();assert(!creates && story_drop_fresh[28] && WORD_AT(0x541ea4)==777 && g_esp==0x20000);
 fail_create=0;story_drop_frame();assert(creates==1 && refreshes==1 && !story_drop_fresh[28]);
 assert(WORD_AT(0x541ea4)==777 && g_esp==0x20000 && WORD_AT(0x72230)==91);
 assert(WORD_AT(0x73044)==126 && WORD_AT(0x730a4)==6 && !(WORD_AT(0x72028)&0x40000000));
 receive(&spawn);story_drop_frame();assert(creates==1 && refreshes==1);
 receive(&landed);story_drop_frame();float y;memcpy(&y,(void *)(g_xbox_mem_offset+0x72068),4);assert(y==-2.28f && creates==1);
 receive(&spawn);story_drop_frame();assert(refreshes==2); /* out-of-order cannot rewind the throw */
 StoryDropState bad=gone;bad.position[0]=NAN;receive(&bad);story_drop_frame();assert(!removes);
 bad=gone;bad.quantity[0]=32768;receive(&bad);story_drop_frame();assert(!removes);
 bad=gone;bad.match++;receive(&bad);story_drop_frame();assert(!removes);
 WORD_AT(0x31000)=1;receive(&gone);story_drop_frame();assert(!removes);WORD_AT(0x31000)=0;
 receive(&gone);story_drop_frame();assert(removes==1);receive(&gone);story_drop_frame();assert(removes==1);
 story_drop_reset();receive(&spawn);story_drop_frame();assert(creates==2);
 WORD_AT(0x7202c)++;receive(&gone);story_drop_frame();assert(removes==1); /* reused storage is never destroyed */
 story_drop_reset();active=0;receive(&spawn);story_drop_frame();assert(creates==2);
 /* Smooth throws need a fresh transform every simulation tick, even though
    guest-local physics is disabled. Verify a full second and duplicate hooks. */
 story_drop_reset();active=1;WORD_AT(0x76b140)=1;WORD_AT(entity+0x28)=0;sends=0;
 for(unsigned tick=1000;tick<1060;tick++) {
  WORD_AT(0x7de318)=tick;pos[0]=(float)(tick-1000)*.05f;pos[1]=5.f-(float)(tick-1000)*.04f;
  memcpy((void *)(g_xbox_mem_offset+entity+0x64),pos,sizeof pos);
  float heading=(float)(tick-1000)*7.f;memcpy((void *)(g_xbox_mem_offset+entity+0x70),&heading,4);
  story_drop_frame();assert(sends==tick-999 && sent[sends-1].position[0]==pos[0]);
  assert(sent[sends-1].angles[1]==heading && sent_reliable[sends-1]==(tick==1000));
  story_drop_frame();assert(sends==tick-999); /* same-tick duplicate */
 }
 WORD_AT(0x7de318)=1060;story_drop_frame();assert(sends==61 && sent_reliable[60]);
 WORD_AT(0x7de318)=1061;story_drop_frame();assert(sends==61); /* stationary: no redundant packet */
 host_physical.present=1;host_physical.rotation[0]=host_physical.rotation[4]=host_physical.rotation[8]=1;
 host_physical.velocity[1]=3.5f;host_physical.angular[2]=1.25f;
 WORD_AT(0x7de318)=1062;story_drop_frame();assert(sends==62 && sent[61].physical.angular[2]==1.25f);
 StoryDropState tumbling=sent[61];WORD_AT(0x76b140)=2;story_drop_reset();receive(&tumbling);story_drop_frame();
 assert(physics_inits==1 && physical_applies==1 && guest_physical.velocity[1]==3.5f);
 tumbling.serial++;receive(&tumbling);story_drop_frame();assert(physics_inits==1 && physical_applies==2);
 tumbling.serial++;tumbling.physical.rotation[0]=NAN;receive(&tumbling);story_drop_frame();assert(physical_applies==2);
 puts("PASS: per-tick dropped-weapon movement/rotation, same-tick dedup and idle recovery; captured NPC pickup creation, landing, reliable recovery/removal, duplicate/stale/foreign rejection, retry and pointer reuse safety");return 0;
}
