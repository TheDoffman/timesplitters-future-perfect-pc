#include "audio_sdl_fake.h"
#include "../src/native/audio_sdl.c"
int audio_lifecycle_stress(int enhanced);
void test_output_fault(void) { fail_put = 1; }
void test_output_recover(void) { fail_put = 0; sdl_now += 1000; }
int test_output_active(unsigned slot) { return voices[slot].stream != NULL; }
int test_output_clean(unsigned slot) {
    return !spatial[slot].enabled && !acoustics[slot].target && !acoustics[slot].amount && !discontinuity[slot];
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int result = audio_lifecycle_stress(atoi(argv[1]));
    assert(created == (int)destroyed);
    return result;
}
