#include "audio_xaudio_fake.h"
#include "hle/audio_output_xaudio2.cpp"
extern "C" int audio_lifecycle_stress(int enhanced);
extern "C" void test_output_fault(void) { registered->OnCriticalError(XAUDIO2_E_DEVICE_INVALIDATED); }
extern "C" void test_output_recover(void) { fake_ms += 1000; }
extern "C" int test_output_active(unsigned slot) { return voices[slot].source != nullptr; }
extern "C" int test_output_clean(unsigned slot) {
    return !spatial[slot].enabled && !acoustics[slot].target && !acoustics[slot].amount && !discontinuity[slot];
}
int main(int argc, char **argv) {
    assert(argc == 2);
    int result = audio_lifecycle_stress(atoi(argv[1]));
    assert(source_created == source_destroyed);
    return result;
}
