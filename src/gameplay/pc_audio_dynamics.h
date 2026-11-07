#ifndef PC_AUDIO_DYNAMICS_H
#define PC_AUDIO_DYNAMICS_H
#include <stdint.h>
#include <string.h>
#include <math.h>
/* Per-source impact control, before the common host filter/output path. It
 * only attenuates peaks; no makeup, resampling, lookahead or completion delay.
 * Stereo detection is linked, so channel balance is retained. */
typedef struct PcAudioDynamics {float envelope;unsigned rate,mode;} PcAudioDynamics;
static inline float pc_audio_pcm_sample(const uint8_t *p,unsigned bits)
{
    if(bits==8)return ((int)*p-128)/128.f;
    int16_t sample;memcpy(&sample,p,2);return sample/32768.f;
}
static inline int pc_audio_speech_pcm(const uint8_t *pcm,unsigned bytes,unsigned bits,int32_t level)
{
    if(!pcm || (bits!=8 && bits!=16) || level<=-10000)return 0;
    float scale=level<0?powf(10.f,level/2000.f):1.f;
    for(unsigned i=0;i+bits/8<=bytes;i+=bits/8)
        if(fabsf(pc_audio_pcm_sample(pcm+i,bits))*scale>.001f)return 1;
    return 0;
}
static inline void pc_audio_dynamics_pcm(PcAudioDynamics *d,uint8_t *pcm,unsigned bytes,
    unsigned rate,unsigned channels,unsigned bits,int32_t level,unsigned mode)
{
    if(!mode || mode>2 || level<=-10000) {memset(d,0,sizeof *d);return;}
    if(!pcm || !rate || (channels!=1 && channels!=2) || (bits!=8 && bits!=16))return;
    if(d->rate!=rate || d->mode!=mode){memset(d,0,sizeof *d);d->rate=rate;d->mode=mode;}
    float scale=level<0?powf(10.f,level/2000.f):1.f;
    float threshold=mode==1?.35f:.125f,ratio=mode==1?2.f:4.f;
    float release=expf(-1.f/((mode==1?.12f:.25f)*rate));
    unsigned stride=bits/8,frames=bytes/(channels*stride);
    for(unsigned f=0;f<frames;f++) {
        float peak=0;
        for(unsigned c=0;c<channels;c++)
            peak=fmaxf(peak,fabsf(pc_audio_pcm_sample(pcm+(f*channels+c)*stride,bits))*scale);
        d->envelope=fmaxf(peak,d->envelope*release);
        if(d->envelope<=threshold)continue; /* exact bytes below threshold */
        float gain=(threshold+(d->envelope-threshold)/ratio)/d->envelope;
        for(unsigned c=0;c<channels;c++) {
            uint8_t *sample=pcm+(f*channels+c)*stride;
            if(bits==8)*sample=(uint8_t)(128+(int)lroundf(((int)*sample-128)*gain));
            else {int16_t input;memcpy(&input,sample,2);int16_t output=(int16_t)lroundf(input*gain);memcpy(sample,&output,2);}
        }
    }
}
#endif
