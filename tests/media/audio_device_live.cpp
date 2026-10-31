/* Opt-in Windows check: real device, silent PCM, injected critical callback. */
#include "hle/audio_output_xaudio2.cpp"
#include <cassert>
int main() {
    uint8_t silence[3840] = {};
    auto submit = [&] { return recomp_audio_output_submit(0,silence,sizeof silence,48000,2,16,0); };
    if (!submit()) { std::fprintf(stderr,"No default audio device available\n"); return 77; }
    uint64_t played=0; uint32_t queued=0;
    assert(recomp_audio_output_position(0,&played,&queued));
    callback.OnCriticalError(XAUDIO2_E_DEVICE_INVALIDATED);
    assert(!recomp_audio_output_position(0,&played,&queued));
    assert(recomp_audio_output_take_discontinuity(0));
    assert(!submit());
    Sleep(RECOMP_AUDIO_RETRY_MS+20);
    assert(submit());
    Sleep(100);
    assert(recomp_audio_output_position(0,&played,&queued) && played==sizeof silence && queued==0);
    recomp_audio_output_shutdown();
    assert(!submit());
    std::puts("PASS: real XAudio device reopened after injected critical error; silent PCM consumed");
}
