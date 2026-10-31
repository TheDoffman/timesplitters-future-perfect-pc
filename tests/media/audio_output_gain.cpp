/* Actual XAudio initialization and dB conversion; mock only the host device. */
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "hle/audio_output.h"
#include "audio_xaudio_fake.h"
#include "hle/audio_output_xaudio2.cpp"
int main(int argc,char **argv)
{
    assert(argc==2);
    float expected=std::strtof(argv[1],nullptr);
    recomp_audio_output_initialize();
    assert(opened==(expected>0));
    if(expected>0) {
        assert(std::fabs(heard_gain-expected)<0.00001f);
        assert(gainOf(0)==1.f);
        assert(std::fabs(gainOf(-600)-0.5011872f)<0.00001f);
        assert(std::fabs(gainOf(-2000)-0.1f)<0.00001f);
        assert(gainOf(-10000)==0.f);
        recomp_audio_output_initialize();assert(opened==1);
        const uint8_t pcm[4]={};
        assert(recomp_audio_output_submit(0,pcm,sizeof pcm,48000,1,16,0));
        auto &source=*voices[0].source;
        recomp_audio_output_set_spatial(0,-1,1);
        assert(source.matrix[0]==1 && source.matrix[1]==0);
        recomp_audio_output_set_spatial(0,1,1);
        assert(source.matrix[0]==0 && source.matrix[1]==1);
        recomp_audio_output_set_spatial(0,0,1);
        assert(std::fabs(source.matrix[0]-0.70710678f)<0.00001f);
        recomp_audio_output_set_spatial(0,0,0);
        assert(source.matrix[0]==1 && source.matrix[1]==1);
        recomp_audio_output_set_spatial(99,1,1); /* reject invalid slots */
    }
    puts("PASS: actual XAudio master initialization and voice attenuation");
}
