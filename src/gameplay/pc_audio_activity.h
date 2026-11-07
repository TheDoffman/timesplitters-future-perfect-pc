#ifndef PC_AUDIO_ACTIVITY_H
#define PC_AUDIO_ACTIVITY_H
#include "gameplay/pc_audio_dynamics.h"
/* Optional ducking metadata only: accepted PCM byte spans, never a guest clock.
 * Merge sub-10ms holes; on saturation merge the oldest spans conservatively.
 * The fixed queue bounds work/memory without allocating on an audio path. */
#define PC_AUDIO_ACTIVITY_SPANS 64
typedef struct PcAudioActivity {
    uint64_t submitted, fallback, last_ms, remainder;
    uint32_t bps;
    unsigned count;
    struct {uint64_t begin,end;} spans[PC_AUDIO_ACTIVITY_SPANS];
} PcAudioActivity;
static inline void pc_audio_activity_step(PcAudioActivity *a,uint64_t now)
{
    if(a->bps && now>a->last_ms) {
        uint64_t left=a->submitted-a->fallback,delta=now-a->last_ms;
        if(delta>=(left*1000+a->bps-1)/a->bps){a->fallback=a->submitted;a->remainder=0;}
        else {uint64_t total=delta*a->bps+a->remainder;a->fallback+=total/1000;a->remainder=total%1000;}
    }
    a->last_ms=now;
}
static inline void pc_audio_activity_span(PcAudioActivity *a,uint64_t begin,uint64_t end)
{
    if(a->count && begin<=a->spans[a->count-1].end+a->bps/100) {
        a->spans[a->count-1].end=end;return;
    }
    if(a->count==PC_AUDIO_ACTIVITY_SPANS) {
        a->spans[1].begin=a->spans[0].begin;
        memmove(a->spans,a->spans+1,(--a->count)*sizeof a->spans[0]);
    }
    a->spans[a->count].begin=begin;a->spans[a->count++].end=end;
}
static inline void pc_audio_activity_submit(PcAudioActivity *a,const uint8_t *pcm,
    unsigned bytes,unsigned rate,unsigned channels,unsigned bits,int32_t level,uint64_t now)
{
    unsigned frame=channels*(bits/8),bps=rate*frame;
    if(!frame || !rate || (bits!=8 && bits!=16) || (channels!=1 && channels!=2))return;
    if(a->bps!=bps){memset(a,0,sizeof *a);a->bps=bps;a->last_ms=now;}
    pc_audio_activity_step(a,now);
    uint64_t begin=0;int inside=0;float gain=pc_audio_db_gain(level);
    for(unsigned i=0;i+frame<=bytes;i+=frame) {
        int active=0;
        for(unsigned c=0;c<channels;c++)
            if(fabsf(pc_audio_pcm_sample(pcm+i+c*(bits/8),bits))*gain>.001f)active=1;
        if(active && !inside){begin=a->submitted+i;inside=1;}
        if(!active && inside){pc_audio_activity_span(a,begin,a->submitted+i);inside=0;}
    }
    if(inside)pc_audio_activity_span(a,begin,a->submitted+bytes);
    a->submitted+=bytes;
}
static inline int pc_audio_activity_pending(PcAudioActivity *a,uint64_t now,
    uint64_t played,int position_known,int pending)
{
    pc_audio_activity_step(a,now);
    if(!position_known)played=a->fallback;
    unsigned expired=0;
    while(expired<a->count && a->spans[expired].end<=played)expired++;
    if(expired){a->count-=expired;memmove(a->spans,a->spans+expired,a->count*sizeof a->spans[0]);}
    return pending && a->count && a->spans[0].begin<=played && played<a->spans[0].end;
}
#endif
