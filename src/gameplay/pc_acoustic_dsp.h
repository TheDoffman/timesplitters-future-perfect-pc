/* Shared, bounded PCM filter. No allocation or guest memory on the audio path.
 * Targets are smoothed over 30 ms; existing byte counts and clocks are intact. */
#ifndef PC_ACOUSTIC_DSP_H
#define PC_ACOUSTIC_DSP_H
#include <math.h>
#include <stdint.h>
#include <string.h>
typedef struct PcAcousticDsp {
    float target,wet,amount,gain,alpha,bed,bed_amount,envelope,low[2],delay[2][4096];
    unsigned cursor,rate;
} PcAcousticDsp;
static inline void pc_acoustic_ambience_target(PcAcousticDsp *d,int enabled)
{d->bed=enabled?1.f:0.f;}
static inline void pc_acoustic_target(PcAcousticDsp *d,float amount,float wet)
{
    d->target=isfinite(amount)?fmaxf(0,fminf(1,amount)):0;
    d->wet=isfinite(wet)?fmaxf(0,fminf(.15f,wet)):0;
}
static inline void pc_acoustic_pcm(PcAcousticDsp *d,uint8_t *pcm,unsigned bytes,
                                  unsigned rate,unsigned channels,unsigned bits)
{
    if(!rate || (channels!=1 && channels!=2) || (bits!=8 && bits!=16))return;
    if(d->target==0 && d->amount==0 && d->wet==0 && d->bed==0 && d->bed_amount==0)return;
    if(d->rate!=rate){memset(d->low,0,sizeof d->low);memset(d->delay,0,sizeof d->delay);d->cursor=0;d->rate=rate;d->gain=1;d->alpha=1-expf(-6.2831853f*18000/rate);}
    unsigned frames=bytes/(channels*(bits/8));
    float smooth=1-expf(-1.f/(.03f*rate));
    float cutoff=18000.f*powf(1000.f/18000.f,d->target);
    if(d->bed)cutoff=fminf(cutoff,4000.f);
    float target_alpha=1-expf(-6.2831853f*cutoff/rate);
    float target_gain=powf(10.f,-9.f*d->target/20.f)*(d->bed?.5f:1.f);
    float release=expf(-1.f/(.12f*rate));
    unsigned tap=(unsigned)(rate*.037f);if(tap>4095)tap=4095;if(!tap)tap=1;
    for(unsigned f=0;f<frames;f++) {
        d->amount+=(d->target-d->amount)*smooth;
        d->bed_amount+=(d->bed-d->bed_amount)*smooth;
        d->alpha+=(target_alpha-d->alpha)*smooth;
        d->gain+=(target_gain-d->gain)*smooth;
        float samples[2]={0,0},peak=0;
        for(unsigned c=0;c<channels;c++) {
            unsigned offset=(f*channels+c)*(bits/8);float sample;
            if(bits==8)sample=((int)pcm[offset]-128)/128.f;
            else{int16_t v;memcpy(&v,pcm+offset,2);sample=v/32768.f;}
            samples[c]=sample;peak=fmaxf(peak,fabsf(sample));
        }
        /* Linked stereo peak control: soften baked explosions without moving
           their stereo image. The 120 ms release avoids pumping between peaks. */
        d->envelope=fmaxf(peak,d->envelope*release);
        float compression=d->envelope>.2f ? (.2f+(d->envelope-.2f)/3)/d->envelope : 1;
        float peak_gain=1+(compression-1)*d->bed_amount;
        for(unsigned c=0;c<channels;c++) {
            unsigned offset=(f*channels+c)*(bits/8);
            float sample=samples[c]*peak_gain;
            d->low[c]+=d->alpha*(sample-d->low[c]);
            float blend=fmaxf(d->amount,d->bed_amount);
            float dry=(sample+(d->low[c]-sample)*blend)*d->gain;
            float echo=d->delay[c][(d->cursor+4096-tap)&4095];
            d->delay[c][d->cursor]=dry+echo*.25f;
            float out=fmaxf(-1,fminf(1,dry*(1-d->wet)+echo*d->wet));
            if(bits==8)pcm[offset]=(uint8_t)(128+(int)(out*127));
            else{int16_t v=(int16_t)(out*32767);memcpy(pcm+offset,&v,2);}
        }
        d->cursor=(d->cursor+1)&4095;
    }
}
#endif
