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
    }
    puts("PASS: actual XAudio master initialization and voice attenuation");
}
