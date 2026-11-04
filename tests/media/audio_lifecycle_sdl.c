#include "audio_sdl_fake.h"
#include "../src/native/audio_sdl.c"
int audio_lifecycle_stress(int enhanced);
int audio_tail_completion(void (*stage)(int));
static void tail_stage(int stage) {
    SDL_AudioStream *s=voices[0].stream;assert(s);
    if(stage==0) {s->queued=3840;s->available=0;assert(!s->flushes);}
    else if(stage==1) {assert(s->flushes==1 && !s->queued && s->available==3840);}
    else {assert(s->flushes==1);s->available=0;}
}
void test_output_fault(void) { fail_put = 1; }
void test_output_recover(void) { fail_put = 0; sdl_now += 1000; }
int test_output_active(unsigned slot) { return voices[slot].stream != NULL; }
int test_output_clean(unsigned slot) {
    return !spatial[slot].enabled && !acoustics[slot].target && !acoustics[slot].amount && !discontinuity[slot];
}
int main(int argc, char **argv) {
    assert(argc == 2);
    if(!strcmp(argv[1],"tail")) {track_queue=1;return audio_tail_completion(tail_stage);}
    int result = audio_lifecycle_stress(atoi(argv[1]));
    assert(created == (int)destroyed);
    return result;
}
