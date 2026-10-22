/* Shared persistent frame-rate preference. The menu queues a live update only
 * after saving succeeds. Keep this separate from resolution previews. */
#ifndef TSFP_PC_FRAME_RATE_SETTINGS_H
#define TSFP_PC_FRAME_RATE_SETTINGS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define PC_FRAME_RATE_COUNT 3
#define PC_FRAME_RATE_BETA "High refresh is in beta. Gameplay issues may occur."
#define PC_FRAME_RATE_APPLY "Changes apply without restarting."
static int pc_frame_rate_value(int choice)
{ return choice==1?120:choice==2?144:0; }
static int pc_frame_rate_choice(int rate)
{ return rate==120?1:rate==144?2:0; }
static const char *pc_frame_rate_label(int choice)
{
    static const char *labels[]={"60 FPS (Original)","120 FPS (Beta)","144 FPS (Beta)"};
    return labels[choice>=0 && choice<PC_FRAME_RATE_COUNT?choice:0];
}
static int pc_frame_rate_path(char *out,size_t size)
{
    const char *source=getenv("TSFP_DISPLAY_CONFIG");
    if(!source || !*source)source=getenv("RECOMP_INPUT_CONFIG");
    if(!source || !*source)source="pc-display.ini";
    if(snprintf(out,size,"%s",source)<0 || strlen(source)>=size)return 0;
    char *a=strrchr(out,'/'),*b=strrchr(out,'\\');
    char *slash=a && b?(a>b?a:b):a?a:b;
    size_t prefix=slash?(size_t)(slash+1-out):0;
    const char *name="pc-frame-rate.ini";
    if(prefix+strlen(name)>=size)return 0;
    strcpy(out+prefix,name);return 1;
}
/* -1 means no valid saved preference: retain the existing launch override. */
static int pc_frame_rate_read(void)
{
    char path[32768],version[32],tail;int rate=-1;
    if(!pc_frame_rate_path(path,sizeof path))return -1;
    FILE *file=fopen(path,"rb");if(!file)return -1;
    if(fscanf(file,"%31s %d %c",version,&rate,&tail)!=2 ||
       strcmp(version,"TSFP_FRAME_RATE_V1") ||
       (rate!=0 && rate!=120 && rate!=144))rate=-1;
    fclose(file);return rate;
}
static int pc_frame_rate_save(int rate)
{
    char path[32768],temp[32768];
    if(rate!=0 && rate!=120 && rate!=144)return 0;
    if(!pc_frame_rate_path(path,sizeof path) || strlen(path)+5>=sizeof temp)return 0;
    snprintf(temp,sizeof temp,"%s.tmp",path);
    FILE *file=fopen(temp,"wb");if(!file)return 0;
    int ok=fprintf(file,"TSFP_FRAME_RATE_V1 %d\n",rate)>0;
    if(fflush(file))ok=0;
    if(fclose(file))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temp);
#else
    if(ok)ok=rename(temp,path)==0;
    if(!ok)remove(temp);
#endif
    return ok;
}
#endif
