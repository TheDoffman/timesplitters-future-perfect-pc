/* Bounded Windows process-loopback capture for private game listening checks.
 * Includes only the requested process tree. Never falls back to an endpoint
 * or microphone capture. Uses the installed Windows SDK, no additional driver.
 * API: learn.microsoft.com/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-activateaudiointerfaceasync
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A00000A
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <wrl.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <vector>
#include <limits>
#pragma comment(lib,"Mmdevapi.lib")
#pragma comment(lib,"Ole32.lib")
using Microsoft::WRL::ComPtr;

struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE v=INVALID_HANDLE_VALUE):value(v){}
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    explicit operator bool()const{return value && value!=INVALID_HANDLE_VALUE;}
};
class Activation final:public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IActivateAudioInterfaceCompletionHandler,Microsoft::WRL::FtmBase> {
public:
    Handle ready{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    ComPtr<IAudioClient> client;
    AUDIOCLIENT_ACTIVATION_PARAMS options{};
    PROPVARIANT parameters{};
    std::atomic<HRESULT> result{E_PENDING};
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation) override {
        HRESULT activated=E_FAIL;ComPtr<IUnknown> object;
        HRESULT hr=operation->GetActivateResult(&activated,&object);
        if(SUCCEEDED(hr))hr=activated;
        if(SUCCEEDED(hr))hr=object.As(&client);
        result.store(hr,std::memory_order_release);SetEvent(ready.value);return S_OK;
    }
};
#pragma pack(push,1)
struct WaveHeader {
    char riff[4];uint32_t riff_bytes;char wave[4],fmt[4];uint32_t fmt_bytes;
    uint16_t tag,channels;uint32_t rate,bytes_per_second;uint16_t align,bits;
    char data[4];uint32_t data_bytes;
};
#pragma pack(pop)
static_assert(sizeof(WaveHeader)==44,"PCM WAV layout");
static bool write_all(HANDLE file,const void *data,DWORD bytes)
{
    DWORD written=0;return WriteFile(file,data,bytes,&written,nullptr) && written==bytes;
}
static bool header(HANDLE file,uint32_t bytes)
{
    WaveHeader h{};memcpy(h.riff,"RIFF",4);memcpy(h.wave,"WAVE",4);memcpy(h.fmt,"fmt ",4);memcpy(h.data,"data",4);
    h.riff_bytes=36+bytes;h.fmt_bytes=16;h.tag=WAVE_FORMAT_PCM;h.channels=2;h.rate=48000;
    h.bytes_per_second=192000;h.align=4;h.bits=16;h.data_bytes=bytes;
    LARGE_INTEGER zero{};return SetFilePointerEx(file,zero,nullptr,FILE_BEGIN) && write_all(file,&h,sizeof h);
}
static bool number(const wchar_t *text,uint32_t *value)
{
    if(!text || !*text)return false;
    uint64_t n=0;
    for(;*text;text++) {
        if(*text<L'0' || *text>L'9')return false;
        n=n*10+(*text-L'0');if(n>std::numeric_limits<uint32_t>::max())return false;
    }
    *value=(uint32_t)n;return true;
}
static int failure(const char *step,HRESULT hr)
{fprintf(stderr,"Process audio capture failed at %s: 0x%08lX\n",step,(unsigned long)hr);return 1;}
int wmain(int argc,wchar_t **argv)
{
    if(argc==2 && !wcscmp(argv[1],L"--help")) {
        puts("capture-game-audio <PID> <new-output.wav> <seconds:1..120>\n"
             "Captures only that process tree; existing output files are preserved.");return 0;
    }
    uint32_t pid=0,seconds=0;
    if(argc!=4 || !number(argv[1],&pid) || !pid || !number(argv[3],&seconds) || seconds<1 || seconds>120) {
        fputs("Usage: capture-game-audio <PID> <new-output.wav> <seconds:1..120>\n",stderr);return 2;
    }
    /* Preflight before activating any audio interface. CREATE_NEW also closes
     * a check/create race and protects selected listening evidence. */
    Handle file(CreateFileW(argv[2],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(!file)return failure("new output file",HRESULT_FROM_WIN32(GetLastError()));
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
    if(!process)return failure("target process",HRESULT_FROM_WIN32(GetLastError()));
    if(WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT)return failure("target already exited",E_INVALIDARG);
    HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(hr))return failure("COM initialization",hr);
    struct ComLifetime{~ComLifetime(){CoUninitialize();}} com_lifetime;
    auto completion=Microsoft::WRL::Make<Activation>();
    if(!completion || !completion->ready)return failure("activation event",E_OUTOFMEMORY);
    auto &options=completion->options;
    options.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    options.ProcessLoopbackParams.TargetProcessId=pid;
    options.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    auto &parameters=completion->parameters;parameters.vt=VT_BLOB;
    parameters.blob.cbSize=sizeof options;parameters.blob.pBlobData=(BYTE *)&options;
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    hr=ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),
        &parameters,completion.Get(),&operation);
    if(FAILED(hr))return failure("process loopback activation",hr);
    if(WaitForSingleObject(completion->ready.value,5000)!=WAIT_OBJECT_0)
        return failure("activation timeout",HRESULT_FROM_WIN32(WAIT_TIMEOUT));
    hr=completion->result.load(std::memory_order_acquire);
    if(FAILED(hr))return failure("activated process interface",hr);
    auto client=completion->client;
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
    format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
    hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,0,0,&format,nullptr);
    if(FAILED(hr))return failure("capture format",hr);
    Handle samples(CreateEventW(nullptr,FALSE,FALSE,nullptr));
    if(!samples)return failure("sample event",HRESULT_FROM_WIN32(GetLastError()));
    hr=client->SetEventHandle(samples.value);if(FAILED(hr))return failure("capture event",hr);
    ComPtr<IAudioCaptureClient> capture;
    hr=client->GetService(IID_PPV_ARGS(&capture));if(FAILED(hr))return failure("capture service",hr);
    if(!header(file.value,0))return failure("WAV header",HRESULT_FROM_WIN32(GetLastError()));
    hr=client->Start();if(FAILED(hr))return failure("capture start",hr);
    const uint64_t limit=(uint64_t)seconds*format.nSamplesPerSec;
    uint64_t frames=0,silent=0,packets=0,discontinuities=0,timestamp_errors=0;
    const ULONGLONG start=GetTickCount64();bool target_exited=false;
    while(frames<limit && GetTickCount64()-start<(uint64_t)seconds*1000) {
        if(WaitForSingleObject(process.value,0)==WAIT_OBJECT_0){target_exited=true;break;}
        DWORD wait=WaitForSingleObject(samples.value,100);
        if(wait==WAIT_TIMEOUT)continue;
        if(wait!=WAIT_OBJECT_0){DWORD error=GetLastError();hr=HRESULT_FROM_WIN32(error?error:ERROR_INVALID_HANDLE);break;}
        UINT32 available=0;hr=capture->GetNextPacketSize(&available);
        while(SUCCEEDED(hr) && available && frames<limit && GetTickCount64()-start<(uint64_t)seconds*1000) {
            BYTE *data=nullptr;UINT32 count=0;DWORD flags=0;UINT64 device=0,qpc=0;
            hr=capture->GetBuffer(&data,&count,&flags,&device,&qpc);if(FAILED(hr))break;
            if(hr==AUDCLNT_S_BUFFER_EMPTY)break;
            if(!count){hr=capture->ReleaseBuffer(0);break;}
            UINT32 take=(UINT32)((limit-frames)<count?(limit-frames):count);
            DWORD bytes=take*format.nBlockAlign;bool ok=true;
            if(flags&AUDCLNT_BUFFERFLAGS_SILENT) {
                std::vector<BYTE> zeros(bytes,0);ok=write_all(file.value,zeros.data(),bytes);silent+=take;
            } else ok=data && write_all(file.value,data,bytes);
            DWORD file_error=ok?ERROR_SUCCESS:GetLastError();
            HRESULT release=capture->ReleaseBuffer(count);
            if(!ok){hr=HRESULT_FROM_WIN32(file_error?file_error:ERROR_WRITE_FAULT);break;}
            if(FAILED(release)){hr=release;break;}
            frames+=take;packets++;
            if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)discontinuities++;
            if(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)timestamp_errors++;
            hr=capture->GetNextPacketSize(&available);
        }
        if(FAILED(hr))break;
    }
    HRESULT stopped=client->Stop();if(SUCCEEDED(hr) && FAILED(stopped))hr=stopped;
    if(!header(file.value,(uint32_t)(frames*format.nBlockAlign)) || !FlushFileBuffers(file.value))
        return failure("final WAV header",HRESULT_FROM_WIN32(GetLastError()));
    if(FAILED(hr))return failure("capture packets (partial file retained)",hr);
    printf("{\"pid\":%lu,\"rate\":48000,\"channels\":2,\"bits\":16,\"frames\":%llu,"
           "\"elapsed_ms\":%llu,\"packets\":%llu,\"silent_frames\":%llu,\"discontinuities\":%llu,"
           "\"timestamp_errors\":%llu,\"target_exited\":%s}\n",(unsigned long)pid,
           (unsigned long long)frames,(unsigned long long)(GetTickCount64()-start),
           (unsigned long long)packets,(unsigned long long)silent,(unsigned long long)discontinuities,
           (unsigned long long)timestamp_errors,target_exited?"true":"false");
    return 0;
}
