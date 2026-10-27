#ifndef TSFP_PC_NETWORK_BOTS_STATE_H
#define TSFP_PC_NETWORK_BOTS_STATE_H
#include <stdint.h>
#include <math.h>
/* Wire values only: no guest pointers, controller records or animation handles. */
typedef struct PcNetworkBotShot {
    uint32_t serial,tick,spawn,hand,mode,flags,target_slot;
    int32_t weapon;
    float transform[12],aim[3],muzzle[3];
} PcNetworkBotShot;
static int pc_network_bot_shot_valid(const PcNetworkBotShot *shot,unsigned actors)
{
    unsigned i;
    if(!shot->serial) return 1;
    if(shot->hand>1u || shot->mode>1u || shot->flags>1u || shot->weapon<0 || shot->weapon>=66 ||
       (shot->target_slot>=actors && shot->target_slot!=UINT32_MAX)) return 0;
    for(i=0;i<12;i++) if(!isfinite(shot->transform[i]) || fabsf(shot->transform[i])>1000000.f) return 0;
    for(i=0;i<3;i++) if(!isfinite(shot->aim[i]) || fabsf(shot->aim[i])>1000000.f ||
        !isfinite(shot->muzzle[i]) || fabsf(shot->muzzle[i])>1000000.f) return 0;
    return 1;
}
typedef struct PcNetworkBotState {
    uint32_t slot,tick,entity_flags,flags[3],life[4],animation;
    uint32_t body_present;
    float position[3],health,armor;
    uint32_t last_damage_tick,spawn;
    float bounds_center[3],bounds_radius[3];
    float max_health,health_scale[2];
    float angles[3],heading,aim[3],muzzle[3];
    int32_t clip;
    float clip_channels[18];
    uint32_t clip_cycle,clip_flags;
    int32_t next_clip;
    float next_channels[18];
    uint32_t next_cycle,next_flags,overlay_phase;
    float overlay_channels[8],blend_channels[3];
    PcNetworkBotShot shot;
    int16_t scores[42];
    /* One authoritative victim column, including human kills of this bot.
       Human-vs-human score columns remain owned by the stock protocol. */
    int16_t killed_by[16];
} PcNetworkBotState;
static int pc_network_bot_state_valid(const PcNetworkBotState *state,unsigned humans,unsigned bots)
{
    unsigned i;
    if(!humans || !bots || bots>10 || humans+bots>16 ||
       state->slot<humans || state->slot>=humans+bots) return 0;
    if(state->animation>=70u) return 0; /* equipped weapon enum */
    if(state->body_present>1u) return 0;
    for(i=0;i<3;i++) if(!isfinite(state->position[i]) || fabsf(state->position[i])>1000000.f) return 0;
    for(i=0;i<3;i++) if(!isfinite(state->bounds_center[i]) || fabsf(state->bounds_center[i])>10000.f ||
        !isfinite(state->bounds_radius[i]) || state->bounds_radius[i]<0.f || state->bounds_radius[i]>10000.f) return 0;
    for(i=0;i<2;i++) if(!isfinite(state->health_scale[i]) || fabsf(state->health_scale[i])>1000.f) return 0;
    if(state->clip<0 || state->clip>4095 || state->next_clip<0 || state->next_clip>4095 || state->overlay_phase>2u) return 0;
    for(i=0;i<3;i++) if(!isfinite(state->angles[i]) || fabsf(state->angles[i])>100000.f) return 0;
    if(!isfinite(state->heading) || fabsf(state->heading)>100000.f) return 0;
    for(i=0;i<3;i++) if(!isfinite(state->aim[i]) || fabsf(state->aim[i])>1000000.f ||
        !isfinite(state->muzzle[i]) || fabsf(state->muzzle[i])>1000000.f) return 0;
    for(i=0;i<18;i++) if(!isfinite(state->clip_channels[i]) || fabsf(state->clip_channels[i])>1000000.f ||
        !isfinite(state->next_channels[i]) || fabsf(state->next_channels[i])>1000000.f) return 0;
    for(i=0;i<8;i++) if(!isfinite(state->overlay_channels[i]) || fabsf(state->overlay_channels[i])>1000000.f) return 0;
    for(i=0;i<3;i++) if(!isfinite(state->blend_channels[i]) || fabsf(state->blend_channels[i])>1000000.f) return 0;
    if(!pc_network_bot_shot_valid(&state->shot,humans+bots)) return 0;
    return isfinite(state->health) && state->health>=-1000.f && state->health<=1000.f &&
           isfinite(state->armor) && state->armor>=0.f && state->armor<=1000.f &&
           isfinite(state->max_health) && state->max_health>=0.f && state->max_health<=1000.f;
}
#endif
