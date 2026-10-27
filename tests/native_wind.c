#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail_allocation;
static unsigned allocations;
static void *owner_realloc(void *p,size_t n){++allocations;return fail_allocation?NULL:realloc(p,n);}
#define PC_NATIVE_WIND_REALLOC owner_realloc
#include "pc_native_wind_owners.h"
#include "pc_native_wind_clock.h"
static PcWindOwner *reg(PcWindOwners *s,uint32_t owner){return pc_wind_owner_register(s,1,4,owner,owner+0x2000,7,1);}
static PcWindOwner *begin(PcWindOwners *s,uint32_t owner,uint32_t tick,int64_t phase){return pc_wind_owner_begin(s,1,4,owner,owner+0x2000,7,tick,phase,720);}
static void prepared(PcWindOwner *p){assert(p);p->axes[0].prepared=p->axes[1].prepared=1;p->axes[0].anchor=10;p->axes[1].anchor=350;}
static void cleared(PcWindOwner *p){assert(p&&!p->axes[0].prepared&&!p->axes[1].prepared&&!p->clock_valid);}

static void fractional_maps(void){
 const unsigned divisions[]={2,3,4,12};
 for(unsigned d=0;d<4;d++){
  unsigned count=divisions[d];float h=1.f/count,a=10,v=0;double reference_a=10,reference_v=0;
  PcWindHistory history={0};
  for(unsigned tick=0;tick<20;tick++){
   /* Independent authored low-angle recurrence: force, drag, restoring, then
    * derive new velocity from the angle change. This stays inside one segment. */
   double next=(1.-1./90.)*(reference_a+.9*(reference_v+.2));
   reference_v=next-reference_a;reference_a=next;
   for(unsigned i=0;i<count;i++){
    PcNativeWindAxis out;
    assert(pc_native_wind_axis(a,v+.2f*h,.2f,.9f,1.f,80.f,h,i+1==count,&out,&history,(double)i/count,1./count));
    a=out.angle;v=out.velocity;
   }
   assert(fabs(a-reference_a)<.001&&fabs(v-reference_v)<.001);
  }
 }
}
static void exact_clocks(void){
 PcNativeStep step={0};step.units_per_tick=720;step.game_units=180;
 assert(pc_native_wind_clock_matches(&step,.25f,0));
 assert(!pc_native_wind_clock_matches(&step,.5f,0));
 assert(!pc_native_wind_clock_matches(&step,.25f,1));
 step.game_phase=540;assert(pc_native_wind_clock_matches(&step,.25f,1));
 assert(!pc_native_wind_clock_matches(&step,.25f,0));
 step.game_units=0;assert(!pc_native_wind_clock_matches(&step,.25f,0));
 step.units_per_tick=INT64_MAX;step.game_phase=INT64_MAX-2;step.game_units=4;
 assert(!pc_native_wind_clock_matches(&step,.25f,0));
 assert(!pc_native_wind_clock_matches(NULL,.25f,0));
}

int main(void){fractional_maps();exact_clocks();
 PcWindOwners s={0};assert(!begin(&s,0x1000,0,0));assert(!allocations);
 reg(&s,0x2000);reg(&s,0x1000);assert(s.count==2&&s.entries[0].owner==0x1000&&s.entries[1].owner==0x2000);
 PcWindOwner *a=begin(&s,0x1000,0,0);prepared(a);assert(pc_wind_owner_end(a,0,0,180,720));
 PcWindOwner *b=begin(&s,0x2000,0,0);cleared(b);prepared(b);b->axes[0].anchor=50;
 a=begin(&s,0x1000,0,180);assert(a->axes[0].prepared&&a->axes[0].anchor==10);assert(b->axes[0].anchor==50);
 /* Several paused host frames produce no begin/end calls. Same game stamp resumes intact. */
 PcWindOwner saved=*a;a=begin(&s,0x1000,0,180);assert(!memcmp(a,&saved,sizeof saved));
 /* A skipped advancing interval invalidates both axes, including a sub-tick gap. */
 cleared(begin(&s,0x1000,0,360));prepared(a);assert(pc_wind_owner_end(a,0,360,180,720));
 cleared(begin(&s,0x1000,1,540));
 /* Re-registration at exactly the same pointers/handle is a new generation. */
 prepared(a);cleared(reg(&s,0x1000));
 a=begin(&s,0x1000,0,0);prepared(a);assert(pc_wind_owner_end(a,0,0,180,720));
 cleared(pc_wind_owner_begin(&s,1,4,0x1000,0x8000,7,0,180,720));
 prepared(a);cleared(pc_wind_owner_begin(&s,1,4,0x1000,0x8000,8,0,180,720));
 prepared(a);assert(pc_wind_owner_end(a,0,180,180,720));cleared(pc_wind_owner_begin(&s,1,4,0x1000,0x8000,8,0,360,1440));
 /* Unregister another slot must be filtered by the caller; slot3 removal releases ownership. */
 pc_wind_owner_register(&s,1,4,0x1000,0,0,0);assert(s.count==1&&!begin(&s,0x1000,0,0));
 reg(&s,0x1000);cleared(begin(&s,0x1000,0,0));
 assert(!pc_wind_owner_begin(&s,1,5,0x1000,0x3000,7,0,0,720)&&s.count==0);
 reg(&s,0x1000);assert(!pc_wind_owner_begin(&s,2,4,0x1000,0x3000,7,0,0,720)&&s.count==0);
 /* Growth failure retains every live owner and performs no callback-side allocation. */
 for(unsigned i=0;i<16;i++)assert(reg(&s,0x1000+i*16));
 PcWindOwner before[16];memcpy(before,s.entries,sizeof before);fail_allocation=1;
 assert(!reg(&s,0x5000));assert(s.count==16&&!memcmp(before,s.entries,sizeof before));
 unsigned n=allocations;for(unsigned i=0;i<16;i++)assert(begin(&s,0x1000+i*16,0,0));assert(n==allocations);
 fail_allocation=0;assert(reg(&s,0x5000)&&s.count==17);
 a=begin(&s,0x1000,0,0);prepared(a);assert(!pc_wind_owner_end(a,0,0,1,0));cleared(a);
 prepared(a);assert(!pc_wind_owner_end(a,0,700,INT64_MAX,720));cleared(a);
 /* Whole-tick rollover uses guest uint32 arithmetic; zero-time pause keeps phase. */
 prepared(a);assert(pc_wind_owner_end(a,UINT32_MAX,540,180,720));
 a=begin(&s,0x1000,0,0);assert(a->axes[0].prepared);assert(pc_wind_owner_end(a,0,0,0,720));
 a=begin(&s,0x1000,0,0);assert(a->axes[0].prepared);
 free(s.entries);puts("PASS owner isolation/reuse/removal, memory/scene/clock resets, pause, growth failure/retry and tick wrap");return 0;
}
