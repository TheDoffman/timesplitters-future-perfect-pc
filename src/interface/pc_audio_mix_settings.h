#ifndef PC_AUDIO_MIX_SETTINGS_H
#define PC_AUDIO_MIX_SETTINGS_H
#include <stdint.h>
#include <math.h>
#include "gameplay/pc_audio_mix.h"
/* Independent of the enhancement master: ordinary volume controls work in
 * either mode. Percentages attenuate the authored mix; 100 preserves it. */
typedef struct PcAudioMixOptions {int master,volume[PC_AUDIO_BUS_COUNT],ducking,dynamics;} PcAudioMixOptions;
#define PC_AUDIO_MIX_DEFAULTS {100,{100,100,100,100,100},0,0}
#ifdef __cplusplus
extern "C" {
#endif
PcAudioMixOptions pc_audio_mix_get(void);
int pc_audio_mix_set(const PcAudioMixOptions *options);
int32_t pc_audio_mix_level(int32_t original,int bus);
unsigned pc_audio_mix_dynamics(int bus);
/* A successful begin owns a nonblocking control guard until end. Read-only
 * audible-queue sampling happens outside the settings writer/audio callbacks. */
int pc_audio_mix_duck_begin(uint64_t now);
void pc_audio_mix_duck_end(int speech_active,uint64_t now);
#ifdef __cplusplus
}
#endif
static inline int32_t pc_audio_mix_apply(PcAudioMixOptions options,int32_t original,int bus)
{
    if(bus<0 || bus>=PC_AUDIO_BUS_COUNT)bus=PC_AUDIO_EFFECTS;
    int gain=options.master*options.volume[bus];
    if(gain==10000)return original; /* neutral path is exactly unchanged */
    if(gain<=0 || original<=-10000)return -10000;
    int32_t cut=(int32_t)lround(2000.*log10(gain/10000.));
    if(original>0)original=0;
    return original+cut < -10000?-10000:original+cut;
}
#endif
#if defined(PC_AUDIO_MIX_SETTINGS_IMPLEMENTATION) && !defined(PC_AUDIO_MIX_SETTINGS_IMPLEMENTED)
#define PC_AUDIO_MIX_SETTINGS_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "interface/pc_audio_config_path.h"
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif
static volatile int64_t pc_audio_mix_live=-1;
static volatile long pc_audio_mix_writer;
static volatile long pc_audio_duck_guard,pc_audio_duck_cut;
static uint64_t pc_audio_duck_last;
static float pc_audio_duck_amount;
static int pc_audio_duck_ready;
static int64_t pc_audio_mix_pack(PcAudioMixOptions o)
{
    uint64_t value=(unsigned)o.master;
    for(unsigned i=0;i<PC_AUDIO_BUS_COUNT;i++)value|=(uint64_t)(unsigned)o.volume[i]<<(7u*(i+1));
    value|=(uint64_t)(unsigned)o.ducking<<42;
    value|=(uint64_t)(unsigned)o.dynamics<<43;
    return (int64_t)value;
}
static PcAudioMixOptions pc_audio_mix_unpack(int64_t value)
{
    PcAudioMixOptions o;o.master=(int)(value&127);
    for(unsigned i=0;i<PC_AUDIO_BUS_COUNT;i++)o.volume[i]=(int)((value>>(7u*(i+1)))&127);
    o.ducking=(int)((value>>42)&1);o.dynamics=(int)((value>>43)&3);
    return o;
}
static int pc_audio_mix_valid(PcAudioMixOptions o)
{
    if(o.master<0 || o.master>100)return 0;
    for(unsigned i=0;i<PC_AUDIO_BUS_COUNT;i++)if(o.volume[i]<0 || o.volume[i]>100)return 0;
    if(o.ducking<0 || o.ducking>1 || o.dynamics<0 || o.dynamics>2)return 0;
    return 1;
}
static int pc_audio_mix_parse(const char *text,PcAudioMixOptions *out)
{
    const char *version="TSFP_AUDIO_MIX_V1";size_t prefix=strlen(version);
    while(isspace((unsigned char)*text))text++;
    int v2=!strncmp(text,"TSFP_AUDIO_MIX_V2",prefix);
    if((strncmp(text,version,prefix) && !v2) || !isspace((unsigned char)text[prefix]))return 0;
    text+=prefix;PcAudioMixOptions options=PC_AUDIO_MIX_DEFAULTS;
    for(unsigned i=0;i<(v2?8u:6u);i++) {
        while(isspace((unsigned char)*text))text++;
        if(*text<'0' || *text>'9' || (*text=='0' && text[1]>='0' && text[1]<='9'))return 0;
        unsigned value=0,count=0;
        while(*text>='0' && *text<='9') {
            if(++count>3)return 0;
            value=value*10u+(unsigned)(*text++-'0');
        }
        if(value>100 || (*text && !isspace((unsigned char)*text)))return 0;
        if(!i)options.master=(int)value;
        else if(i<6)options.volume[i-1]=(int)value;
        else if(i==6)options.ducking=(int)value;
        else options.dynamics=(int)value;
    }
    while(isspace((unsigned char)*text))text++;
    if(*text || !pc_audio_mix_valid(options))return 0;*out=options;return 1;
}
static int pc_audio_mix_read(PcAudioMixOptions *out)
{
    char path[32768],text[256];
    if(!pc_audio_config_path(path,sizeof path,"pc-audio-mix.ini"))return -1;
    FILE *file=fopen(path,"rb");if(!file)return errno==ENOENT?0:-1;
    size_t bytes=fread(text,1,sizeof text-1,file);
    int ok=!ferror(file) && bytes<sizeof text-1 && !memchr(text,0,bytes);
    if(fclose(file))ok=0;text[bytes]=0;
    return ok && pc_audio_mix_parse(text,out)?1:-1;
}
PcAudioMixOptions pc_audio_mix_get(void)
{
#ifdef _WIN32
    int64_t live=_InterlockedCompareExchange64(&pc_audio_mix_live,0,0);
#else
    int64_t live=__atomic_load_n(&pc_audio_mix_live,__ATOMIC_ACQUIRE);
#endif
    if(live>=0)return pc_audio_mix_unpack(live);
    PcAudioMixOptions options=PC_AUDIO_MIX_DEFAULTS;(void)pc_audio_mix_read(&options);
    int64_t wanted=pc_audio_mix_pack(options);
#ifdef _WIN32
    live=_InterlockedCompareExchange64(&pc_audio_mix_live,wanted,-1);
#else
    live=-1;__atomic_compare_exchange_n(&pc_audio_mix_live,&live,wanted,0,__ATOMIC_RELEASE,__ATOMIC_ACQUIRE);
#endif
    return pc_audio_mix_unpack(live<0?wanted:live);
}
int pc_audio_mix_set(const PcAudioMixOptions *options)
{
    char path[32768],temporary[32768];PcAudioMixOptions old;
    if(!options || !pc_audio_mix_valid(*options) ||
       !pc_audio_config_path(path,sizeof path,"pc-audio-mix.ini") || strlen(path)+5>=sizeof temporary)return 0;
#ifdef _WIN32
    if(_InterlockedCompareExchange(&pc_audio_mix_writer,1,0))return 0;
#else
    if(__atomic_exchange_n(&pc_audio_mix_writer,1,__ATOMIC_ACQUIRE))return 0;
#endif
    int ok=0;
    if(pc_audio_mix_read(&old)<0)goto done;
    snprintf(temporary,sizeof temporary,"%s.tmp",path);
    FILE *file=fopen(temporary,"wb");if(!file)goto done;
    int v2=options->ducking || options->dynamics;
    ok=fprintf(file,"TSFP_AUDIO_MIX_V%d %d %d %d %d %d %d",v2?2:1,options->master,
        options->volume[PC_AUDIO_EFFECTS],options->volume[PC_AUDIO_MUSIC],options->volume[PC_AUDIO_DIALOGUE],
        options->volume[PC_AUDIO_BACKGROUND],options->volume[PC_AUDIO_MOVIES])>0;
    if(v2 && fprintf(file," %d %d",options->ducking,options->dynamics)<0)ok=0;
    if(fputc('\n',file)==EOF)ok=0;
    if(fflush(file))ok=0;if(fclose(file))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temporary);
    if(ok)_InterlockedExchange64(&pc_audio_mix_live,pc_audio_mix_pack(*options));
#else
    if(ok)ok=rename(temporary,path)==0;
    if(!ok)remove(temporary);
    if(ok)__atomic_store_n(&pc_audio_mix_live,pc_audio_mix_pack(*options),__ATOMIC_RELEASE);
#endif
done:
#ifdef _WIN32
    _InterlockedExchange(&pc_audio_mix_writer,0);
#else
    __atomic_store_n(&pc_audio_mix_writer,0,__ATOMIC_RELEASE);
#endif
    return ok;
}
static long pc_audio_duck_read(void)
{
#ifdef _WIN32
    return _InterlockedCompareExchange(&pc_audio_duck_cut,0,0);
#else
    return __atomic_load_n(&pc_audio_duck_cut,__ATOMIC_ACQUIRE);
#endif
}
static void pc_audio_duck_publish(long cut)
{
#ifdef _WIN32
    _InterlockedExchange(&pc_audio_duck_cut,cut);
#else
    __atomic_store_n(&pc_audio_duck_cut,cut,__ATOMIC_RELEASE);
#endif
}
static void pc_audio_duck_unlock(void)
{
#ifdef _WIN32
    _InterlockedExchange(&pc_audio_duck_guard,0);
#else
    __atomic_store_n(&pc_audio_duck_guard,0,__ATOMIC_RELEASE);
#endif
}
int pc_audio_mix_duck_begin(uint64_t now)
{
#ifdef _WIN32
    if(_InterlockedCompareExchange(&pc_audio_duck_guard,1,0))return 0;
#else
    if(__atomic_exchange_n(&pc_audio_duck_guard,1,__ATOMIC_ACQUIRE))return 0;
#endif
    if(!pc_audio_mix_get().ducking) {
        pc_audio_duck_amount=0;pc_audio_duck_ready=0;pc_audio_duck_publish(0);
        pc_audio_duck_unlock();return 0;
    }
    if(pc_audio_duck_ready && now>=pc_audio_duck_last && now-pc_audio_duck_last<5) {
        pc_audio_duck_unlock();return 0;
    }
    if(pc_audio_duck_ready && now<pc_audio_duck_last) {pc_audio_duck_ready=0;pc_audio_duck_amount=0;}
    return 1;
}
void pc_audio_mix_duck_end(int active,uint64_t now)
{
    uint64_t elapsed=pc_audio_duck_ready && now>=pc_audio_duck_last?now-pc_audio_duck_last:5;
    float target=active?1.f:0.f,tau=active?20.f:300.f;
    pc_audio_duck_amount=target+(pc_audio_duck_amount-target)*expf(-(float)elapsed/tau);
    if(pc_audio_duck_amount<.00001f)pc_audio_duck_amount=0;
    if(pc_audio_duck_amount>.99999f)pc_audio_duck_amount=1;
    pc_audio_duck_publish((long)lroundf(-600.f*pc_audio_duck_amount));
    pc_audio_duck_last=now;pc_audio_duck_ready=1;pc_audio_duck_unlock();
}
int32_t pc_audio_mix_level(int32_t original,int bus)
{
    if(bus<0 || bus>=PC_AUDIO_BUS_COUNT)bus=PC_AUDIO_EFFECTS;
    PcAudioMixOptions options=pc_audio_mix_get();
    int32_t level=pc_audio_mix_apply(options,original,bus);
    if(options.ducking && (bus==PC_AUDIO_EFFECTS || bus==PC_AUDIO_MUSIC || bus==PC_AUDIO_BACKGROUND)) {
        int32_t cut=(int32_t)pc_audio_duck_read();
        if(cut)return level<=-10000?-10000:(level+cut<-10000?-10000:level+cut);
    }
    return level;
}
unsigned pc_audio_mix_dynamics(int bus)
{return bus==PC_AUDIO_DIALOGUE?0u:(unsigned)pc_audio_mix_get().dynamics;}
#endif
