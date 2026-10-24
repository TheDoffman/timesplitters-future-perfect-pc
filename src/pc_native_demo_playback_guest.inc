#include "pc_native_timing.h"
#include <string.h>
#ifndef PC_NATIVE_DEMO_READ
#include "pc_guest_memory.h"
static int pc_native_demo_read(uint32_t address,void *out,unsigned bytes)
{
    if(address<0x10000u || address>0x84000000u || bytes>0x84000000u-address)return 0;
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    if(bytes>UINTPTR_MAX-host)return 0;
    unsigned char *target=out;
    while(bytes) {
        size_t available=pc_guest_region(host,0);
        if(!available)return 0;
        unsigned count=available<bytes?(unsigned)available:bytes;
        memcpy(target,(const void *)host,count);
        host+=count;target+=count;bytes-=count;
    }
    return 1;
}
#define PC_NATIVE_DEMO_READ pc_native_demo_read
#endif
extern void pc_native_visual_rebase(uint32_t value,double fraction);

typedef struct PcNativeDemoBundle {
    uint32_t selected,next,level,raw_time,raw,game;
} PcNativeDemoBundle;
typedef struct PcNativeDemoPlan {
    PcNativeDemoBundle packet;
    int64_t frequency,phase,units,raw_units,game_units;
    uint32_t slot[4],prior[4];
    unsigned generation;
    int valid,zero;
} PcNativeDemoPlan;
static RECOMP_MANUAL_TLS struct {
    uint32_t base,size,channels,current,expected;
    int64_t frequency,phase,debt;
    int valid;
} pc_native_demo_playback;

static void pc_native_demo_playback_reset(void)
{
    pc_native_demo_playback.valid=0;
}

/* Old native recordings contain zero-tick groups. Fold their pending game
 * counts into the following authored group; physical digital polling held
 * buttons between raw ticks. The original decoder still owns input decoding.
 * Bound scans of malformed/all-zero streams and never cache VM permissions. */
static int pc_native_demo_bundle(uint32_t cursor,PcNativeDemoBundle *out)
{
    uint32_t stride=14u*pc_native_demo_playback.channels;
    uint64_t end=(uint64_t)pc_native_demo_playback.base+pc_native_demo_playback.size;
    unsigned game=0;uint32_t level=0;
    for(unsigned n=0;n<128;n++) {
        unsigned char bytes[56];
        if((uint64_t)cursor+stride>end || !PC_NATIVE_DEMO_READ(cursor,bytes,stride))return 0;
        const unsigned char *last=bytes+stride-14;
        if(!n)level=last[2]|((uint32_t)last[3]<<8);
        game+=last[1];
        if(game>255u)return 0;
        if(last[0]) {
            *out=(PcNativeDemoBundle){cursor,cursor+stride,level,
                last[4]|((uint32_t)last[5]<<8),last[0],game};
            return 1;
        }
        cursor+=stride;
    }
    return 0;
}

static PcNativeDemoPlan pc_native_demo_playback_begin(int active,uint32_t mode,
                                                     uint32_t ticks,float delta)
{
    PcNativeDemoPlan plan={0};
    if(mode!=1u) {
        pc_native_demo_playback_reset();return plan;
    }
    uint32_t base=ND_WORD(0x6B989Cu),size=ND_WORD(0x6B98A0u);
    uint32_t cursor=ND_WORD(0x6B94C0u),channels=ND_WORD(0x6B98A8u);
    uint64_t end=(uint64_t)base+size;
    if(base<0x10000u || end>0x84000000u || channels<1 || channels>4 ||
       cursor<base || cursor>end || (cursor-base)%(14u*channels)) {
        pc_native_demo_playback_reset();return plan;
    }
    /* Console recordings have positive raw ticks. Keep their Original60
     * decoder unchanged; a leading zero identifies an old native stream that
     * needs the same zero-group repair even when opened in Original60. */
    int legacy=0;
    if(!active && !pc_native_demo_playback.valid) {
        unsigned char bytes[56];unsigned stride=14u*channels;
        if((uint64_t)cursor+stride>end || !PC_NATIVE_DEMO_READ(cursor,bytes,stride) ||
           bytes[stride-14]!=0)return plan;
        legacy=1;
    }
    int64_t f=active?g_tsfp_native_step.units_per_tick:
        (legacy?60000000:pc_native_demo_playback.frequency);
    if(f<=0 || f>INT64_MAX/1024 || ticks>3u) {
        pc_native_demo_playback_reset();return plan;
    }
    int64_t units=active?g_tsfp_native_step.raw_units:(int64_t)ticks*f;
    if(units<0 || units>3*f ||
       (active && (g_tsfp_native_step.raw_delta!=delta || g_tsfp_native_step.raw_ticks!=ticks))) {
        pc_native_demo_playback_reset();return plan;
    }
    if(!pc_native_demo_playback.valid || pc_native_demo_playback.base!=base ||
       pc_native_demo_playback.size!=size || pc_native_demo_playback.channels!=channels ||
       pc_native_demo_playback.expected!=cursor) {
        pc_native_demo_playback.base=base;pc_native_demo_playback.size=size;
        pc_native_demo_playback.channels=channels;pc_native_demo_playback.current=cursor;
        pc_native_demo_playback.expected=cursor;pc_native_demo_playback.phase=0;
        pc_native_demo_playback.debt=0;pc_native_demo_playback.frequency=f;
        pc_native_demo_playback.valid=1;
    } else if(pc_native_demo_playback.frequency!=f) {
        long double scale=(long double)f/pc_native_demo_playback.frequency;
        pc_native_demo_playback.phase=(int64_t)(pc_native_demo_playback.phase*scale);
        pc_native_demo_playback.debt=(int64_t)(pc_native_demo_playback.debt*scale);
        if(pc_native_demo_playback.phase>=f)pc_native_demo_playback.phase=f-1;
        pc_native_demo_playback.frequency=f;
    }
    plan.frequency=f;plan.phase=pc_native_demo_playback.phase;
    plan.generation=pc_native_demo_generation;
    if(!pc_native_demo_bundle(pc_native_demo_playback.current,&plan.packet)) {
        /* Let the original EOF/cancellation path run without reading a partial
         * group or an inaccessible stream. Do not publish a fabricated packet. */
        ND_WORD(0x6B94C0u)=(uint32_t)end;
        pc_native_demo_playback_reset();return plan;
    }
    for(unsigned c=0;c<channels;c++) {
        plan.slot[c]=ND_WORD(0x6B93C0u+c*4u);
        if(plan.slot[c]>=4u) { pc_native_demo_playback_reset();return (PcNativeDemoPlan){0}; }
        plan.prior[c]=ND_WORD(0x6B93D8u+plan.slot[c]*0x3cu);
    }
    plan.zero=units==0;
    /* At most one authored group advance per game update prevents a catch-up
     * interval from silently skipping a press/release packet. Repay bounded
     * debt on later native frames; normal <1-tick intervals remain unchanged. */
    int64_t available=plan.zero?0:units+pc_native_demo_playback.debt;
    if(available>3*f)available=3*f;
    plan.units=available>f?f:available;
    if(!plan.zero)pc_native_demo_playback.debt=available-plan.units;
    int64_t first=plan.units<f-plan.phase?plan.units:f-plan.phase;
    plan.raw_units=first*plan.packet.raw;plan.game_units=first*plan.packet.game;
    if(plan.units>first) {
        PcNativeDemoBundle next;
        if(pc_native_demo_bundle(plan.packet.next,&next)) {
            plan.raw_units+=(plan.units-first)*next.raw;
            plan.game_units+=(plan.units-first)*next.game;
        } else plan.raw_units+=plan.units-first; /* EOF freezes game, not UI time. */
    }
    if(!plan.zero)ND_WORD(0x6B94C0u)=plan.packet.selected;
    plan.valid=1;return plan;
}

static void pc_native_demo_playback_end(const PcNativeDemoPlan *plan)
{
    if(!plan->valid || !pc_native_demo_playback.valid || plan->generation!=pc_native_demo_generation)return;
    if(ND_WORD(0x6B98A4u)!=1u || ND_WORD(0x6B989Cu)!=pc_native_demo_playback.base ||
       ND_WORD(0x6B98A0u)!=pc_native_demo_playback.size ||
       ND_WORD(0x6B98A8u)!=pc_native_demo_playback.channels ||
       (!plan->zero && ND_WORD(0x6B94C0u)!=plan->packet.next)) {
        pc_native_demo_playback_reset();return;
    }
    if(plan->zero) {
        /* Polling must not steal the first recorded edge on a zero-time frame.
         * Preserve prior held state; the next advancing decoder emits it once. */
        for(unsigned c=0;c<pc_native_demo_playback.channels;c++) {
            uint32_t record=0x6B93D0u+plan->slot[c]*0x3cu;
            ND_WORD(record+8u)=ND_WORD(record+12u)=plan->prior[c];
            ND_WORD(record+16u)=ND_WORD(record+20u)=0;
        }
        return;
    }
    int64_t f=plan->frequency,phase=plan->phase;
    int64_t raw_phase=phase*plan->packet.raw%f,game_phase=phase*plan->packet.game%f;
    uint32_t level=plan->packet.level+(uint32_t)(phase*plan->packet.game/f);
    ND_WORD(0x7DE318u)=level;
    pc_native_visual_rebase(level,(double)game_phase/f);
    ND_WORD(0x4E7940u)=(uint32_t)((raw_phase+plan->raw_units)/f);
    ND_WORD(0x4E7944u)=(uint32_t)((game_phase+plan->game_units)/f);
    /* The original writer stores raw time AFTER its clock update, but level
     * time BEFORE simulation. Rebase raw time to this interval's end, rather
     * than treating both recorded timestamps as start-of-packet clocks. */
    ND_WORD(0x7497B0u)=(plan->packet.raw_time-plan->packet.raw+
        (uint32_t)(phase*plan->packet.raw/f)+ND_WORD(0x4E7940u))&0xffffu;
    ND_FLOAT(0x4E7948u)=(float)((double)plan->raw_units/f);
    ND_FLOAT(0x4E794Cu)=(float)((double)plan->game_units/f);
    if(g_tsfp_native_timing_active) {
        g_tsfp_native_step=(PcNativeStep){ND_FLOAT(0x4E7948u),ND_FLOAT(0x4E794Cu),
            ND_WORD(0x4E7940u),ND_WORD(0x4E7944u),plan->game_units,game_phase,f,
            plan->raw_units,raw_phase};
    }
    int64_t advanced=phase+plan->units;
    if(advanced>=f)pc_native_demo_playback.current=plan->packet.next;
    pc_native_demo_playback.phase=advanced%f;
    pc_native_demo_playback.expected=ND_WORD(0x6B94C0u);
}
