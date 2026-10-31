#define main existing_gain_main
#include "audio_sdl_gain.c"
#undef main
static uint8_t pcm[64];
static int submit(unsigned slot) {
    return recomp_audio_output_submit(slot,pcm,sizeof pcm,48000,2,16,0);
}
int main(int argc, char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"diagnostics")) {
        RecompAudioEngineStats stats;
        fail_open=1; assert(!submit(0));
        assert(!recomp_audio_output_engine_stats(&stats));
        assert(stats.open_attempts==1 && stats.device_failed==1 && stats.unavailable==1 && !stats.opens);
        fail_open=0; sdl_now=1000; assert(submit(0));
        queued_bytes=96001; assert(!submit(0)); queued_bytes=0;
        assert(!recomp_audio_output_submit(0,pcm,3,48000,2,16,0));
        fail_put=1; assert(!submit(0)); fail_put=0; assert(!submit(0));
        recomp_audio_output_engine_stats(&stats);
        assert(stats.queue_full==1 && stats.invalid_pcm==1 && stats.voice_failed==1 && stats.retry_wait==1);
        sdl_now=2000; assert(submit(0));
        recomp_audio_output_reset_voice(0); recomp_audio_output_shutdown();
        assert(!recomp_audio_output_engine_stats(&stats));
        assert(stats.open_attempts==2 && stats.opens==1 && stats.device_failed==1);
        assert(stats.queue_full==1 && stats.invalid_pcm==1 && stats.voice_failed==1);
        return 0;
    }
    if(!strcmp(argv[1],"enhanced-on") || !strcmp(argv[1],"enhanced-off")) {
        const int enabled=!strcmp(argv[1],"enhanced-on");
        recomp_audio_output_set_spatial(0,-1,enabled);
        recomp_audio_output_set_acoustics(0,(float)enabled,0);
        recomp_audio_output_set_ambience(0,enabled);
        pcm[0]=0x51;assert(submit(0));
        if(!enabled)assert(!memcmp(heard_pcm,pcm,sizeof pcm));
        const PcAcousticDsp before=acoustics[0];
        fail_put=1;assert(!submit(0));
        assert(!memcmp(&before,&acoustics[0],sizeof before));
        fail_put=0;sdl_now=1000;assert(submit(0));
        assert(spatial[0].enabled==enabled && acoustics[0].bed==enabled);
        assert(enabled?acoustics[0].amount>0:!memcmp(heard_pcm,pcm,sizeof pcm));
        recomp_audio_output_reset_voice(0);
        assert(!spatial[0].enabled && !acoustics[0].bed);
    } else if(!strcmp(argv[1],"startup") || !strcmp(argv[1],"subsystem")) {
        fail_open=!strcmp(argv[1],"startup"); fail_init=!fail_open;
        assert(!submit(0)); assert(recomp_audio_output_take_discontinuity(0));
        unsigned tries=inits, opens=opened;
        fail_open=fail_init=0;
        sdl_now=999; assert(!submit(0) && inits==tries && opened==opens);
        sdl_now=1000; assert(submit(0));
        assert(inits==(tries+(!strcmp(argv[1],"subsystem"))));
    } else if(!strcmp(argv[1],"volume")) {
        assert(submit(0) && submit(1)); fail_gain=1;
        assert(!recomp_audio_output_set_volume(0,-600));
        assert(!voices[0].stream && voices[1].stream);
        assert(recomp_audio_output_take_discontinuity(0));
        fail_gain=0; sdl_now=1000; assert(submit(0));
    } else if(!strcmp(argv[1],"shutdown")) {
        recomp_audio_output_shutdown(); sdl_now=10000; assert(!submit(0) && !opened);
    } else if(!strcmp(argv[1],"queue")) {
        assert(submit(0)); queued_bytes=96001;
        assert(!submit(0)); assert(!recomp_audio_output_take_discontinuity(0));
        queued_bytes=0; assert(submit(0) && !destroyed);
    } else {
        assert(submit(1));
        fail_bind=!strcmp(argv[1],"bind"); fail_put=!fail_bind;
        assert(!submit(0)); assert(voices[1].stream && !voices[0].stream);
        assert(recomp_audio_output_take_discontinuity(0));
        fail_bind=fail_put=0; sdl_now=999; assert(!submit(0));
        sdl_now=1000; assert(submit(0));
        recomp_audio_output_reset_voice(0); assert(!recomp_audio_output_take_discontinuity(0));
        assert(opened==1 && inits==1);
    }
    recomp_audio_output_shutdown(); assert(created==(int)destroyed);
    sdl_now=5000; assert(!submit(0));
    puts("PASS: SDL retry/recovery and terminal shutdown");
    return 0;
}
