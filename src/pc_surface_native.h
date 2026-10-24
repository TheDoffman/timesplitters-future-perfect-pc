#ifndef PC_SURFACE_NATIVE_H
#define PC_SURFACE_NATIVE_H
#include "pc_surface_capture.h"
#include "graphics/surface_interval.h"

/* Common renderers use native water with the high-refresh beta presets.
 * Explicit 0 is the diagnostic recovery path; older experiments stay separate. */
static inline int pc_surface_native_policy(int supported,const char *override,
    const char *fractional,const char *floating,const char *live) {
    if(!supported || (override && override[0] && strcmp(override,"1")))return 0;
    return !(fractional && fractional[0]=='1') && !(floating && floating[0]=='1') && !(live && live[0]=='1');
}
/* Cosmetic field follows the last authored event boundary. Original feedback
 * remains authoritative: events/RNG never move to an earlier native frame. */
typedef struct PcSurfaceNativeSample {
    PcSurfaceCapture feedback;
    uint32_t final_texture[4];
    int64_t frequency,start_phase,units;
} PcSurfaceNativeSample;
typedef struct PcSurfaceNativeClock {
    int64_t frequency,phase;
    uint32_t frame,ticks,scene,level,ready;
} PcSurfaceNativeClock;
/* 0=fallback, 1=begin from authored input, 2=continue, 3=duplicate. Commit the
 * returned clock only after the renderer successfully computes the field. */
static inline unsigned pc_surface_native_plan(const PcSurfaceNativeClock *old,
    const PcSurfaceNativeSample *sample,PcSurfaceNativeClock *next,double *delta) {
    if(!old || !sample || !next || !delta)return 0;
    PcSurfaceInterval interval;
    const PcSurfaceCapture *s=&sample->feedback;
    if(!pc_surface_interval_split(sample->frequency,sample->start_phase,sample->units,&interval) ||
       !sample->units || (sample->start_phase+sample->units)/sample->frequency!=s->whole)return 0;
    if(old->ready && old->frame==s->frame && old->ticks==s->level_ticks &&
       old->scene==s->scene && old->level==s->level && old->frequency==sample->frequency &&
       old->phase==interval.end_phase){*next=*old;*delta=0;return 3;}
    if(!s->whole && (!old->ready || old->frequency!=sample->frequency ||
       old->phase!=sample->start_phase || old->ticks!=s->level_ticks ||
       old->scene!=s->scene || old->level!=s->level))return 0;
    *next=(PcSurfaceNativeClock){sample->frequency,interval.end_phase,s->frame,s->level_ticks,s->scene,s->level,1};
    *delta=(double)(s->whole?interval.end_phase:sample->units)/(double)sample->frequency;
    return s->whole?1:2;
}
#endif
