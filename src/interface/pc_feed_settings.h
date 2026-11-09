#ifndef PC_FEED_SETTINGS_H
#define PC_FEED_SETTINGS_H
/* Shared product setting, exposed by the title's frontend and pause menus. */
#define PC_FEED_LABEL "Kill Feed"
int pc_feed_enabled(void);
int pc_feed_set_enabled(int enabled); /* save first; publish only on success */
#ifdef PC_FEED_SETTINGS_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif
static volatile long pc_feed_live=-1;
static long pc_feed_load_state(void)
{
#ifdef _WIN32
    return _InterlockedCompareExchange(&pc_feed_live,0,0);
#else
    return __atomic_load_n(&pc_feed_live,__ATOMIC_ACQUIRE);
#endif
}
static int pc_feed_path(char *out,size_t bytes)
{
    const char *source=getenv("TSFP_DISPLAY_CONFIG");
    if(!source || !*source)source=getenv("RECOMP_INPUT_CONFIG");
    if(!source || !*source)source="config/pc-display.ini";
    if(strlen(source)>=bytes)return 0;
    strcpy(out,source);char *a=strrchr(out,'/'),*b=strrchr(out,'\\');
    char *slash=a && b?(a>b?a:b):a?a:b;
    size_t prefix=slash?(size_t)(slash+1-out):0;
    if(prefix+strlen("pc-hud.ini")>=bytes)return 0;
    strcpy(out+prefix,"pc-hud.ini");return 1;
}
static int pc_feed_read_preference(void)
{
    char path[32768],version[32],tail;int enabled=-1;
    if(!pc_feed_path(path,sizeof path))return -1;
    FILE *file=fopen(path,"rb");if(!file)return -1;
    if(fscanf(file,"%31s %d %c",version,&enabled,&tail)!=2 ||
       strcmp(version,"TSFP_HUD_V1") || (enabled!=0 && enabled!=1))enabled=-1;
    fclose(file);return enabled;
}
int pc_feed_enabled(void)
{
    long state=pc_feed_load_state();if(state>=0)return state!=0;
    int preference=pc_feed_read_preference();
    if(preference<0)preference=0;
#ifdef _WIN32
    state=_InterlockedCompareExchange(&pc_feed_live,preference,-1);
#else
    state=-1;__atomic_compare_exchange_n(&pc_feed_live,&state,preference,0,__ATOMIC_RELEASE,__ATOMIC_ACQUIRE);
#endif
    return (state<0?preference:state)!=0;
}
int pc_feed_set_enabled(int enabled)
{
    char path[32768],temporary[32768];enabled=!!enabled;
    if(!pc_feed_path(path,sizeof path) || strlen(path)+5>=sizeof temporary)return 0;
    snprintf(temporary,sizeof temporary,"%s.tmp",path);
    FILE *file=fopen(temporary,"wb");if(!file)return 0;
    int ok=fprintf(file,"TSFP_HUD_V1 %d\n",enabled)>0;
    if(fflush(file))ok=0;if(fclose(file))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temporary);
    if(ok)_InterlockedExchange(&pc_feed_live,enabled);
#else
    if(ok)ok=rename(temporary,path)==0;
    if(!ok)remove(temporary);
    if(ok)__atomic_store_n(&pc_feed_live,enabled,__ATOMIC_RELEASE);
#endif
    return ok;
}
#endif
#endif
