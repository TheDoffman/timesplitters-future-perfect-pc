#ifndef TSFP_PC_NETWORK_BOT_MOTION_H
#define TSFP_PC_NETWORK_BOT_MOTION_H
#include "pc_network_bots_state.h"
#include <string.h>

/* Presentation only. Never predict AI, advance game clocks or interpolate
 * health, scores, ownership, shots, spawn IDs, or absence/death transitions.
 * Retarget from the current displayed transform rather than jumping backwards
 * to the preceding packet when arrival intervals vary. Stop at the last known
 * host transform on loss; a new body/session always starts without history. */
typedef struct PcNetworkBotMotion {
    PcNetworkBotState from, target;
    double received, duration;
    int ready;
} PcNetworkBotMotion;

static float pc_network_bot_motion_angle(float a,float b,float t)
{
    const float pi=3.14159265358979323846f;
    float delta=fmodf(b-a,2.f*pi);
    if(delta>pi) delta-=2.f*pi;
    if(delta< -pi) delta+=2.f*pi;
    return a+t*delta;
}

static int pc_network_bot_motion_sample(const PcNetworkBotMotion *motion,
                                        double now,PcNetworkBotState *out)
{
    float t;unsigned i;
    if(!motion->ready) return 0;
    *out=motion->target;
    t=motion->duration>0. ? (float)((now-motion->received)/motion->duration) : 1.f;
    if(t>=1.f) return 1;
    if(t<0.f) t=0.f;
#define BOT_MOTION_VECTOR(field,count) \
    for(i=0;i<count;i++) out->field[i]=motion->from.field[i]+t*(motion->target.field[i]-motion->from.field[i])
    BOT_MOTION_VECTOR(position,3);
    BOT_MOTION_VECTOR(bounds_center,3);
    BOT_MOTION_VECTOR(muzzle,3);
    BOT_MOTION_VECTOR(aim,3);
    for(i=0;i<3;i++) out->angles[i]=pc_network_bot_motion_angle(motion->from.angles[i],motion->target.angles[i],t);
    out->heading=pc_network_bot_motion_angle(motion->from.heading,motion->target.heading,t);
    /* US sub_0005ADC0 consumes clip+8 (model DC/134) as frame time.
       Interpolate only that verified scalar, within the same clip/cycle.
       Other channels and crossfade controls form an atomic animator state:
       5CEF0 can promote the secondary clip when elapsed reaches duration,
       even in zero-advance evaluation. Mixing fades can select a stale clip. */
    if(motion->from.clip==motion->target.clip && motion->from.clip_cycle==motion->target.clip_cycle) {
        out->clip_channels[0]=motion->from.clip_channels[0]+t*(motion->target.clip_channels[0]-motion->from.clip_channels[0]);
    }
    if(motion->from.next_clip==motion->target.next_clip && motion->from.next_cycle==motion->target.next_cycle) {
        out->next_channels[0]=motion->from.next_channels[0]+t*(motion->target.next_channels[0]-motion->from.next_channels[0]);
    }
#undef BOT_MOTION_VECTOR
    return 1;
}

static void pc_network_bot_motion_push(PcNetworkBotMotion *motion,
                                       const PcNetworkBotState *state,double now)
{
    PcNetworkBotState displayed;
    double elapsed=now-motion->received;
    int continuous=pc_network_bot_motion_sample(motion,now,&displayed) &&
        state->spawn==motion->target.spawn && state->slot==motion->target.slot &&
        state->body_present && motion->target.body_present &&
        state->health>0.f && motion->target.health>0.f &&
        (state->entity_flags&1u) && (motion->target.entity_flags&1u) &&
        elapsed>=0. && elapsed<=.25;
    /* A long gap is a resynchronization, not permission to animate a stale
       trajectory. Ordinary three-tick packets use a 50 ms presentation span. */
    motion->from=continuous?displayed:*state;
    motion->duration=continuous?.05:0.;
    motion->target=*state;
    motion->received=now;
    motion->ready=1;
}
#endif
