/* Original constructor/registration, frame/level-clock, active selection,
 * wind sampler and rotation/point graph. World/model services, visibility,
 * sine table and host timestamps are controlled. Pytest injects original
 * routines from a locally verified XBE; no generated guest code is tracked. */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
#undef RECOMP_FAST_ENTRY
#define RECOMP_FAST_ENTRY(site) 0
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected indirect tail")
ptrdiff_t g_xbox_mem_offset;int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
/* TIMING_HEADER */
static double wind_start_phase,wind_exact_delta;
static PcNativeTiming wind_host_clock;static int64_t wind_host_now;PcNativeStep g_tsfp_native_step;
static unsigned matrices,zero_points,changed_points;static int bad_matrix;static double world_elapsed;static float last_point[3]; static float windx,windz;
static float pitch,yaw;
static int legacy_gate;
RECOMP_TLS double g_fp_stack[8];RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
/* WIND_GUEST */

void sub_000A5CE0_actual(void);
void sub_000A5CE0(void){pc_native_wind_register_guest(sub_000A5CE0_actual);}

void sub_001DDF10_gen(void);
void sub_001DDF10(void){pc_native_wind_callback_guest(sub_001DDF10_gen);}
void sub_000AB050(void);
void pc_native_wind_step(void){if(legacy_gate)sub_000AB050();else pc_native_wind_oscillator_guest(sub_000AB050);}
static uint32_t wind_load(uint32_t site,uint32_t address,uint32_t value){
 if(legacy_gate&&(site==0x1ddf7b||site==0x1ddf96))return value;
 return site==0x1ddf7b||site==0x1ddf96 ? pc_native_wind_global_load(address,value) : pc_native_global_load(site,address,value);
}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) wind_load(site,address,value)
void sub_00116680(void);
void sub_00151650_actual(void);
void sub_00151650(void){assert(MEM32(g_esp+4)==0x4000||MEM32(g_esp+4)==0x4800);pitch=MEMF(g_esp+8);yaw=MEMF(g_esp+16);
 if(bad_matrix&&g_tsfp_native_timing_active&&matrices)g_esp+=4;else sub_00151650_actual();matrices++;}
void sub_000A06C0(void){g_esp+=4;}
void sub_0009E690(void){g_esp+=4;}
void sub_000966E0(void){assert((MEM32(g_esp+4)==0x2000||MEM32(g_esp+4)==0x2800)&&MEM32(g_esp+8)==1&&MEM32(g_esp+12)==0);g_esp+=4;}
void sub_0009E720(void){assert((MEM32(g_esp+4)==0x1000||MEM32(g_esp+4)==0x12e8)&&MEM32(g_esp+8)==0x1dde30);g_esp+=4;}
void sub_00257860(void){assert(!"Unexpected constructor branch");}



static unsigned dispatch_callbacks;
void sub_00099EE0(void){assert(!"Unexpected normal-frame tail");}
void sub_001295A0(void){g_esp+=4;}
void sub_001DDF10(void);
static void dispatch(uint32_t target,uint32_t stack,uint32_t site){assert(target==0x1ddf10&&site==0xa77e6&&(MEM32(g_esp+4)==0x1000||MEM32(g_esp+4)==0x12e8));++dispatch_callbacks;sub_001DDF10();assert(g_esp==stack-8);}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) dispatch(target,stack,site)
void sub_0003E970(void){assert(MEM32(g_esp+4)==64);assert(g_esi==0x1000||g_esi==0x12e8);g_eax=MEM32(g_esi+4)+0x2000;g_esp+=4;}
void sub_0009E330(void){assert((MEM32(g_esp+4)==0x1000||MEM32(g_esp+4)==0x12e8)&&MEM32(g_esp+8)==2);uint32_t matrix=MEM32(MEM32(MEM32(g_esp+4)+4)+0x10);for(unsigned i=0;i<16;i++)MEMF(matrix+4*i)=i%5==0?1.f:0.f;g_esp+=4;}

void sub_000AB050(void);
/* ORIGINAL_ROUTINES */

void sub_001DE250(void);
void sub_000A5CE0(void);
void sub_00116510(void);
void sub_00116680(void);
void sub_00151650_actual(void);
void sub_00151E80(void);







static int geometry(void){
 uint32_t saved=g_esp;
 MEMF(0x5000)=1;MEMF(0x5004)=2;MEMF(0x5008)=3;
 g_esp=0x18000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x4000;MEM32(g_esp+8)=0x5000;
 sub_00151E80();assert(g_esp==0x18004&&!g_fp_top);g_esp=saved;
 double x=MEMF(0x30e8)*3.14159265358979323846/180,z=MEMF(0x30ec)*3.14159265358979323846/180;
 double u=cos(z)-2*sin(z),v=sin(z)+2*cos(z);
 double expected[3]={u,v*cos(x)-3*sin(x),v*sin(x)+3*cos(x)};
 int changed=0;
 for(unsigned i=0;i<3;i++){assert(isfinite(MEMF(0x5000+4*i)));assert(fabs(MEMF(0x5000+4*i)-expected[i])<.00001);changed|=last_point[i]!=MEMF(0x5000+4*i);last_point[i]=MEMF(0x5000+4*i);}
 for(unsigned i=0;i<16;i++)assert(isfinite(MEMF(0x4000+4*i)));
 return changed;
}

void sub_000389B0(void){g_esp+=4;}
void sub_0003F050(void){g_esp+=4;}
void sub_00046350(void){g_esp+=4;}
void sub_00046610(void){g_esp+=4;}
void sub_00068830(void);
void sub_00083480(void){fprintf(stderr,"UNEXPECTED sub_00083480\n");abort();}
void sub_000834A0(void){fprintf(stderr,"UNEXPECTED sub_000834A0\n");abort();}
void sub_000897D0(void){g_esp+=4;}
void sub_0008E970(void){fprintf(stderr,"UNEXPECTED sub_0008E970\n");abort();}
void sub_00099990(void){g_esp+=4;}
void sub_0009E4B0(void){g_esp+=4;}
void sub_0009F5E0(void){fprintf(stderr,"UNEXPECTED sub_0009F5E0\n");abort();}
void sub_000A0860(void){fprintf(stderr,"UNEXPECTED sub_000A0860\n");abort();}
void sub_000A2B50(void){fprintf(stderr,"UNEXPECTED sub_000A2B50\n");abort();}
void sub_000C7950(void){g_esp+=4;}
void sub_000C9C20(void){g_esp+=4;}
void sub_000DC7B0(void){g_esp+=4;}
void sub_000F6D50(void){g_esp+=4;}
void sub_000F7B10(void){g_esp+=4;}
void sub_000FE000(void){g_esp+=4;}
void sub_001154E0(void){g_esp+=4;}
void sub_00116410(void){g_esp+=4;}
void sub_0011CB30(void){g_esp+=4;}
void sub_00128FA0(void){fprintf(stderr,"UNEXPECTED sub_00128FA0\n");abort();}
void sub_00153C00(void){g_esp+=4;}
void sub_00158C80(void){g_esp+=4;}
void sub_0015E830(void){g_esp+=4;}
void sub_00164E00(void){g_esp+=4;}
void sub_001651C0(void){g_esp+=4;}
void sub_00187580(void){g_esp+=4;}
void sub_001D1960(void){g_esp+=4;}
void sub_00259930(void){g_esp+=4;}
void sub_002D2930(void){g_esp+=4;}
void sub_002FB4C0(void){g_esp+=4;}
void sub_00347CD0(void){g_esp+=4;}
void sub_003493D0(void){g_esp+=4;}
void sub_003533F0(void){fprintf(stderr,"UNEXPECTED sub_003533F0\n");abort();}
void sub_00358560(void){fprintf(stderr,"UNEXPECTED sub_00358560\n");abort();}
void sub_000A7670(void);
void sub_000A0C50(void);
void sub_0036E430(void);
void sub_00153240(void);
void sub_000A0130(void);
void sub_0009FBA0(void);
void sub_000A00B0(void);
void sub_001CE2E0(void);








static unsigned clock_stores;static uint32_t clock_store_site;
uint32_t pc_native_visual_store(uint32_t site,uint32_t value){++clock_stores;clock_store_site=site;return value;}
void sub_000344E0(void){g_eax=0;g_esp+=4;}
void sub_0003D7B0(void){g_eax=0;g_esp+=4;}
void sub_00042830(void){g_eax=0;g_esp+=4;}
void sub_00042870(void){g_eax=0;g_esp+=4;}
void sub_000459E0(void){g_eax=0;g_esp+=4;}
void sub_00055D80(void){g_eax=0;g_esp+=4;}
void sub_00055D90(void){g_eax=0;g_esp+=4;}
void sub_00056050(void){g_eax=0;g_esp+=4;}
void sub_00061E90(void){g_eax=0;g_esp+=4;}
void sub_00061EE0(void){g_eax=0;g_esp+=4;}
void sub_00064D10(void){g_eax=0;g_esp+=4;}
void sub_00065230(void){g_eax=0;g_esp+=4;}
void sub_00065250(void){g_eax=0;g_esp+=4;}
void sub_00065270(void){g_eax=0;g_esp+=4;}
void sub_00065470(void){g_eax=0;g_esp+=4;}
void sub_000658D0(void){g_eax=0;g_esp+=4;}
void sub_00065940(void){g_eax=0;g_esp+=4;}
void sub_00065FE0(void){g_eax=0;g_esp+=4;}
void sub_000667B0(void){g_eax=0;g_esp+=4;}
void sub_000667F0(void){g_eax=0;g_esp+=4;}
void sub_000681C0(void){g_eax=0;g_esp+=4;}
void sub_000687A0(void){g_eax=0;g_esp+=4;}
void sub_00098DB0(void){g_eax=0;g_esp+=4;}
void sub_0009DA10(void){g_eax=0;g_esp+=4;}
void sub_000C6390(void){g_eax=0;g_esp+=4;}
void sub_000C63F0(void){g_eax=0;g_esp+=4;}
void sub_000CBCF0(void){g_eax=0;g_esp+=4;}
void sub_000D0CE0(void){g_eax=0;g_esp+=4;}
void sub_000D50E0(void){g_eax=0;g_esp+=4;}
void sub_00125F50(void){g_eax=0;g_esp+=4;}
void sub_0012D350(void){g_eax=0;g_esp+=4;}
void sub_00153AF0(void){g_eax=0;g_esp+=4;}
void sub_001553E0(void){g_eax=0;g_esp+=4;}
void sub_00164A30(void){g_eax=0;g_esp+=4;}
void sub_00164B60(void){g_eax=0;g_esp+=4;}
void sub_001CDAE0(void){g_eax=0;g_esp+=4;}
void sub_00212490(void){g_eax=0;g_esp+=4;}
void sub_0022DEC0(void){g_eax=0;g_esp+=4;}
void sub_002321C0(void){g_eax=0;g_esp+=4;}
void sub_0025A570(void){g_eax=0;g_esp+=4;}
void sub_002E44A0(void){g_eax=0;g_esp+=4;}
void sub_002E4C10(void){g_eax=0;g_esp+=4;}
void sub_002E68C0(void){g_eax=0;g_esp+=4;}
void sub_002E68E0(void){g_eax=0;g_esp+=4;}
void sub_002EB030(void){g_eax=0;g_esp+=4;}
void sub_002EB1F0(void){g_eax=0;g_esp+=4;}
void sub_002EC250(void){g_eax=0;g_esp+=4;}
void sub_002EDD90(void){g_eax=0;g_esp+=4;}
void sub_002F2D30(void){g_eax=0;g_esp+=4;}
void sub_002F43D0(void){g_eax=0;g_esp+=4;}
void sub_003526D0(void){g_eax=0;g_esp+=4;}
void sub_00352BB0(void){g_eax=0;g_esp+=4;}
void sub_00355EC0(void){g_eax=0;g_esp+=4;}
void sub_00355F10(void){g_eax=0;g_esp+=4;}
void sub_00356800(void){g_eax=0;g_esp+=4;}
void sub_00357690(void){g_eax=0;g_esp+=4;}
void sub_00357BD0(void){g_eax=0;g_esp+=4;}
void sub_00358E40(void){g_eax=0;g_esp+=4;}
void sub_00359AA0(void){g_eax=0;g_esp+=4;}
void sub_00359E70(void){g_eax=0;g_esp+=4;}
void sub_0035DB10(void){g_eax=0;g_esp+=4;}
void sub_0035E960(void){g_eax=0;g_esp+=4;}
void sub_003C85D4(void){g_eax=0;g_esp+=4;}

static float ref[301][2];
static float norm(float a){return a>=180?a-360:a;}
static void init(float angle,float velocity){
 /* history is initialized by original registration hook */
 memset((char*)g_xbox_mem_offset+0x1000,0,0x3000);MEM32(0x1004)=0x2000;MEM32(0x2004)=0x2100;MEM32(0x2104)=1;MEM32(0x2010)=0x4000;MEM32(0x107c)=0x3000;
 MEMF(0x30c8)=MEMF(0x47862c);MEMF(0x30cc)=MEMF(0x475d4c);MEMF(0x30d0)=MEMF(0x475d18);MEMF(0x30d4)=MEMF(0x476040);MEMF(0x30d8)=MEMF(0x47d1f4);
 MEMF(0x30e8)=angle;MEMF(0x30ec)=angle?360-angle:0;MEMF(0x30f0)=velocity;MEMF(0x30f4)=-velocity;

 world_elapsed=0;matrices=0;MEM32(0x7de318)=0;MEM32(0x7de30c)=7;wind_host_clock=(PcNativeTiming){0};wind_host_now=0;g_tsfp_native_step=pc_native_timing_step(&wind_host_clock,0,43200,240,0);MEM32(0x79094c)=4;MEM32(0x7de458)=0;MEM32(0x72f5e8)=0x1000;MEM32(0x7b4fac)=1;MEM32(0x7b4cc0)=0x1000;
 MEMF(0x7ac814)=100;MEMF(0x7ac818)=60;MEMF(0x7ac81c)=40;MEMF(0x7ac820)=windx;
 MEM16(0x1094)=0x32;MEM32(0x1020)=1;MEM32(0x1004)=0x2000;MEM32(0x2004)=0x2100;MEM32(0x2104)=1;MEM32(0x2010)=0x4000;MEM32(0x107c)=0x3000;
 g_esp=0x10000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x1000;MEM32(g_esp+8)=0;
 sub_001DE250();assert(g_esp==0x10004&&MEM32(0x1160)==0x1ddf10&&MEM8(0x1032)==2);
 assert(MEMF(0x30c8)==MEMF(0x47862c)&&MEMF(0x30cc)==MEMF(0x475d4c)&&MEMF(0x30d0)==MEMF(0x475d18)&&MEMF(0x30d4)==MEMF(0x476040)&&MEMF(0x30d8)==MEMF(0x47d1f4));
 MEM32(0x1024)=0x6000;MEM32(0x6028)=0x200;MEMF(0x1064)=3;MEMF(0x1068)=4;MEMF(0x106c)=5;
 MEMF(0x30e8)=angle;MEMF(0x30ec)=angle?360-angle:0;MEMF(0x30f0)=velocity;MEMF(0x30f4)=-velocity;
 g_esp=0x18000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x4000;MEMF(g_esp+8)=angle*MEMF(0x475d0c);MEMF(g_esp+12)=0;MEMF(g_esp+16)=(angle?360-angle:0)*MEMF(0x475d0c);sub_00151650();assert(g_esp==0x18004);geometry();

}

static const uint32_t owners[2]={0x1000,0x12e8};
static unsigned char solo[2][3000][320];
static unsigned fresh[2];
static void owner_register(uint32_t owner,uint32_t callback){
 g_esp=0x10000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=owner;MEM32(g_esp+8)=3;MEM32(g_esp+12)=callback;
 sub_000A5CE0();assert(g_esp==0x10004&&MEM32(owner+0x160)==callback);
}
static void setup_pair(unsigned mode){
 g_tsfp_native_timing_active=1;windx=.2f;windz=-.2f;init(10,5);
 memcpy((void *)(g_xbox_mem_offset+0x12e8),(void *)(g_xbox_mem_offset+0x1000),0x2e8);
 memcpy((void *)(g_xbox_mem_offset+0x2800),(void *)(g_xbox_mem_offset+0x2000),0x100);
 memcpy((void *)(g_xbox_mem_offset+0x2900),(void *)(g_xbox_mem_offset+0x2100),16);
 memcpy((void *)(g_xbox_mem_offset+0x3800),(void *)(g_xbox_mem_offset+0x3000),0x100);
 MEM32(0x12ec)=0x2800;MEM32(0x2804)=0x2900;MEM32(0x2810)=0x4800;MEM32(0x1364)=0x3800;MEM32(0x1314)=2;
 g_esp=0x10000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x12e8;MEM32(g_esp+8)=0;
 sub_001DE250();assert(g_esp==0x10004&&MEM32(0x1448)==0x1ddf10);
 MEMF(0x38e8)=350;MEMF(0x38ec)=10;MEMF(0x38f0)=-5;MEMF(0x38f4)=5;
 MEMF(0x134c)=13;MEMF(0x1350)=24;MEMF(0x1354)=35;
 assert(wind_owners.count==2);
 MEM32(0x7b4fac)=2;
 if(mode<2)MEM32(owners[1-mode]+0x28)=0x80000000;
 fresh[0]=fresh[1]=0;
}
static void point_check(unsigned which){
 uint32_t data=MEM32(owners[which]+0x7c),matrix=MEM32(MEM32(owners[which]+4)+0x10);
 g_esp=0x18000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=matrix;MEM32(g_esp+8)=0x5000;
 MEMF(0x5000)=1;MEMF(0x5004)=2;MEMF(0x5008)=3;
 sub_00151E80();assert(g_esp==0x18004&&!g_fp_top);
 double x=MEMF(data+0xe8)*3.14159265358979323846/180,z=MEMF(data+0xec)*3.14159265358979323846/180;
 double u=cos(z)-2*sin(z),v=sin(z)+2*cos(z),expected[3]={u,v*cos(x)-3*sin(x),v*sin(x)+3*cos(x)};
 for(unsigned i=0;i<3;i++)assert(isfinite(MEMF(0x5000+4*i))&&fabs(MEMF(0x5000+4*i)-expected[i])<.00001);
}

static unsigned pause_frames,divergent_phases;
static void pause_pair(void){
 unsigned char data[2][320];PcWindOwner history[2];
 for(unsigned i=0;i<2;i++){
  memcpy(data[i],(void *)(g_xbox_mem_offset+0x3000+i*0x800),256);
  memcpy(data[i]+256,(void *)(g_xbox_mem_offset+0x4000+i*0x800),64);
  history[i]=wind_owners.entries[pc_wind_owner_index(&wind_owners,owners[i])];
 }
 uint32_t level=MEM32(0x7de318);
 for(unsigned j=0;j<7;j++){
  wind_host_now+=75;g_tsfp_native_step=pc_native_timing_step(&wind_host_clock,wind_host_now,43200,240,1);
  assert(!g_tsfp_native_step.game_units&&!g_tsfp_native_step.game_ticks&&g_tsfp_native_step.game_delta==0);
  divergent_phases+=g_tsfp_native_step.raw_phase!=g_tsfp_native_step.game_phase;
  MEM32(0x4e7940)=g_tsfp_native_step.raw_ticks;MEMF(0x4e7948)=g_tsfp_native_step.raw_delta;
  MEM32(0x4e7944)=g_tsfp_native_step.game_ticks;MEMF(0x4e794c)=g_tsfp_native_step.game_delta;
  g_esp=0x10000;MEM32(g_esp)=0x12345678;g_ebx=0xfeed;g_esi=0xdead;g_edi=0xbeef;g_ebp=g_seh_ebp=0;
  sub_001CE2E0();assert(g_esp==0x10004&&!g_fp_top&&MEM32(0x7de318)==level);
  for(unsigned i=0;i<2;i++){
   assert(!memcmp(data[i],(void *)(g_xbox_mem_offset+0x3000+i*0x800),256));
   assert(!memcmp(data[i]+256,(void *)(g_xbox_mem_offset+0x4000+i*0x800),64));
   assert(!memcmp(&history[i],wind_owners.entries+pc_wind_owner_index(&wind_owners,owners[i]),sizeof(PcWindOwner)));
  }
  ++pause_frames;
 }
}

static void pair_frame(unsigned n,unsigned units,unsigned mode){
 unsigned tick=n*units/720,next=(n+1)*units/720,whole=next-tick;
 wind_host_now+=units;
 g_tsfp_native_step=pc_native_timing_step(&wind_host_clock,wind_host_now,43200,240,0);
 assert(g_tsfp_native_step.game_ticks==whole);
 wind_exact_delta=(double)g_tsfp_native_step.game_units/g_tsfp_native_step.units_per_tick;
 MEM32(0x4e7944)=whole;MEMF(0x4e794c)=g_tsfp_native_step.game_delta;
 MEM32(0x4e7940)=g_tsfp_native_step.raw_ticks;MEMF(0x4e7948)=g_tsfp_native_step.raw_delta;
 MEMF(0x7ac820)=.2f+(float)(((n+1)*wind_exact_delta)*.01);
 unsigned char before[2][64];for(unsigned i=0;i<2;i++)memcpy(before[i],(void *)(g_xbox_mem_offset+0x4000+i*0x800),64);
 unsigned callbacks=dispatch_callbacks;
 g_esp=0x10000;MEM32(g_esp)=0x12345678;g_ebx=0xfeed;g_esi=0xdead;g_edi=0xbeef;g_ebp=g_seh_ebp=0;
 sub_001CE2E0();assert(g_esp==0x10004&&!g_fp_top&&g_ebx==0xfeed&&g_esi==0xdead&&g_edi==0xbeef);
 assert(dispatch_callbacks==callbacks+(mode==2?2:1));
 assert(MEM32(0x7de318)==next&&clock_stores&&clock_store_site==0x69265);
 for(unsigned i=0;i<2;i++)if(mode==2||mode==i){
  PcWindOwner *entry=wind_owners.entries+pc_wind_owner_index(&wind_owners,owners[i]);
  if(g_tsfp_native_step.game_delta<1)assert(entry->clock_valid&&entry->tick==next&&entry->phase==wind_host_clock.game_remainder);else assert(!entry->clock_valid);
 }
 for(unsigned i=0;i<2;i++)if(mode==2||mode==i){
  unsigned char current[320];memcpy(current,(void *)(g_xbox_mem_offset+0x3000+i*0x800),256);
  memcpy(current+256,(void *)(g_xbox_mem_offset+0x4000+i*0x800),64);
  point_check(i);
  if(!whole && n*units<43200){assert(memcmp(before[i],current+256,64));++fresh[i];}
  if(mode==2)assert(!memcmp(current,solo[i][n],320));else memcpy(solo[i][n],current,320);
 }
 if(mode==2&&n%97==31)pause_pair();
}

static void check_scopes(void){
unsigned parity=0;unsigned char original[320];
for(unsigned type=0;type<6;type++)for(unsigned hi=0;hi<2;hi++)for(unsigned whole=0;whole<4;whole++){
 float h=hi?.5f:.25f;PcNativeStep step={0};step.game_delta=.25f;step.game_units=10800;step.units_per_tick=43200;
 if(type==1)step.game_units=0;
 if(type==2)step.units_per_tick=0;
 if(type==3)step.game_phase=-1;
 if(type==4)step.game_phase=43200;
 if(type==5)step.game_units=-10800;
 if(pc_native_wind_clock_matches(&step,h,whole))continue;
 for(unsigned baseline=0;baseline<2;baseline++){
  setup_pair(2);legacy_gate=baseline==0;g_tsfp_native_step=step;
  MEMF(0x4e794c)=h;MEM32(0x4e7944)=whole;
  g_esp=0x10000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x1000;g_ebp=g_seh_ebp=0;
  sub_001DDF10();assert(g_esp==0x10004&&!g_fp_top);
  unsigned char current[320];memcpy(current,(void *)(g_xbox_mem_offset+0x3000),256);memcpy(current+256,(void *)(g_xbox_mem_offset+0x4000),64);
  if(!baseline)memcpy(original,current,320);else assert(!memcmp(original,current,320));
 }
 ++parity;
}
assert(parity==47);
/* A local advancing scope with published game time paused has zero whole
 * ticks, so the oscillator hook is never entered. Entry observation must reset. */
setup_pair(2);legacy_gate=0;g_tsfp_native_step=(PcNativeStep){0};g_tsfp_native_step.units_per_tick=43200;g_tsfp_native_step.game_units=10800;
MEMF(0x4e794c)=.25f;MEM32(0x4e7944)=0;
g_esp=0x10000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x1000;sub_001DDF10();
PcWindOwner *entry=wind_owners.entries+pc_wind_owner_index(&wind_owners,0x1000);assert(entry->axes[0].prepared&&entry->clock_valid);
g_tsfp_native_step.game_phase=10800;g_tsfp_native_step.game_units=0;
g_esp=0x10000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x1000;sub_001DDF10();
assert(!entry->axes[0].prepared&&!entry->axes[1].prepared&&!entry->clock_valid);
printf("unsupported_scopes original_byte_parity=%u skipped_oscillator_history_reset=PASS\n",parity);
}

static void check_whole_fallback(void){
 for(unsigned mode=0;mode<3;mode++){
  setup_pair(mode);for(unsigned n=0;n<180;n++)pair_frame(n,720,mode);
 }
 setup_pair(2);g_tsfp_native_timing_active=0;
 for(unsigned n=0;n<180;n++)pair_frame(n,720,2);
 puts("PASS: Original/native whole fallback360 owner frames byte equal");
}

int main(int argc,char **argv){legacy_gate=argc>1&&!strcmp(argv[1],"legacy");bad_matrix=argc>1&&!strcmp(argv[1],"freeze-matrix");setbuf(stdout,NULL);void *m=calloc(1,0x800000);assert(m);g_xbox_mem_offset=(ptrdiff_t)m;
/* ORIGINAL_CONSTANTS */
for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+4*i)=(float)sin(2*3.14159265358979323846*i/2048);
if(argc>1&&!strcmp(argv[1],"scopes")){check_scopes();free(wind_owners.entries);free(m);return 0;}
if(argc>1&&!strcmp(argv[1],"whole")){check_whole_fallback();free(wind_owners.entries);free(m);return 0;}
const unsigned steps[]={360,300,240,180,45};unsigned comparisons=0;
for(unsigned k=0;k<5;k++){
 unsigned units=steps[k],frames=129600/units;assert(frames<=3000);
 for(unsigned mode=0;mode<3;mode++){
  setup_pair(mode);for(unsigned n=0;n<frames;n++)pair_frame(n,units,mode);
  for(unsigned i=0;i<2;i++)if(mode==2||mode==i)assert(fresh[i]==43200/units-60);
  if(mode==2)comparisons+=frames*2;
 }
}
/* Actual re-registration, removal and re-registration of the same guest slot. */
PcWindOwner *first=wind_owners.entries+pc_wind_owner_index(&wind_owners,0x1000),saved=*first;
owner_register(0x12e8,0x1ddf10);
PcWindOwner *second=wind_owners.entries+pc_wind_owner_index(&wind_owners,0x12e8);
assert(!second->axes[0].prepared&&!second->axes[1].prepared&&!second->clock_valid);
first=wind_owners.entries+pc_wind_owner_index(&wind_owners,0x1000);assert(!memcmp(first,&saved,sizeof saved));
owner_register(0x12e8,0);assert(wind_owners.count==1);
owner_register(0x12e8,0x1ddf10);assert(wind_owners.count==2);
second=wind_owners.entries+pc_wind_owner_index(&wind_owners,0x12e8);assert(!second->axes[0].prepared&&!second->clock_valid);
assert(pause_frames>0&&divergent_phases>0);printf("pause_frames=%u divergent_raw_game_phases=%u\n",pause_frames,divergent_phases);
printf("multiowner comparisons=%u original_registration_reuse_removal=PASS\n",comparisons);
puts("PASS: original wind clock, registration, two-owner independence and paused resume");free(wind_owners.entries);free(m);return 0;}
