#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "gameplay/pc_audio_dynamics.h"
#include "gameplay/pc_audio_activity.h"
int main(void)
{
    /* Non-silent spans follow played PCM, including stereo/U8 silence and
     * a fractional-rate fallback; saturation remains bounded and expires. */
    uint8_t speech[]={128,128,255,128,128,128},saved[6];memcpy(saved,speech,6);
    PcAudioActivity activity={0};
    pc_audio_activity_submit(&activity,speech,6,8000,2,8,0,1000);
    assert(!memcmp(saved,speech,6));
    assert(!pc_audio_activity_pending(&activity,1000,0,1,1));
    assert(pc_audio_activity_pending(&activity,1000,2,1,1));
    assert(!pc_audio_activity_pending(&activity,1000,4,1,1));
    PcAudioActivity one={0},many={0};one.bps=many.bps=44084;
    one.submitted=many.submitted=44084;
    pc_audio_activity_step(&one,999);
    for(unsigned ms=1;ms<=999;ms++)pc_audio_activity_step(&many,ms);
    assert(one.fallback==many.fallback && one.remainder==many.remainder);
    pc_audio_activity_step(&many,1000);assert(many.fallback==many.submitted);
    memset(&activity,0,sizeof activity);activity.bps=1000;activity.submitted=10000;
    for(unsigned i=0;i<100;i++)pc_audio_activity_span(&activity,i*100,i*100+10);
    assert(activity.count<=PC_AUDIO_ACTIVITY_SPANS);
    assert(pc_audio_activity_pending(&activity,0,9900,1,1));
    assert(!pc_audio_activity_pending(&activity,0,9910,1,1) && !activity.count);
    int16_t original[4096],large[4096],chunks[4096];
    for(unsigned i=0;i<4096;i+=2){original[i]=i%14?30000:-32768;original[i+1]=original[i]/2;}
    for(unsigned mode=1;mode<=2;mode++) {
        memcpy(large,original,sizeof large);memcpy(chunks,original,sizeof chunks);
        PcAudioDynamics a={0},b={0};
        pc_audio_dynamics_pcm(&a,(uint8_t *)large,sizeof large,48000,2,16,0,mode);
        unsigned frame=0;
        while(frame<2048) {
            unsigned n=1+(frame%137);if(n>2048-frame)n=2048-frame;
            pc_audio_dynamics_pcm(&b,(uint8_t *)(chunks+2*frame),n*4,48000,2,16,0,mode);frame+=n;
        }
        assert(!memcmp(large,chunks,sizeof large) && !memcmp(&a,&b,sizeof a));
        for(unsigned i=0;i<4096;i+=2) {
            assert(abs(large[i])<abs(original[i]) && abs(large[i+1])<=abs(original[i+1]));
            assert(abs(large[i]-2*large[i+1])<=1); /* linked stereo */
        }
        pc_audio_dynamics_pcm(&a,(uint8_t *)large,sizeof large,48000,2,16,0,0);
        assert(!a.rate);memcpy(chunks,original,sizeof chunks);
        pc_audio_dynamics_pcm(&a,(uint8_t *)chunks,sizeof chunks,48000,2,16,0,0);
        assert(!memcmp(chunks,original,sizeof chunks));
        pc_audio_dynamics_pcm(&a,(uint8_t *)chunks,sizeof chunks,48000,2,16,-6000,mode);
        assert(!memcmp(chunks,original,sizeof chunks)); /* already attenuated input */
    }
    uint8_t bytes[]={0,64,128,192,255,128,128,128};PcAudioDynamics state={0};
    pc_audio_dynamics_pcm(&state,bytes,sizeof bytes,8000,1,8,0,2);
    assert(bytes[0]>0 && bytes[4]<255 && bytes[2]==128);
    uint8_t quiet[]={128,128,128};assert(!pc_audio_speech_pcm(quiet,sizeof quiet,8,0));
    assert(pc_audio_speech_pcm(bytes,sizeof bytes,8,0) && !pc_audio_speech_pcm(bytes,sizeof bytes,8,-10000));
    int16_t silence[2]={0,0};assert(!pc_audio_speech_pcm((uint8_t *)silence,sizeof silence,16,0));
    uint8_t invalid[]={1,2,3};uint8_t before[3];memcpy(before,invalid,3);
    pc_audio_dynamics_pcm(&state,invalid,3,48000,2,16,0,2);assert(!memcmp(before,invalid,3));
    puts("PASS: linked impact reduction, partition invariance, exact neutral, existing gain and speech detection");
}
