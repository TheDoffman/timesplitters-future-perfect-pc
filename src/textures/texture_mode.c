#include "texture_mode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static volatile LONG enabled;
static void mode_store(int value) { InterlockedExchange(&enabled,value); }
static int mode_load(void) { return (int)InterlockedCompareExchange(&enabled,0,0); }
#define mode_equal _stricmp
#else
#include <pthread.h>
#include <stdatomic.h>
#include <strings.h>
static pthread_once_t once = PTHREAD_ONCE_INIT;
static atomic_int enabled;
static void mode_store(int value) { atomic_store(&enabled,value); }
static int mode_load(void) { return atomic_load(&enabled); }
#define mode_equal strcasecmp
#endif

/* Keep the preference beside the launcher's other shared product settings. */
static int preference_path(char *out,size_t bytes)
{
    const char *source=getenv("TSFP_DISPLAY_CONFIG");
    if(!source || !*source)source=getenv("RECOMP_INPUT_CONFIG");
    if(!source || !*source)source="config/pc-display.ini";
    if(strlen(source)>=bytes)return 0;
    strcpy(out,source);
    char *a=strrchr(out,'/'),*b=strrchr(out,'\\');
    char *slash=a && b?(a>b?a:b):a?a:b;
    size_t prefix=slash?(size_t)(slash+1-out):0;
    if(prefix+strlen("pc-textures.ini")>=bytes)return 0;
    strcpy(out+prefix,"pc-textures.ini");return 1;
}
static int read_preference(void)
{
    char path[32768],version[32],tail;int value=-1;
    if(!preference_path(path,sizeof path))return -1;
    FILE *file=fopen(path,"rb");if(!file)return -1;
    if(fscanf(file,"%31s %d %c",version,&value,&tail)!=2 ||
       strcmp(version,"TSFP_TEXTURES_V1") || (value!=0 && value!=1))value=-1;
    fclose(file);return value;
}

static void configure(void)
{
    const char *mode = getenv("TSFP_TEXTURE_MODE");
    /* Preserve legacy launch defaults until a choice has been saved. */
    const char *pack = getenv("TSFP_TEXTURE_PACK_DIR");
    int preference=read_preference();
    mode_store(preference>=0 ? preference : mode ? !mode_equal(mode,"4k") : (pack && *pack));
    fprintf(stderr,"[TEXTURE-MODE] %s; missing replacements use Classic.\n",
            mode_load() ? "4K" : "Classic");
}

#ifdef _WIN32
static BOOL CALLBACK init(PINIT_ONCE unused, PVOID parameter, PVOID *context)
{
    (void)unused; (void)parameter; (void)context;
    configure();
    return TRUE;
}
#endif

int tsfp_texture_is_4k(void)
{
#ifdef _WIN32
    InitOnceExecuteOnce(&once,init,NULL,NULL);
#else
    pthread_once(&once,configure);
#endif
    return mode_load();
}

void tsfp_texture_set_4k(int value)
{
    tsfp_texture_is_4k();
    mode_store(!!value);
}

int tsfp_texture_save_4k(int value)
{
    char path[32768],temporary[32768];value=!!value;
    if(!preference_path(path,sizeof path) || strlen(path)+5>=sizeof temporary)return 0;
    strcpy(temporary,path);strcat(temporary,".tmp");
    FILE *file=fopen(temporary,"wb");if(!file)return 0;
    int ok=fprintf(file,"TSFP_TEXTURES_V1 %d\n",value)>0;
    if(fflush(file))ok=0;
    if(fclose(file))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temporary);
#else
    if(ok)ok=rename(temporary,path)==0;
    if(!ok)remove(temporary);
#endif
    if(ok)tsfp_texture_set_4k(value);
    return ok;
}

int tsfp_texture_toggle(void)
{
    int after;
    tsfp_texture_is_4k();
    after=!mode_load();
    if(!tsfp_texture_save_4k(after)) {
        fprintf(stderr,"[TEXTURE-MODE] Could not save texture preference; mode unchanged.\n");
        return mode_load();
    }
    fprintf(stderr,"[TEXTURE-MODE] %s; missing replacements use Classic.\n",after ? "4K" : "Classic");
    return after;
}
