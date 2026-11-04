#ifndef TSFP_GRAPHICS_SETTINGS_H
#define TSFP_GRAPHICS_SETTINGS_H
#include "graphics_options.h"
TsfpGraphicsOptions pc_graphics_get(void);
int pc_graphics_set(const TsfpGraphicsOptions *options);
int pc_graphics_clear_cache(void); /* removed file count, or -1 on failure */

static inline int pc_graphics_path(char *out,size_t size)
{
    const char *source=getenv("TSFP_DISPLAY_CONFIG");
    char *a,*b,*slash;
    size_t prefix;
    if(!source || !*source) source=getenv("RECOMP_INPUT_CONFIG");
    if(!source || !*source) source="config/pc-display.ini";
    if(strlen(source)>=size) return 0;
    strcpy(out,source);
    a=strrchr(out,'/'); b=strrchr(out,'\\');
    slash=a && b?(a>b?a:b):a?a:b;
    prefix=slash?(size_t)(slash+1-out):0;
    if(prefix+sizeof("pc-graphics.ini")>size) return 0;
    strcpy(out+prefix,"pc-graphics.ini");return 1;
}
#ifdef TSFP_GRAPHICS_SETTINGS_IMPLEMENTATION
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <unistd.h>
#endif

static volatile long pc_graphics_state = -1;
static long pc_graphics_atomic_read(void)
{
#ifdef _WIN32
    return _InterlockedCompareExchange(&pc_graphics_state,0,0);
#else
    return __atomic_load_n(&pc_graphics_state,__ATOMIC_ACQUIRE);
#endif
}
static long pc_graphics_pack(const TsfpGraphicsOptions *o)
{
    return o->msaa | (o->edge_smoothing << 4) | (o->anisotropy << 5) |
           (o->ambient_occlusion << 10) | (o->shader_cache << 11) |
           (o->supersampling << 12);
}
TsfpGraphicsOptions pc_graphics_get(void)
{
    long state=pc_graphics_atomic_read();
    TsfpGraphicsOptions o=tsfp_graphics_defaults();
    if(state<0) {
        char path[32768];FILE *file;
#ifdef TSFP_GRAPHICS_DEFAULT_ANISOTROPY
        int legacy=TSFP_GRAPHICS_DEFAULT_ANISOTROPY;
        if(legacy>=1 && legacy<=16)o.anisotropy=legacy;
#endif
        if(pc_graphics_path(path,sizeof path) && (file=fopen(path,"rb"))!=NULL) {
            tsfp_graphics_read(file,&o);fclose(file);
        }
        state=pc_graphics_pack(&o);
#ifdef _WIN32
        long previous=_InterlockedCompareExchange(&pc_graphics_state,state,-1);
        if(previous>=0) state=previous;
#else
        long expected=-1;
        if(!__atomic_compare_exchange_n(&pc_graphics_state,&expected,state,0,__ATOMIC_RELEASE,__ATOMIC_ACQUIRE)) state=expected;
#endif
    }
    o.msaa=(int)(state&15);o.edge_smoothing=(int)((state>>4)&1);
    o.anisotropy=(int)((state>>5)&31);o.ambient_occlusion=(int)((state>>10)&1);
    o.shader_cache=(int)((state>>11)&1);o.supersampling=(int)((state>>12)&3);return o;
}

/* UI-thread operation. Save before publishing a complete atomic snapshot to
 * rendering threads. A failed write leaves the current rendering unchanged. */
int pc_graphics_set(const TsfpGraphicsOptions *options)
{
    char path[32768],temporary[32768];FILE *file;int ok;
    if(!tsfp_graphics_valid(options) || !pc_graphics_path(path,sizeof path) ||
       strlen(path)+5>=sizeof temporary) return 0;
    /* Length was checked above; avoid compiler-dependent truncation warnings. */
    memcpy(temporary,path,strlen(path));
    memcpy(temporary+strlen(path),".tmp",sizeof(".tmp"));
    file=fopen(temporary,"wb");if(!file)return 0;
    ok=tsfp_graphics_write(file,options);
    if(fflush(file))ok=0;
    if(fclose(file))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temporary);
    if(ok)_InterlockedExchange(&pc_graphics_state,pc_graphics_pack(options));
#else
    if(ok)ok=rename(temporary,path)==0;
    if(!ok)remove(temporary);
    if(ok)__atomic_store_n(&pc_graphics_state,pc_graphics_pack(options),__ATOMIC_RELEASE);
#endif
    return ok;
}
#endif
#endif
