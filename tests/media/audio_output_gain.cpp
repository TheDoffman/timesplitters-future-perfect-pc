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
        for(int level=-9999;level<0;level++)
            assert(gainOf(level)==std::pow(10.f,level/2000.f)); /* existing Windows curve */
        recomp_audio_output_initialize();assert(opened==1);
        const uint8_t pcm[4]={};
        assert(recomp_audio_output_submit(0,pcm,sizeof pcm,48000,1,16,0));
        assert(recomp_audio_output_submit(RECOMP_AUDIO_SLOT_MOVIE,pcm,sizeof pcm,48000,1,16,0));
        for(unsigned slot: {0u,256u,(unsigned)RECOMP_AUDIO_SLOT_MOVIE}) {
            assert(recomp_audio_output_submit(slot,pcm,sizeof pcm,48000,1,16,-10000));
            auto *same=voices[slot].source;uint32_t held=same->queued;
            assert(same->gain==0.f);
            assert(recomp_audio_output_set_volume(slot,-600));
            assert(std::fabs(same->gain-0.5011872f)<0.00001f);
            assert(recomp_audio_output_set_volume(slot,-10000) && same->gain==0.f);
            assert(recomp_audio_output_set_volume(slot,-12000) && same->gain==0.f);
            assert(recomp_audio_output_set_volume(slot,1000) && same->gain==1.f);
            assert(voices[slot].source==same && same->queued==held);
        }
        auto *movie_voice=voices[RECOMP_AUDIO_SLOT_MOVIE].source;uint32_t queued=movie_voice->queued;
        recomp_audio_output_movie_speed(2.f);assert(movie_voice->speed==2.f && movie_voice->queued==queued && voices[RECOMP_AUDIO_SLOT_MOVIE].source==movie_voice);
        recomp_audio_output_movie_speed(.5f);assert(movie_voice->speed==.5f && movie_voice->queued==queued);
        recomp_audio_output_movie_speed(1.f);
        assert(recomp_audio_output_game_time(1000,0)==1000);
        recomp_audio_output_game_speed(2.f);assert(voices[0].source->speed==2.f);
        assert(recomp_audio_output_game_time(2000,0)==3000);
        recomp_audio_output_game_speed(.5f);assert(voices[0].source->speed==.5f);
        assert(recomp_audio_output_game_time(3000,0)==3500);
        assert(recomp_audio_output_game_time(2999,0)==3500); /* Delayed worker sample. */
        assert(recomp_audio_output_game_time(3002,0)==3501);
        assert(recomp_audio_output_game_time(4000,1)==4000); /* Independent tick domain. */
        assert(recomp_audio_output_game_time(5000,1)==4500);
        recomp_audio_output_game_speed(1.f);assert(voices[0].source->speed==1.f);

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
