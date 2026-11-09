#include "audio_sdl_fake.h"
#include "../src/native/audio_sdl.c"
int main(int argc,char **argv)
{
    const float front[]={0,0,1},up[]={0,1,0};
    const float right[]={10,0,0},left[]={-10,0,0},far_right[]={1000,0,0};
    const float behind[]={0,0,-10},above[]={0,10,0},coincident[]={0,0,0};
    const float turned[]={1,0,0},bad[]={NAN,0,0};
    assert(recomp_spatial_pan(right,front,up,0)==1);
    assert(recomp_spatial_pan(left,front,up,0)==-1);
    assert(recomp_spatial_pan(far_right,front,up,0)==1);
    assert(recomp_spatial_pan(behind,front,up,0)==0);
    assert(recomp_spatial_pan(above,front,up,0)==0);
    assert(recomp_spatial_pan(coincident,front,up,0)==0);
    assert(recomp_spatial_pan(right,turned,up,0)==0); /* camera turns toward source */
    assert(recomp_spatial_pan(right,turned,up,1)==1); /* head-relative ignores world basis */
    assert(recomp_spatial_pan(right,front,up,2)==0); /* disabled 3D */
    assert(recomp_spatial_pan(bad,front,up,0)==0);
    const float muzzle[]={0.1f,0,0},nearby[]={1.5f,0,0},world[]={2,0,0};
    assert(recomp_spatial_pan(muzzle,front,up,0)==0);
    assert(fabsf(recomp_spatial_pan(nearby,front,up,0)-0.5f)<0.00001f);
    assert(recomp_spatial_pan(world,front,up,0)==1);
    assert(recomp_weapon_sound_range(50)==150);
    assert(recomp_weapon_sound_range(70)==210);
    assert(recomp_weapon_sound_range(200)==600);
    assert(recomp_weapon_sound_range(-1)==-1);
    assert(isnan(recomp_weapon_sound_range(NAN)));
    uint8_t pcm[64]={0};
    assert(argc==2);
    float expected=strtof(argv[1],NULL);
    recomp_audio_output_initialize();assert(opened==(expected>0));
    if(expected>0) {
        /* Sound buffer, music stream and movie slot use the same output level. */
        const unsigned slots[]={0,256,RECOMP_AUDIO_SLOT_MOVIE};
        for(unsigned i=0;i<3;i++) {
            assert(recomp_audio_output_submit(slots[i],pcm,sizeof pcm,48000,2,16,0));
            assert(fabsf(streams[i].gain-expected)<0.00001f);
            assert(recomp_audio_output_set_volume(slots[i],-600));
            assert(fabsf(streams[i].gain-expected*0.5011872f)<0.00001f);
            assert(recomp_audio_output_set_volume(slots[i],-2000));
            assert(fabsf(streams[i].gain-expected*0.1f)<0.00001f);
            assert(recomp_audio_output_set_volume(slots[i],0));
            assert(fabsf(streams[i].gain-expected)<0.00001f); /* no cumulative reduction */
            assert(recomp_audio_output_set_volume(slots[i],-10000));
            assert(voices[slots[i]].stream->gain==0.f); /* mute is exact, not -100dB leakage */
            assert(recomp_audio_output_set_volume(slots[i],-12000));
            assert(voices[slots[i]].stream->gain==0.f);
            assert(recomp_audio_output_set_volume(slots[i],1000));
            assert(voices[slots[i]].stream->gain==expected);
        }
        track_queue=1;
        assert(recomp_audio_output_submit(42,pcm,sizeof pcm,48000,2,16,-10000));
        SDL_AudioStream *muted=voices[42].stream;int queued_mute=muted->queued;
        assert(muted->gain==0.f && queued_mute==sizeof pcm);
        assert(recomp_audio_output_set_volume(42,0) && muted->gain==expected);
        assert(voices[42].stream==muted && muted->queued==queued_mute);
        track_queue=0;
        SDL_AudioStream *movie=voices[RECOMP_AUDIO_SLOT_MOVIE].stream;int queued=movie->queued;
        recomp_audio_output_movie_speed(2.f);assert(movie->speed==2.f && movie->queued==queued && voices[RECOMP_AUDIO_SLOT_MOVIE].stream==movie);
        recomp_audio_output_movie_speed(.5f);assert(movie->speed==.5f && movie->queued==queued);
        recomp_audio_output_movie_speed(1.f);
        assert(recomp_audio_output_game_time(1000,0)==1000);
        recomp_audio_output_game_speed(2.f);assert(voices[0].stream->speed==2.f);
        assert(recomp_audio_output_game_time(2000,0)==3000);
        recomp_audio_output_game_speed(.5f);assert(voices[0].stream->speed==.5f);
        assert(recomp_audio_output_game_time(3000,0)==3500);
        assert(recomp_audio_output_game_time(2999,0)==3500); /* Delayed worker sample. */
        assert(recomp_audio_output_game_time(3002,0)==3501);
        assert(recomp_audio_output_game_time(4000,1)==4000); /* Independent tick domain. */
        assert(recomp_audio_output_game_time(5000,1)==4500);
        recomp_audio_output_game_speed(1.f);assert(voices[0].stream->speed==1.f);

        assert(opened==1);
        const int16_t tone[]={12000,-12000};int16_t heard[4];
        recomp_audio_output_set_spatial(3,-1,1);
        assert(recomp_audio_output_submit(3,(const uint8_t *)tone,sizeof tone,48000,1,16,0));
        assert(heard_bytes==8);memcpy(heard,heard_pcm,8);
        assert(heard[0]==12000 && heard[1]==0 && heard[2]==-12000 && heard[3]==0);
        recomp_audio_output_set_spatial(3,1,1);
        assert(recomp_audio_output_submit(3,(const uint8_t *)tone,sizeof tone,48000,1,16,0));
        memcpy(heard,heard_pcm,8);assert(heard[0]==0 && heard[1]==12000);
        recomp_audio_output_reset_voice(3);
        assert(recomp_audio_output_submit(3,(const uint8_t *)tone,sizeof tone,48000,1,16,0));
        memcpy(heard,heard_pcm,8);assert(heard[0]==12000 && heard[1]==12000);
        recomp_audio_output_set_acoustics(3,1,0);
        refuse=1;unsigned cursor=acoustics[3].cursor;
        assert(!recomp_audio_output_submit(3,(const uint8_t *)tone,sizeof tone,48000,1,16,0));
        assert(acoustics[3].cursor==cursor && acoustics[3].amount==0);
        refuse=0;sdl_now+=RECOMP_AUDIO_RETRY_MS;
        for(unsigned i=0;i<2000;i++)assert(recomp_audio_output_submit(3,(const uint8_t *)tone,sizeof tone,48000,1,16,0));
        memcpy(heard,heard_pcm,8);assert(abs(heard[0])<6000 && abs(heard[1])<6000);
        assert(heard_bytes==8); /* filtering leaves the output clock unchanged */
        recomp_audio_output_reset_voice(3);assert(acoustics[3].amount==0 && acoustics[3].target==0);
        recomp_audio_output_set_ambience(3,1);
        for(unsigned i=0;i<2000;i++)assert(recomp_audio_output_submit(3,(const uint8_t *)tone,sizeof tone,48000,1,16,0));
        memcpy(heard,heard_pcm,8);assert(abs(heard[0])<6000 && acoustics[3].bed_amount>.9f);
        recomp_audio_output_reset_voice(3);assert(acoustics[3].bed==0 && acoustics[3].bed_amount==0);
    }
    recomp_audio_output_shutdown();
    puts("PASS: actual SDL buffer/music/movie output gain and live volume changes");
    return 0;
}
