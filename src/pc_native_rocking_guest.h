#ifndef TSFP_PC_NATIVE_ROCKING_GUEST_H
#define TSFP_PC_NATIVE_ROCKING_GUEST_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
extern ptrdiff_t g_xbox_mem_offset;
extern int g_tsfp_native_timing_active;
#if defined(_MSC_VER)
#define PC_ROCK_TLS __declspec(thread)
#else
#define PC_ROCK_TLS _Thread_local
#endif
#define ROCK_WORD(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
#define ROCK_FLOAT(a) (*(float *)((uintptr_t)(a)+g_xbox_mem_offset))

/* US A5AF0 clears capacity[72F5E4] objects of stride 2E8 at [72F5E8].
 * Host state never occupies guest padding. Initializers and pool resets
 * invalidate reused slots; unknown entities retain their original updater. */
typedef struct PcRockingState {
    double phase;
    float anchor[4], expected[6], hidden_height;
    unsigned ready;
} PcRockingState;
typedef struct PcRockingContext {
    PcRockingState state;
    uint32_t entity;
    double remaining, span;
    unsigned iterations, done, boundary;
    float value[4];
} PcRockingContext;
static PcRockingState *pc_rocking_slots;
static uint32_t pc_rocking_base, pc_rocking_count;
static PC_ROCK_TLS PcRockingContext *pc_rocking_context;
static const unsigned pc_rocking_offsets[]={0x220,0x224,0xd4,0xdc,0xe0,0xe8};

static PcRockingState *pc_rocking_slot(uint32_t entity,int create)
{
    uint32_t base=ROCK_WORD(0x72F5E8u), count=ROCK_WORD(0x72F5E4u);
    if(!base || !count || count>0x4000000u/0x2E8u ||
       base>UINT32_MAX-count*0x2E8u || entity<base ||
       (entity-base)%0x2E8u || (entity-base)/0x2E8u>=count)
        return NULL;
    if(base!=pc_rocking_base || count!=pc_rocking_count) {
        if(!create) {
            free(pc_rocking_slots);pc_rocking_slots=NULL;
            pc_rocking_base=pc_rocking_count=0;return NULL;
        }
        PcRockingState *slots=calloc(count,sizeof *slots);
        if(!slots)return NULL;
        free(pc_rocking_slots);pc_rocking_slots=slots;
        pc_rocking_base=base;pc_rocking_count=count;
    }
    return &pc_rocking_slots[(entity-base)/0x2E8u];
}
static void pc_rocking_anchor(PcRockingState *state,uint32_t entity)
{
    for(unsigned i=0;i<4;i++)state->anchor[i]=ROCK_FLOAT(entity+pc_rocking_offsets[i]);
}
static void pc_rocking_remember(PcRockingState *state,uint32_t entity)
{
    for(unsigned i=0;i<6;i++)state->expected[i]=ROCK_FLOAT(entity+pc_rocking_offsets[i]);
}
static void pc_rocking_project(PcRockingState *state,uint32_t entity)
{
    state->hidden_height=fminf(0,ROCK_FLOAT(entity+0x220));
    if(state->hidden_height<0)ROCK_FLOAT(entity+0x220)=0;
    pc_rocking_remember(state,entity);
}
static void pc_rocking_restore(PcRockingState *state,uint32_t entity)
{
    if(state->hidden_height<0 && ROCK_FLOAT(entity+0x220)==state->expected[0])
        ROCK_FLOAT(entity+0x220)=state->hidden_height;
    state->hidden_height=0;
}
/* An impulse can arrive partway through an original contact interval.
 * Rebase the affine flow at the current phase, rather than restarting its
 * contact clock or discarding the new velocity. Whole-step starts stay exact. */
static void pc_rocking_rebase(PcRockingState *state,uint32_t entity)
{
    pc_rocking_anchor(state,entity);
    if(state->phase>0) {
        double d=state->phase,dt=1.0/ROCK_WORD(0x4E8CECu);
        double r=1-ROCK_FLOAT(0x47D56Cu)*dt,q=ROCK_FLOAT(0x47D570u)*dt;
        double a=ROCK_FLOAT(0x475CC4u)*dt,b=ROCK_FLOAT(0x47D578u)*dt*dt;
        double rd=pow(r,d),s=-expm1(d*log(r))/(1-r);
        double v=(state->anchor[1]+q*s)/rd;
        state->anchor[0]=(float)(state->anchor[0]-a*s*v+b*d+a*q/(1-r)*(d-s));
        state->anchor[1]=(float)v;
        state->anchor[2]-=(float)(ROCK_FLOAT(entity+0xe0)*d);
        state->anchor[3]-=(float)(ROCK_FLOAT(entity+0xe8)*d);
    }
}
static int pc_rocking_settled(uint32_t entity)
{
    /* Match A8521..A856E, including its repeated D4 upper-bound check. */
    return fabsf(ROCK_FLOAT(entity+0x224))<=ROCK_FLOAT(0x475CDCu) &&
        ROCK_FLOAT(entity+0xd4)>ROCK_FLOAT(0x47D3A4u) &&
        ROCK_FLOAT(entity+0xd4)<ROCK_FLOAT(0x475DBCu) &&
        ROCK_FLOAT(entity+0xdc)>ROCK_FLOAT(0x47D3A4u) &&
        ROCK_FLOAT(entity+0x220)<=0;
}
static void pc_rocking_next(PcRockingContext *ctx)
{
    PcRockingState *state=&ctx->state;
    double edge=1-state->phase;
    ctx->span=fmin(ctx->remaining,edge);ctx->boundary=ctx->span>=edge;
    double d=ctx->boundary?1:state->phase+ctx->span;
    float h=state->anchor[0],v=state->anchor[1];
    if(ctx->boundary) {
        /* Same float operations/order as A85B0..A8659. Anchor evaluation
         * prevents fractional rounding from changing contact/audio events. */
        float dt=1.f/(float)ROCK_WORD(0x4E8CECu);
        float a=v*ROCK_FLOAT(0x475CC4u),b=dt*ROCK_FLOAT(0x47D578u);
        ctx->value[0]=(a-b)*dt+h;
        a=dt*ROCK_FLOAT(0x47D570u);b=dt*v;
        b=b*ROCK_FLOAT(0x47D56Cu);ctx->value[1]=(v-a)-b;
    } else {
        double dt=1.0/ROCK_WORD(0x4E8CECu);
        double r=1-ROCK_FLOAT(0x47D56Cu)*dt,q=ROCK_FLOAT(0x47D570u)*dt;
        double a=ROCK_FLOAT(0x475CC4u)*dt,b=ROCK_FLOAT(0x47D578u)*dt*dt;
        double logstep=d*log(r),s=-expm1(logstep)/(1-r);
        ctx->value[0]=(float)(h+a*s*v-b*d-a*q/(1-r)*(d-s));
        ctx->value[1]=(float)(exp(logstep)*v-q*s);
    }
    ctx->value[2]=state->anchor[2]+(float)(ROCK_FLOAT(ctx->entity+0xe0)*d);
    ctx->value[3]=state->anchor[3]+(float)(ROCK_FLOAT(ctx->entity+0xe8)*d);
}
uint32_t pc_native_rocking_gate(uint32_t site,uint32_t value)
{
    PcRockingContext *ctx=pc_rocking_context;
    if(!ctx)return value;
    if(site==0xA8595u) {
        double delta=ROCK_FLOAT(0x4E794Cu);
        if(delta==0)return 0;
        double end=ctx->state.phase+delta,nearest=floor(end+.5);
        /* Float frame deltas accumulate tiny errors at exact tick boundaries.
         * Never snap a positive interval to zero. */
        if(nearest>0 && fabs(end-nearest)<1e-6)end=nearest;
        ctx->remaining=end-ctx->state.phase;
        ctx->iterations=(unsigned)ceil(end);ctx->done=0;
        if(ctx->state.phase==0)pc_rocking_anchor(&ctx->state,ctx->entity);
        pc_rocking_next(ctx);return ctx->iterations;
    }
    ctx->done++;ctx->remaining-=ctx->span;
    if(ctx->boundary) {
        ctx->state.phase=0;pc_rocking_anchor(&ctx->state,ctx->entity);
        /* Do not start another fractional fall after reaching the original
         * settled state. The next live callback performs original retirement. */
        if(ctx->remaining<1 && pc_rocking_settled(ctx->entity)) {
            ctx->remaining=0;ctx->iterations=ctx->done;
        }
    } else ctx->state.phase+=ctx->span;
    if(ctx->done<ctx->iterations)pc_rocking_next(ctx);
    else pc_rocking_project(&ctx->state,ctx->entity);
    return ctx->iterations;
}
int pc_native_rocking_branch(uint32_t site,int taken)
{
    PcRockingContext *ctx=pc_rocking_context;
    if(ctx && ((site==0xA8526u && ctx->state.phase>0) ||
               (site==0xA86C0u && !ctx->boundary)))return 1;
    return taken;
}
float pc_native_rocking_add(uint32_t site,float left,float right)
{
    PcRockingContext *ctx=pc_rocking_context;
    if(!ctx)return left+right;
    return ctx->value[site==0xA85F2u?0:site==0xA8651u?2:3];
}
float pc_native_rocking_sub(float left,float right)
{ return pc_rocking_context?pc_rocking_context->value[1]:left-right; }

static void pc_native_rocking_update_guest(void (*original)(void),uint32_t entity)
{
    PcRockingContext *saved=pc_rocking_context,ctx={0};
    PcRockingState *slot=pc_rocking_slot(entity,g_tsfp_native_timing_active);
    float delta=ROCK_FLOAT(0x4E794Cu);
    pc_rocking_context=NULL;
    if(slot && g_tsfp_native_timing_active && isfinite(delta) && delta>=0 &&
       delta<=6 && ROCK_WORD(0x4E8CECu)==60) {
        ctx.entity=entity;ctx.state=*slot;
        if(!ctx.state.ready) {
            memset(&ctx.state,0,sizeof ctx.state);ctx.state.ready=1;
            pc_rocking_anchor(&ctx.state,entity);pc_rocking_remember(&ctx.state,entity);
        }
        if(delta>0) {
            int changed=0;
            for(unsigned i=0;i<6;i++)
                changed|=ROCK_FLOAT(entity+pc_rocking_offsets[i])!=ctx.state.expected[i];
            pc_rocking_restore(&ctx.state,entity);
            if(changed)pc_rocking_rebase(&ctx.state,entity);
        }
        pc_rocking_context=&ctx;
    } else if(slot)memset(slot,0,sizeof *slot);
    original();
    if(pc_rocking_context) {
        /* Resolve again: a nested callback may have reallocated the pool. */
        slot=pc_rocking_slot(entity,g_tsfp_native_timing_active);
        if(slot) {
            if(ROCK_WORD(entity+0x16c)==0xA84F0u) {
                pc_rocking_remember(&ctx.state,entity);*slot=ctx.state;
            } else memset(slot,0,sizeof *slot);
        }
    }
    pc_rocking_context=saved;
}
static void pc_native_rocking_init_guest(void (*original)(void),uint32_t entity)
{
    PcRockingState *slot=pc_rocking_slot(entity,g_tsfp_native_timing_active);
    float before[6]={0};int had_state=slot && slot->ready;
    if(slot && (!g_tsfp_native_timing_active || ROCK_WORD(entity+0x16c)!=0xA84F0u))
        memset(slot,0,sizeof *slot);
    if(slot && slot->ready)pc_rocking_restore(slot,entity);
    if(had_state)for(unsigned i=0;i<6;i++)before[i]=ROCK_FLOAT(entity+pc_rocking_offsets[i]);
    original();
    slot=pc_rocking_slot(entity,g_tsfp_native_timing_active);
    if(slot && slot->ready) {
        int changed=0;
        for(unsigned i=0;i<6;i++)changed|=before[i]!=ROCK_FLOAT(entity+pc_rocking_offsets[i]);
        if(changed)pc_rocking_rebase(slot,entity);
        pc_rocking_project(slot,entity);
    }
}
static void pc_native_rocking_reset_guest(void (*original)(void))
{
    if(pc_rocking_slots)memset(pc_rocking_slots,0,pc_rocking_count*sizeof *pc_rocking_slots);
    original();
}
#undef ROCK_WORD
#undef ROCK_FLOAT
#undef PC_ROCK_TLS
#endif
