#include "audio_xaudio_fake.h"
#include "hle/audio_output_xaudio2.cpp"
#include <thread>
#include <initializer_list>
static uint8_t pcm[64];
static int submit(unsigned slot=0) {
    return recomp_audio_output_submit(slot, pcm, sizeof pcm, 48000, 2, 16, 0);
}
int main(int argc, char **argv) {
    assert(argc==2); const char *mode=argv[1]; pcm[0]=0x51;
    if (!strcmp(mode,"exit-failure")) {
        fail_operation="engine"; assert(!submit()); return 0;
    }
    if (!strcmp(mode,"exit-muted")) { assert(!submit()); return 0; }
    if (!strcmp(mode,"diagnostics")) {
        RecompAudioEngineStats stats{};
        fail_operation="engine"; assert(!submit());
        assert(!recomp_audio_output_engine_stats(&stats));
        assert(stats.open_attempts==1 && stats.device_failed==1 && stats.unavailable==1 && !stats.opens);
        fail_operation=nullptr; fake_ms=1000; assert(submit());
        for(unsigned i=1;i<12;i++) assert(submit());
        assert(!submit());
        assert(!recomp_audio_output_submit(0,pcm,3,48000,2,16,0));
        voices[0].source->queued=0;
        fail_operation="submit"; assert(!submit());
        fail_operation=nullptr; assert(!submit());
        assert(recomp_audio_output_engine_stats(&stats));
        assert(stats.queue_full==1 && stats.invalid_pcm==1 && stats.voice_failed==1 && stats.retry_wait==1);
        fake_ms=2000; assert(submit());
        registered->OnCriticalError(XAUDIO2_E_DEVICE_INVALIDATED);
        assert(!submit()); fake_ms=3000; assert(submit());
        assert(recomp_audio_output_engine_stats(&stats));
        assert(stats.open_attempts==3 && stats.opens==2 && stats.device_failed==2);
        assert(stats.queue_full==1 && stats.invalid_pcm==1 && stats.unavailable==2);
        recomp_audio_output_reset_voice(0);
        recomp_audio_output_shutdown();
        recomp_audio_output_shutdown();
        assert(!recomp_audio_output_engine_stats(&stats));
        assert(stats.queue_full==1 && stats.voice_failed==1 && stats.opens==2);
        return 0;
    }
    if (!strcmp(mode,"muted") || !strcmp(mode,"invalid-gain")) {
        for(fake_ms=0;fake_ms<5000;fake_ms+=1000) assert(!submit());
        assert(opened==0); return 0;
    }
    if (!strcmp(mode,"enhanced-on") || !strcmp(mode,"enhanced-off")) {
        const int enabled=!strcmp(mode,"enhanced-on");
        recomp_audio_output_set_spatial(0,-1,enabled);
        recomp_audio_output_set_acoustics(0,(float)enabled,0);
        recomp_audio_output_set_ambience(0,enabled);
        assert(submit());
        assert(voices[0].source->matrix[1]==0);
        assert(voices[0].source->matrix[3]==(enabled?0:1));
        if(!enabled) assert(!memcmp(voices[0].source->last_pcm,pcm,sizeof pcm));
        const PcAcousticDsp before=acoustics[0];
        fail_operation="submit"; assert(!submit());
        assert(!memcmp(&before,&acoustics[0],sizeof before));
        fail_operation=nullptr;fake_ms=1000;assert(submit());
        assert(spatial[0].enabled==enabled && acoustics[0].bed==enabled);
        assert(voices[0].source->matrix[3]==(enabled?0:1));
        registered->OnCriticalError(XAUDIO2_E_DEVICE_INVALIDATED);
        assert(!submit());fake_ms=2000;assert(submit());
        assert(spatial[0].enabled==enabled && acoustics[0].bed==enabled);
        assert(voices[0].source->matrix[3]==(enabled?0:1));
        if(!enabled)assert(!memcmp(voices[0].source->last_pcm,pcm,sizeof pcm));
        recomp_audio_output_reset_voice(0);
        assert(!spatial[0].enabled && !acoustics[0].bed);
    } else if (!strcmp(mode,"com-retry")) {
        fail_operation="com"; assert(!submit()); assert(com_started==1 && !opened);
        fail_operation=nullptr; fake_ms=1000; assert(submit()); assert(com_started==2);
    } else if (!strcmp(mode,"device-submit")) {
        assert(submit(0) && submit(1));
        fail_operation="submit"; fail_code=XAUDIO2_E_DEVICE_INVALIDATED;
        assert(!submit(0) && !engine && released==1);
        assert(recomp_audio_output_take_discontinuity(1));
        fail_operation=nullptr; fake_ms=1000; assert(submit());
    } else if (!strcmp(mode,"live-volume")) {
        assert(submit(0) && submit(1)); fail_operation="volume";
        assert(!recomp_audio_output_set_volume(0,-600));
        assert(engine && voices[1].source && !voices[0].source);
        assert(recomp_audio_output_take_discontinuity(0));
        fail_operation=nullptr; fake_ms=1000; assert(submit());
    } else if (!strcmp(mode,"startup") || !strcmp(mode,"register") || !strcmp(mode,"master-volume")) {
        fail_operation=!strcmp(mode,"startup")?"master":mode; assert(!submit()); assert(opened==1 && released==1);
        fail_operation=nullptr;
        for(fake_ms=1;fake_ms<1000;fake_ms++) assert(!submit());
        assert(opened==1); assert(submit()); assert(opened==2);
        assert(recomp_audio_output_take_discontinuity(0));
    } else if (!strcmp(mode,"shutdown")) {
        recomp_audio_output_shutdown(); fake_ms=5000; assert(!submit()); assert(opened==0);
    } else if (!strcmp(mode,"critical")) {
        assert(submit(0) && submit(256) && submit(272));
        registered->OnCriticalError(XAUDIO2_E_DEVICE_INVALIDATED);
        uint64_t played=99; uint32_t queued=99;
        assert(!recomp_audio_output_position(0,&played,&queued));
        assert(released==1 && source_destroyed==3 && master_destroyed==1);
        for(unsigned slot : {0u,256u,272u}) {
            assert(recomp_audio_output_take_discontinuity(slot));
            assert(!recomp_audio_output_take_discontinuity(slot));
        }
        fail_operation="engine"; fake_ms=1000; assert(!submit());
        assert(opened==2); fail_operation=nullptr;
        fake_ms=1999; assert(!submit()); fake_ms=2000; assert(submit());
        assert(opened==3 && voices[0].source->last_pcm[0]==0x51);
        assert(!critical_error.load());
        registered->OnCriticalError(E_FAIL); assert(!submit());
        fake_ms=3000; assert(submit()); assert(opened==4);
    } else if (!strcmp(mode,"queue")) {
        for(unsigned i=0;i<12;i++) assert(submit());
        assert(!submit() && opened==1 && released==0);
        assert(!recomp_audio_output_take_discontinuity(0));
        voices[0].source->queued=0; assert(submit());
    } else if (!strcmp(mode,"concurrent")) {
        std::thread a([] {for(int i=0;i<100;i++) { submit(0); recomp_audio_output_reset_voice(0); }});
        std::thread b([] {for(int i=0;i<100;i++) { submit(256); recomp_audio_output_reset_voice(256); }});
        a.join(); b.join(); assert(opened==1);
    } else {
        assert(submit(1));
        fail_operation=mode;
        assert(!submit(0));
        assert(engine && released==0 && voices[1].source); /* isolate voice failure */
        assert(recomp_audio_output_take_discontinuity(0));
        fail_operation=nullptr; fake_ms=999; assert(!submit(0));
        fake_ms=1000; assert(submit(0));
        recomp_audio_output_reset_voice(0);
        assert(!recomp_audio_output_take_discontinuity(0));
    }
    recomp_audio_output_shutdown();
    assert(source_created==source_destroyed);
    unsigned before=opened; fake_ms+=10000; assert(!submit() && opened==before);
    puts("PASS: real XAudio adapter recovery");
}
