/* Original normal-game caller, voice queue setters, scheduler and main RNG.
 * Other frame services, voice definitions and audio consumers are controlled. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3;
static int legacy_gate,ignore_generation;
static uint32_t gate(uint32_t site,uint32_t address,uint32_t value)
{return legacy_gate&&site==0x164e00u?value:pc_native_global_load(site,address,value);}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) gate(site,address,value)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"unexpected frame dispatch target")
enum {POOL=0x1000,PRIMARY=POOL+0x2e8,SECONDARY=POOL+0x5d0,DATA=0x8000,VOICES=0x9000,STACK=0x10000};
static unsigned primary_events,secondary_events,lookups;
static double elapsed,primary_time,secondary_time;
static uint64_t initial_seed,expected_seed;
static uint32_t primary_id,default_id;
static void frame_service(void) {g_esp+=4;}
void sub_00164470(void)
{assert(g_eax==PRIMARY);++lookups;g_eax=VOICES;g_esp+=4;}
static void request(uint32_t data,uint32_t id,uint32_t flags)
{
 if(id==primary_id) {
  assert(flags==(id==0x1008||id==0x1b25?0x100u:0x900u));
  assert(data==(flags==0x900?DATA:0));++primary_events;primary_time=elapsed;
 } else {
  assert(id==default_id&&data==DATA+0x100&&flags==0x100);
  ++secondary_events;secondary_time=elapsed;
 }
}
void sub_00164310(void)
{assert(MEM32(g_esp+12)==0);request(0,MEM32(g_esp+4),MEM32(g_esp+8));g_eax=100;g_esp+=4;}
void sub_00164C40(void)
{assert(MEM32(g_esp+16)==0);request(MEM32(g_esp+4),MEM32(g_esp+8),MEM32(g_esp+12));g_eax=100;g_esp+=4;}
/* FRAME_SERVICES */
/* ROUTINES */
static void stack(void)
{g_esp=STACK;MEM32(STACK)=0x12345678;g_ebx=0x1111;g_esi=0x2222;g_edi=0x3333;g_ebp=g_seh_ebp=0;}
static void returned(void)
{assert(g_esp==STACK+4&&MEM32(STACK)==0x12345678&&g_ebx==0x1111&&g_esi==0x2222&&g_edi==0x3333);}
static void primary_queue(void)
{stack();MEM32(STACK+4)=PRIMARY;sub_001648D0();returned();}
static void secondary_queue(void)
{stack();MEM32(STACK+4)=SECONDARY;sub_00164920();returned();}
static void setup(unsigned scene,unsigned choice)
{
 MEM32(0x79094c)=scene;MEM32(0x7de458)=0;
 MEM32(0x72f5e8)=POOL;MEM32(PRIMARY+0x2c)=0x400001;
 MEM32(SECONDARY+0x2c)=0x800001;
 MEM32(PRIMARY+0x7c)=DATA;MEM32(SECONDARY+0x7c)=DATA+0x100;
 MEM32(VOICES)=3;MEM32(VOICES+4)=0x1008;MEM32(VOICES+8)=0x1b25;MEM32(VOICES+12)=0x1009;
 primary_id=MEM32(VOICES+4+4*choice);default_id=scene==4?0x1b26:0x10e8;
 initial_seed=0x1234567800000001ull;
 unsigned attempts;
 for(attempts=0;attempts<32;attempts++) {expected_seed=initial_seed*0x343fdull+0x269ec3ull;if((uint32_t)expected_seed%3==choice)break;++initial_seed;}
 assert(attempts<32);
 MEM32(0x4e7950)=(uint32_t)initial_seed;MEM32(0x4e7954)=(uint32_t)(initial_seed>>32);MEM32(0x7497c8)=0;
 MEM32(0x7497cc)=0xabc;MEM32(0x7497d0)=0xdef;
 primary_events=secondary_events=lookups=0;elapsed=primary_time=secondary_time=0;
 stack();sub_00164960();returned();MEMF(0x74c3c8)=0;
 primary_queue();assert(MEM32(0x74c3f0)==0x400001&&MEMF(0x74c3d0)==1.f);
}
static void frame(unsigned whole,float delta)
{
 MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
 uint32_t current=MEM32(0x7497cc),saved=MEM32(0x7497d0);
 stack();sub_001CE2E0();returned();
 assert(MEM32(0x4e7944)==whole&&MEMF(0x4e794c)==delta);
 /* Actual153C00 saves/restores the separate float RNG; it does not seed it
  * from the tick count. The voice scheduler uses the independent main RNG. */
 assert(MEM32(0x7497cc)==(whole?current:saved));
 assert(MEM32(0x7497d0)==(whole?current:saved));
}
int main(int argc,char **argv)
{
 legacy_gate=argc>1&&!strcmp(argv[1],"legacy-gate");ignore_generation=argc>1&&!strcmp(argv[1],"ignore-generation");
 setbuf(stdout,NULL);void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
 /* CONSTANTS */
 MEM32(0x4e8cec)=60;
 const unsigned rates[]={60,60,120,144,180,240};unsigned cases=0;double max_error=0;
 for(unsigned r=0;r<6;r++)for(unsigned slow=1;slow<=4;slow*=4)
 for(unsigned phase=0;phase<4;phase++)for(unsigned choice=0;choice<3;choice++)for(unsigned stale=0;stale<4;stale++) {
  unsigned den=rates[r]*slow,previous=0;g_tsfp_native_timing_active=r!=0;setup(4+(choice%2)*2,choice);
  MEMF(0x74c3c8)=5.f;
  for(unsigned f=1;f<=den/4;f++) {
   unsigned tick=(f*240+phase*den)/(den*4),whole=tick-previous;previous=tick;
   frame(whole,r?60.f/den:(whole?1.f:0.f));
  }
  printf("actor_voice fps=%u native=%u slow=%u phase=%u choice=%u stale=%u consumed=%.8f\n",rates[r],r!=0,slow,phase,choice,stale,1.f-MEMF(0x74c3d0));
  assert(fabsf(MEMF(0x74c3d0)-.75f)<.002f&&fabsf(MEMF(0x74c3c8)-4.75f)<.002f);
  assert(!primary_events&&!secondary_events&&MEM32(0x7497c8)==0);
  setup(4+(choice%2)*2,choice);secondary_queue();assert(MEM32(0x74c3ec)==0x800001);
  if(stale&1)++MEM32(PRIMARY+0x2c);if(stale&2)++MEM32(SECONDARY+0x2c);
  previous=0;
  for(unsigned f=1;f<=den*7;f++) {
   unsigned tick=(f*240+phase*den)/(den*4),whole=tick-previous;previous=tick;
   elapsed=(double)f/den;float delta=r?60.f/den:(whole?1.f:0.f);frame(whole,delta);
   if(r&&f%101==0) {
    uint32_t low=MEM32(0x4e7950),high=MEM32(0x4e7954),count=MEM32(0x7497c8);
    float a=MEMF(0x74c3d0),b=MEMF(0x74c3c8);unsigned p=primary_events,s=secondary_events;
    frame(1,0);assert(MEM32(0x4e7950)==low&&MEM32(0x4e7954)==high&&MEM32(0x7497c8)==count);
    assert(MEMF(0x74c3d0)==a&&MEMF(0x74c3c8)==b&&primary_events==p&&secondary_events==s);
   }
   if(!stale&&f==den) {assert(MEMF(0x74c3c8)>0&&!MEM32(0x74c3ec));float before=MEMF(0x74c3c8);secondary_queue();assert(!MEM32(0x74c3ec)&&MEMF(0x74c3c8)==before);}
  }
  assert(primary_events==!(stale&1)&&secondary_events==!(stale&2));
  assert(!MEM32(0x74c3f0)&&!MEM32(0x74c3ec));
  uint64_t seed=(uint64_t)MEM32(0x4e7950)|((uint64_t)MEM32(0x4e7954)<<32);
  assert(seed==((stale&1)?initial_seed:expected_seed)&&MEM32(0x7497c8)==!(stale&1));
  assert(lookups==((stale&1)?1:2));
  if(primary_events){double e=fabs(primary_time-1.);if(e>max_error)max_error=e;assert(e<2./60+1./den);}
  if(secondary_events){double e=fabs(secondary_time-.2);if(e>max_error)max_error=e;assert(e<2./60+1./den);assert(MEMF(0x74c3c8)<=0);}
  if(!stale){secondary_queue();assert(MEM32(0x74c3ec)==0x800001&&MEMF(0x74c3c8)==MEMF(0x4785e4));}
  ++cases;
 }
 printf("actor_voice cases=%u max_event_error=%.9f\n",cases,max_error);
 unsigned parity=0;
 for(unsigned choice=0;choice<3;choice++)for(unsigned due=0;due<4;due++)for(unsigned whole=1;whole<=3;whole++) {
  uint32_t original[25];
  for(unsigned native=0;native<2;native++) {
   g_tsfp_native_timing_active=native;setup(4+(choice%2)*2,choice);secondary_queue();
   MEMF(0x74c3d0)=due&1?.005f:1.f;MEMF(0x74c3c8)=due&2?.005f:5.f;
   elapsed=whole/60.;frame(whole,(float)whole);
   uint32_t observed[25];
   for(unsigned i=0;i<17;i++)observed[i]=MEM32(0x74c3c8+4*i);
   observed[17]=MEM32(0x4e7950);observed[18]=MEM32(0x4e7954);observed[19]=MEM32(0x7497c8);
   observed[20]=MEM32(0x7497cc);observed[21]=MEM32(0x7497d0);
   observed[22]=primary_events;observed[23]=secondary_events;observed[24]=lookups;
   if(!native)memcpy(original,observed,sizeof original);
   else {assert(!memcmp(original,observed,sizeof original));++parity;}
  }
 }
 printf("actor_voice whole_catchup_byte_comparisons=%u\n",parity);
 free(memory);puts("PASS: original frame/voice queues, deferred delivery, cooldown, main RNG, stale handles, pause and ABI");return 0;
}
