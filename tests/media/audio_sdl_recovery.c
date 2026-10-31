#define main existing_gain_main
#include "audio_sdl_gain.c"
#undef main
static uint8_t pcm[64];
static int submit(unsigned slot) {
    return recomp_audio_output_submit(slot,pcm,sizeof pcm,48000,2,16,0);
}
int main(int argc, char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"startup") || !strcmp(argv[1],"subsystem")) {
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
