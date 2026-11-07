#ifndef PC_AUDIO_SETTINGS_H
#define PC_AUDIO_SETTINGS_H
/* Shared product setting, exposed by the title's frontend and pause menus. */
#define PC_AUDIO_BETA_LABEL "Enhanced Audio (Beta)"
typedef struct PcAudioOptions {int enhanced,spatial,weapon_range,environment,ambience;} PcAudioOptions;
typedef enum PcAudioFeature {PC_AUDIO_SPATIAL,PC_AUDIO_WEAPON_RANGE,
    PC_AUDIO_ENVIRONMENT,PC_AUDIO_AMBIENCE} PcAudioFeature;
#ifdef __cplusplus
extern "C" {
#endif
PcAudioOptions pc_audio_get_options(void);
int pc_audio_set_options(const PcAudioOptions *options);
int pc_audio_feature_enabled(PcAudioFeature feature);
int pc_audio_enhanced_enabled(void);
int pc_audio_set_enhanced(int enabled); /* save first; publish only on success */
#ifdef __cplusplus
}
#endif
#ifdef PC_AUDIO_SETTINGS_IMPLEMENTATION
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
static volatile long pc_audio_live=-1;
static volatile long pc_audio_writer;
static long pc_audio_load_state(void)
{
#ifdef _WIN32
    return _InterlockedCompareExchange(&pc_audio_live,0,0);
#else
    return __atomic_load_n(&pc_audio_live,__ATOMIC_ACQUIRE);
#endif
}
static int pc_audio_path(char *out,size_t bytes)
{
    return pc_audio_config_path(out,bytes,"pc-audio.ini");
}
static int pc_audio_pack(PcAudioOptions o)
{return o.enhanced|(o.spatial<<1)|(o.weapon_range<<2)|(o.environment<<3)|(o.ambience<<4);}
static PcAudioOptions pc_audio_unpack(long state)
{PcAudioOptions o={(int)(state&1),(int)((state>>1)&1),(int)((state>>2)&1),
    (int)((state>>3)&1),(int)((state>>4)&1)};return o;}
static int pc_audio_valid(PcAudioOptions o)
{return (o.enhanced==0 || o.enhanced==1) && (o.spatial==0 || o.spatial==1) &&
    (o.weapon_range==0 || o.weapon_range==1) && (o.environment==0 || o.environment==1) &&
    (o.ambience==0 || o.ambience==1);}
/* Bounded text parsing rejects overflow, suffixes and unsupported versions. */
static int pc_audio_parse(const char *text,PcAudioOptions *out)
{
    PcAudioOptions o={0,1,1,1,1};int count=0;
    while(isspace((unsigned char)*text))text++;
    if(!strncmp(text,"TSFP_AUDIO_V1",13) && isspace((unsigned char)text[13]))count=1;
    else if(!strncmp(text,"TSFP_AUDIO_V2",13) && isspace((unsigned char)text[13]))count=5;
    else return 0;
    text+=13;int *fields[]={&o.enhanced,&o.spatial,&o.weapon_range,&o.environment,&o.ambience};
    for(int i=0;i<count;i++) {
        while(isspace((unsigned char)*text))text++;
        if((*text!='0' && *text!='1') || (text[1] && !isspace((unsigned char)text[1])))return 0;
        *fields[i]=*text++-'0';
    }
    while(isspace((unsigned char)*text))text++;
    if(*text)return 0;
    *out=o;return 1;
}
/* 1 valid, 0 absent, -1 unreadable/malformed: never overwrite an unknown file. */
static int pc_audio_read_preference(PcAudioOptions *out)
{
    char path[32768],text[256];
    if(!pc_audio_path(path,sizeof path))return -1;
    FILE *file=fopen(path,"rb");if(!file)return errno==ENOENT?0:-1;
    size_t bytes=fread(text,1,sizeof text-1,file);
    int ok=!ferror(file) && bytes<sizeof text-1 && !memchr(text,0,bytes);
    if(fclose(file))ok=0;text[bytes]=0;
    return ok && pc_audio_parse(text,out)?1:-1;
}
PcAudioOptions pc_audio_get_options(void)
{
    long state=pc_audio_load_state();if(state>=0)return pc_audio_unpack(state);
    PcAudioOptions options={0,1,1,1,1};
    if(pc_audio_read_preference(&options)==0) {
        const char *value=getenv("RECOMP_ENHANCED_AUDIO");
        if(!value || !*value)value=getenv("RECOMP_ACOUSTICS");
        options.enhanced=value && (!strcmp(value,"1") || !strcmp(value,"on"));
    }
    int preference=pc_audio_pack(options);
#ifdef _WIN32
    state=_InterlockedCompareExchange(&pc_audio_live,preference,-1);
#else
    state=-1;__atomic_compare_exchange_n(&pc_audio_live,&state,preference,0,__ATOMIC_RELEASE,__ATOMIC_ACQUIRE);
#endif
    return pc_audio_unpack(state<0?preference:state);
}
int pc_audio_enhanced_enabled(void) {return pc_audio_get_options().enhanced;}
int pc_audio_feature_enabled(PcAudioFeature feature)
{
    PcAudioOptions o=pc_audio_get_options();
    if(!o.enhanced)return 0;
    switch(feature) {
    case PC_AUDIO_SPATIAL:return o.spatial;
    case PC_AUDIO_WEAPON_RANGE:return o.weapon_range;
    case PC_AUDIO_ENVIRONMENT:return o.environment;
    case PC_AUDIO_AMBIENCE:return o.ambience;
    default:return 0;
    }
}
int pc_audio_set_options(const PcAudioOptions *options)
{
    if(!options || !pc_audio_valid(*options))return 0;
    char path[32768],temporary[32768];PcAudioOptions prior;
    if(!pc_audio_path(path,sizeof path) || strlen(path)+5>=sizeof temporary)return 0;
#ifdef _WIN32
    if(_InterlockedCompareExchange(&pc_audio_writer,1,0))return 0;
#else
    if(__atomic_exchange_n(&pc_audio_writer,1,__ATOMIC_ACQUIRE))return 0;
#endif
    int ok=0;
    if(pc_audio_read_preference(&prior)<0)goto done;
    snprintf(temporary,sizeof temporary,"%s.tmp",path);
    FILE *file=fopen(temporary,"wb");if(!file)goto done;
    /* Keep the legacy representation when it describes the whole preference. */
    if(options->spatial && options->weapon_range && options->environment && options->ambience)
        ok=fprintf(file,"TSFP_AUDIO_V1 %d\n",options->enhanced)>0;
    else ok=fprintf(file,"TSFP_AUDIO_V2 %d %d %d %d %d\n",options->enhanced,options->spatial,
        options->weapon_range,options->environment,options->ambience)>0;
    if(fflush(file))ok=0;if(fclose(file))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temporary);
    if(ok)_InterlockedExchange(&pc_audio_live,pc_audio_pack(*options));
#else
    if(ok)ok=rename(temporary,path)==0;
    if(!ok)remove(temporary);
    if(ok)__atomic_store_n(&pc_audio_live,pc_audio_pack(*options),__ATOMIC_RELEASE);
#endif
done:
#ifdef _WIN32
    _InterlockedExchange(&pc_audio_writer,0);
#else
    __atomic_store_n(&pc_audio_writer,0,__ATOMIC_RELEASE);
#endif
    return ok;
}
int pc_audio_set_enhanced(int enabled)
{PcAudioOptions o=pc_audio_get_options();o.enhanced=!!enabled;return pc_audio_set_options(&o);}
#endif
#endif
