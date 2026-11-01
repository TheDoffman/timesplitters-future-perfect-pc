#ifndef TSFP_PC_CHARACTER_POSE_H
#define TSFP_PC_CHARACTER_POSE_H
#include <stdint.h>
#include <math.h>
#include <string.h>
/* Numeric animation state only. Models, asset handles and bone buffers remain
   local to each guest. This layout matches the pose slice of the bot wire. */
typedef struct PcCharacterPose {
    int32_t clip;
    float clip_channels[18];
    uint32_t clip_cycle,clip_flags;
    int32_t next_clip;
    float next_channels[18];
    uint32_t next_cycle,next_flags,overlay_phase;
    float overlay_channels[8],blend_channels[3];
    /* Two 12-bit authored clip IDs and five controller bits, not asset handles.
       Packing keeps Story's pose message plus cursor within256 bytes. */
    uint32_t limb_mask,limb_clips_flags;
    float limb_elapsed,aim_frame;
} PcCharacterPose;
static inline int pc_character_pose_valid(const PcCharacterPose *p)
{
    if(p->clip<0 || p->clip>4095 || p->next_clip<0 || p->next_clip>4095 || p->overlay_phase>2u) return 0;
    if((p->limb_clips_flags&0xe0000000u) ||
       !isfinite(p->limb_elapsed) || p->limb_elapsed<0.f || p->limb_elapsed>1000000.f ||
       !isfinite(p->aim_frame) || fabsf(p->aim_frame)>1000000.f) return 0;
    if(p->limb_mask && !(p->limb_clips_flags&(1u<<24)) &&
       (p->limb_clips_flags&(12u<<24)) && p->overlay_channels[7]<=0.f) return 0;
    for(unsigned i=0;i<18;i++) if(!isfinite(p->clip_channels[i]) || fabsf(p->clip_channels[i])>1000000.f ||
        !isfinite(p->next_channels[i]) || fabsf(p->next_channels[i])>1000000.f) return 0;
    for(unsigned i=0;i<8;i++) if(!isfinite(p->overlay_channels[i]) || fabsf(p->overlay_channels[i])>1000000.f) return 0;
    for(unsigned i=0;i<3;i++) if(!isfinite(p->blend_channels[i]) || fabsf(p->blend_channels[i])>1000000.f) return 0;
    return 1;
}
/* The caller validates a readable model span of at least0x200 bytes. */
static inline void pc_character_pose_read(PcCharacterPose *p,const void *model,const void *actor)
{
    const uint8_t *m=(const uint8_t *)model;
    memcpy(&p->clip,m+0xd4,4);memcpy(p->clip_channels,m+0xdc,72);
    memcpy(&p->clip_cycle,m+0x124,4);memcpy(&p->clip_flags,m+0x128,4);
    memcpy(&p->next_clip,m+0x12c,4);memcpy(p->next_channels,m+0x134,72);
    memcpy(&p->next_cycle,m+0x17c,4);memcpy(&p->next_flags,m+0x180,4);
    memcpy(&p->overlay_phase,m+0xa8,4);memcpy(p->overlay_channels,m+0xac,32);
    memcpy(p->blend_channels,m+0x198,12);
    uint32_t first,second,flags;
    memcpy(&p->limb_mask,m+0x9c,4);memcpy(&first,m+0xa0,4);memcpy(&second,m+0xa4,4);
    memcpy(&flags,m+0xd0,4);memcpy(&p->limb_elapsed,m+0xcc,4);
    p->limb_clips_flags=(first&4095u)|((second&4095u)<<12)|((flags&31u)<<24);
    if(first>4095u || second>4095u || flags>31u) p->limb_clips_flags|=0x80000000u;
    p->aim_frame=0.f;if(actor) memcpy(&p->aim_frame,(const uint8_t *)actor+0xb08,4);

}
#endif
