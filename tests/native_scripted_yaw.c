/* Original callback registration, both entity passes, yaw cache and point transform.
 * World/physics/model services and sound consumers are controlled. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum {ENTITY=0x1000,MODEL=0x3000,INFO=0x3500,MATRIX=0x4000,POINT=0x5000,STACK=0x10000};
static unsigned starts,stops,ends,callbacks;
static int legacy_gate,bad_dirty;
static uint32_t gate(uint32_t site,uint32_t address,uint32_t value)
{return legacy_gate&&site==0x224170?value:pc_native_global_load(site,address,value);}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) gate(site,address,value)
#undef RECOMP_AFTER_STORE32
#define RECOMP_AFTER_STORE32(site,address) do { \
    PC_GPU_ACK_AFTER_STORE32(site,address); \
    if((site)==0x1D8BFDu)pc_native_trap_contact_store((uint32_t)(address)); \
    if(bad_dirty && (site)==0xA74F4u)MEM32(address)&=~0x1000u; \
} while(0)
void sub_00224170(void);
static void indirect(uint32_t target,uint32_t stack,uint32_t site)
{assert(target==0x224170&&MEM32(g_esp+4)==ENTITY);++callbacks;sub_00224170();assert(g_esp==stack-8);(void)site;}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) indirect(target,stack,site)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"unexpected matrix reflection")
void sub_000A06C0(void){g_esp+=4;}
void sub_0008C690(void){g_esp+=4;}void sub_003515D0(void){g_esp+=4;}
void sub_00171FB0(void){g_esp+=4;}void sub_0008C8E0(void){g_esp+=4;}
void sub_000A6530(void){assert(MEM32(g_esp+4)==ENTITY);g_esp+=4;}
void sub_0009FBE0(void){assert(MEM32(g_esp+4)==ENTITY);g_esp+=4;}
void sub_0009FD60(void){assert(MEM32(g_esp+4)==ENTITY);g_esp+=4;}
void sub_00045540(void){assert(MEM32(g_esp+4)==100);g_eax=1;g_esp+=4;}
void sub_00045460(void){assert(MEM32(g_esp+4)==100);++stops;g_esp+=4;}
void sub_000217B0(void){unsigned id=MEM32(g_esp+4);assert(id==0x378||id==0x379);if(id==0x378)++starts;else ++ends;g_eax=100;g_esp+=4;}
/* UNEXPECTED */
/* ROUTINES */
static void stack(void){g_esp=STACK;MEM32(STACK)=0x12345678;g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=g_seh_ebp=0;}
static void returned(void){assert(g_esp==STACK+4&&!g_fp_top&&MEM32(STACK)==0x12345678);assert(g_esi==0xdead&&g_edi==0xbeef&&g_ebx==0xfeed);}
static void transform(void){
 stack();MEM32(STACK+4)=MATRIX;MEM32(STACK+8)=ENTITY;MEM32(STACK+12)=1;MEM32(STACK+16)=0;sub_000A14B0();returned();
 MEMF(POINT)=1;MEMF(POINT+4)=MEMF(POINT+8)=0;stack();MEM32(STACK+4)=MATRIX;MEM32(STACK+8)=POINT;sub_00151E80();returned();
 float angle=MEMF(ENTITY+0x70)*MEMF(0x475d0c);
 assert(fabsf(MEMF(POINT)-(3+cosf(angle)))<.00001f&&fabsf(MEMF(POINT+4)-4)<.00001f&&fabsf(MEMF(POINT+8)-(5-sinf(angle)))<.00001f);
}
static void frame(unsigned whole,float delta)
{
 MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
 MEM32(0x4e7940)=whole;MEMF(0x4e7948)=delta;
 stack();sub_000A7670();returned();
 stack();sub_000A7870();returned();
 assert(MEMF(ENTITY+0x108)==MEMF(ENTITY+0x70));
 transform();
 assert(MEM32(0x4e7944)==whole&&MEMF(0x4e794c)==delta);
}
static void prepare(unsigned direction){
 memset((void *)(g_xbox_mem_offset+ENTITY),0,0x2e8);starts=stops=ends=callbacks=0;
 MEM32(ENTITY+4)=MODEL;MEM32(MODEL+4)=INFO;MEM32(ENTITY+0x20)=1;MEM8(ENTITY+0x32)=1;
 for(unsigned i=0;i<3;i++)MEMF(ENTITY+0x64+4*i)=MEMF(ENTITY+0xec+4*i)=3.f+i;
 MEM32(ENTITY+0x1a0)=direction;MEMF(ENTITY+0x70)=direction?0.f:91.f;
 MEMF(ENTITY+0x108)=MEMF(ENTITY+0x70);
 MEM32(ENTITY+0x22c)=0xffffffff;MEM32(ENTITY+0x28)=0x1000;
 stack();MEM32(STACK+4)=ENTITY;MEM32(STACK+8)=3;MEM32(STACK+12)=0x224170;sub_000A5CE0();returned();assert(MEM32(ENTITY+0x160)==0x224170);
 MEM32(0x7b4cc0)=ENTITY;transform();
}
int main(int argc,char **argv){legacy_gate=argc>1&&!strcmp(argv[1],"legacy-gate");bad_dirty=argc>1&&!strcmp(argv[1],"missing-dirty");setbuf(stdout,NULL);void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
/* CONSTANTS */
MEM32(0x4e8cec)=60;for(unsigned i=0;i<16;i++)MEMF(0x4e7900+4*i)=i%5==0?1.f:0.f;
const unsigned rates[]={60,60,120,144,180,240};double max_stop_error=0;
unsigned cases=0;
for(unsigned r=0;r<6;r++)for(unsigned slow=1;slow<=4;slow*=4)
for(unsigned phase=0;phase<4;phase++)for(unsigned direction=0;direction<2;direction++) {
 unsigned den=rates[r]*slow,previous=0,fresh=0,changed=0;
 g_tsfp_native_timing_active=r!=0;prepare(direction);
 printf("scripted_yaw fps=%u native=%u slow=%u phase=%u direction=%u\n",rates[r],r!=0,slow,phase,direction);
 float target=direction?91.f:0.f;unsigned f;
 for(f=1;f<=den*17;f++) {
  unsigned tick=(f*240+phase*den)/(den*4),whole=tick-previous;previous=tick;
  float delta=r?60.f/den:(whole?1.f:0.f),before=MEMF(ENTITY+0x70);
  float old_point[3];memcpy(old_point,(void *)(g_xbox_mem_offset+POINT),sizeof old_point);
  frame(whole,delta);
  int moved=MEMF(ENTITY+0x70)!=before;
  int new_point=memcmp(old_point,(void *)(g_xbox_mem_offset+POINT),sizeof old_point)!=0;
  changed+=moved;fresh+=new_point;
  assert(moved==new_point);
  if(r && before!=target)assert(moved);
  if(!r && !whole)assert(!moved);
  double expected=direction?fmin(91.,6.*f/den):fmax(0.,91.-6.*f/den);
  assert(fabs(MEMF(ENTITY+0x70)-expected)<.11);
  if(f%97==0) {
   unsigned a=starts,b=stops,c=ends;float angle=MEMF(ENTITY+0x70);
   float point[3];memcpy(point,(void *)(g_xbox_mem_offset+POINT),sizeof point);
   frame(r?1:0,0);
   assert(MEMF(ENTITY+0x70)==angle && starts==a && stops==b && ends==c);
   assert(!memcmp(point,(void *)(g_xbox_mem_offset+POINT),sizeof point));
  }
  if(ends)break;
 }
 assert(f<=den*17 && starts==1 && stops==1 && ends==1);
 assert(MEMF(ENTITY+0x70)==target && !(MEM32(ENTITY+0x28)&0x4000000));
 assert(fresh==changed && fresh>0);
 double error=fabs((double)f/den-91./6.);if(error>max_stop_error)max_stop_error=error;
 assert(error<2./60+1./den);++cases;
 for(unsigned i=0;i<3;i++)frame(1,r?60.f/den:1.f);
 assert(starts==1 && stops==1 && ends==1 && MEMF(ENTITY+0x70)==target);
}
printf("scripted_yaw cases=%u max_stop_error=%.9f\n",cases,max_stop_error);
unsigned parity=0;
for(unsigned direction=0;direction<2;direction++)for(unsigned start=0;start<3;start++)
for(unsigned whole=1;whole<=3;whole++) {
 unsigned char original[0x2e8];float point[3];unsigned events[3];
 for(unsigned native=0;native<2;native++) {
  g_tsfp_native_timing_active=native;prepare(direction);
  MEMF(ENTITY+0x70)=MEMF(ENTITY+0x108)=start==0?0.f:start==1?45.f:91.f;
  MEM32(ENTITY+0x28)|=0x1000;transform();frame(whole,(float)whole);
  if(!native) {
   memcpy(original,(void *)(g_xbox_mem_offset+ENTITY),sizeof original);
   memcpy(point,(void *)(g_xbox_mem_offset+POINT),sizeof point);
   events[0]=starts;events[1]=stops;events[2]=ends;
  } else {
   assert(!memcmp(original,(void *)(g_xbox_mem_offset+ENTITY),sizeof original));
   assert(!memcmp(point,(void *)(g_xbox_mem_offset+POINT),sizeof point));
   assert(events[0]==starts&&events[1]==stops&&events[2]==ends);++parity;
  }
 }
}
printf("scripted_yaw whole_catchup_byte_comparisons=%u\n",parity);
free(memory);puts("PASS: original registration/entity passes, fresh yaw matrices/points, cadence, clamp, sound events, pause and ABI");return 0;}
