/* Fake only the OS/device boundary; include the entire production adapter. */
#include <cassert>
#include <cstdint>
#include <cstring>
#include <atomic>
using HRESULT = int32_t;
using WORD = uint16_t;
#define STDMETHODCALLTYPE
#define FAILED(x) ((x) < 0)
#define SUCCEEDED(x) ((x) >= 0)
constexpr HRESULT S_OK = 0, E_FAIL = -1;
constexpr HRESULT XAUDIO2_E_DEVICE_INVALIDATED = -2, RPC_E_CHANGED_MODE = -3;
constexpr unsigned COINIT_MULTITHREADED = 0, XAUDIO2_DEFAULT_PROCESSOR = 0;
constexpr unsigned XAUDIO2_MIN_SAMPLE_RATE = 1000, XAUDIO2_MAX_SAMPLE_RATE = 200000;
constexpr unsigned WAVE_FORMAT_PCM = 1, XAUDIO2_VOICE_NOPITCH = 1, XAUDIO2_VOICE_NOSAMPLESPLAYED = 1;
struct LARGE_INTEGER { int64_t QuadPart; };
static uint64_t fake_ms;
static void QueryPerformanceFrequency(LARGE_INTEGER *v) { v->QuadPart = 1000; }
static void QueryPerformanceCounter(LARGE_INTEGER *v) { v->QuadPart = fake_ms; }
static const char *fail_operation;
static HRESULT fail_code = E_FAIL;
static HRESULT result(const char *op) { return fail_operation && !strcmp(fail_operation, op) ? fail_code : S_OK; }
static unsigned opened, released, source_created, source_destroyed, master_destroyed;
static std::atomic<unsigned> com_started{0}, com_ended{0};
static float heard_gain;
static HRESULT CoInitializeEx(void *, unsigned) { ++com_started; return result("com"); }
static void CoUninitialize() { ++com_ended; }
struct WAVEFORMATEX { WORD wFormatTag,nChannels; uint32_t nSamplesPerSec,nAvgBytesPerSec; WORD nBlockAlign,wBitsPerSample; };
struct XAUDIO2_VOICE_STATE { uint64_t SamplesPlayed; uint32_t BuffersQueued; };
struct XAUDIO2_BUFFER { uint32_t AudioBytes; const uint8_t *pAudioData; };
struct XAUDIO2_VOICE_DETAILS { uint32_t InputSampleRate; };
struct XAUDIO2_PERFORMANCE_DATA { uint32_t CurrentLatencyInSamples,GlitchesSinceEngineStarted,ActiveSourceVoiceCount,TotalSourceVoiceCount; };
struct IXAudio2EngineCallback {
    virtual void OnProcessingPassStart() = 0;
    virtual void OnProcessingPassEnd() = 0;
    virtual void OnCriticalError(HRESULT) = 0;
};
static IXAudio2EngineCallback *registered;
struct IXAudio2SourceVoice {
    uint32_t queued = 0;
    uint64_t played = 0;
    const uint8_t *last_pcm = nullptr;
    void DestroyVoice() { ++source_destroyed; delete this; }
    HRESULT Start() { return result("start"); }
    HRESULT SetVolume(float) { return result("volume"); }
    void GetState(XAUDIO2_VOICE_STATE *s, unsigned) { s->SamplesPlayed=played; s->BuffersQueued=queued; }
    HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER *b) {
        if (FAILED(result("submit"))) return result("submit");
        ++queued; last_pcm=b->pAudioData; return S_OK;
    }
};
struct IXAudio2MasteringVoice {
    void DestroyVoice() { ++master_destroyed; delete this; }
    HRESULT SetVolume(float gain) { heard_gain=gain; return result("master-volume"); }
    void GetVoiceDetails(XAUDIO2_VOICE_DETAILS *d) { d->InputSampleRate=48000; }
};
struct IXAudio2 {
    HRESULT RegisterForCallbacks(IXAudio2EngineCallback *cb) {
        if (FAILED(result("register"))) return result("register"); registered=cb; return S_OK;
    }
    void UnregisterForCallbacks(IXAudio2EngineCallback *) { registered=nullptr; }
    void Release() { ++released; delete this; }
    HRESULT CreateMasteringVoice(IXAudio2MasteringVoice **out) {
        if (FAILED(result("master"))) return result("master"); *out=new IXAudio2MasteringVoice; return S_OK;
    }
    HRESULT CreateSourceVoice(IXAudio2SourceVoice **out, const WAVEFORMATEX *, unsigned, float) {
        if (FAILED(result("create"))) return result("create"); *out=new IXAudio2SourceVoice; ++source_created; return S_OK;
    }
    void GetPerformanceData(XAUDIO2_PERFORMANCE_DATA *p) { *p={}; }
};
static HRESULT XAudio2Create(IXAudio2 **out, unsigned, unsigned) {
    ++opened;
    if (FAILED(result("engine"))) return result("engine"); *out=new IXAudio2; return S_OK;
}
