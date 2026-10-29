/* Actual XAudio initialization and dB conversion; mock only the host device. */
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "hle/audio_output.h"
#include "gameplay/pc_spatial_audio.h"
using HRESULT=long;
#define FAILED(value) ((value)<0)
enum { COINIT_MULTITHREADED, XAUDIO2_DEFAULT_PROCESSOR };
static bool attempted,com_initialized,callback_registered;
static int callback,opened;
static float heard_gain=-1;
struct Master { HRESULT SetVolume(float gain) {heard_gain=gain;return 0;} } device;
static Master *master;
static constexpr unsigned kVoiceCount=4;
struct Source {
    float matrix[4];
    void SetOutputMatrix(Master *,unsigned src,unsigned dst,const float *m)
    {assert(dst==2);memcpy(matrix,m,src*dst*sizeof(float));}
} source;
struct OutputVoice {Source *source;unsigned channels;float original_matrix[4];} voices[kVoiceCount];
struct Spatial {float pan;int enabled;} spatial[kVoiceCount];
struct Engine {
    HRESULT RegisterForCallbacks(int *) {return 0;}
    HRESULT CreateMasteringVoice(Master **out,unsigned channels) {assert(channels==2);++opened;*out=&device;return 0;}
} instance;
static Engine *engine;
static void checkCriticalError() {}
static void disableOutput(const char *,HRESULT) {assert(false);}
static HRESULT CoInitializeEx(void *,int) {return 0;}
static HRESULT XAudio2Create(Engine **out,int,int) {*out=&instance;return 0;}
#include "xaudio_gain.inc"
int main(int argc,char **argv)
{
    assert(argc==2);
    float expected=std::strtof(argv[1],nullptr);
    recomp_audio_output_initialize();
    assert(opened==(expected>0));
    if(expected>0) {
        assert(std::fabs(heard_gain-expected)<0.00001f);
        assert(gainOf(0)==1.f);
        assert(std::fabs(gainOf(-600)-0.5011872f)<0.00001f);
        assert(std::fabs(gainOf(-2000)-0.1f)<0.00001f);
        assert(gainOf(-10000)==0.f);
        recomp_audio_output_initialize();assert(opened==1);
        voices[0]={&source,1,{1,1,0,0}};
        recomp_audio_output_set_spatial(0,-1,1);
        assert(source.matrix[0]==1 && source.matrix[1]==0);
        recomp_audio_output_set_spatial(0,1,1);
        assert(source.matrix[0]==0 && source.matrix[1]==1);
        recomp_audio_output_set_spatial(0,0,1);
        assert(std::fabs(source.matrix[0]-0.70710678f)<0.00001f);
        recomp_audio_output_set_spatial(0,0,0);
        assert(source.matrix[0]==1 && source.matrix[1]==1);
        recomp_audio_output_set_spatial(99,1,1); /* reject invalid slots */
    }
    puts("PASS: actual XAudio master initialization and voice attenuation");
}
