#ifndef TSFP_PC_SMAA_SETTINGS_H
#define TSFP_PC_SMAA_SETTINGS_H
/* One model for the renderer and both frontend/pause video menus. */
int pc_smaa_enabled(void);
int pc_smaa_set_enabled(int enabled); /* Persist first, then publish atomically. */

#ifdef PC_SMAA_SETTINGS_IMPLEMENTATION
#include "recomp_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static volatile long pc_smaa_live=-1;
int pc_smaa_enabled(void)
{
#ifdef _WIN32
    long value=InterlockedCompareExchange(&pc_smaa_live,0,0);
#else
    long value=__atomic_load_n(&pc_smaa_live,__ATOMIC_ACQUIRE);
#endif
    return value>=0 ? value!=0 : recomp_config_bool("TSFP_SMAA","smaa",0);
}
static int pc_smaa_key(const char *line,const char *end)
{
    while(line<end && (*line==' ' || *line=='\t'))line++;
    if(end-line<4 || memcmp(line,"smaa",4))return 0;
    line+=4;
    while(line<end && (*line==' ' || *line=='\t'))line++;
    return line<end && *line=='=';
}
int pc_smaa_set_enabled(int enabled)
{
    char path[32768],temp[32768];struct stat info;
    if(enabled!=0 && enabled!=1)return 0;
    if(!recomp_settings_path(recomp_config_title_id(),path,sizeof path))return 0;
    int exists=stat(path,&info)==0;
    if(exists && (!(info.st_mode&0222) || info.st_size<0 || info.st_size>1024*1024))return 0;
    FILE *input=fopen(path,"rb");
    if(!input && (exists || errno!=ENOENT))return 0;
    char *text=malloc(1024*1024+1);
    if(!text){if(input)fclose(input);return 0;}
    size_t size=0;
    if(input) {
        size=fread(text,1,1024*1024+1,input);
        int bad=ferror(input) || size>1024*1024;
        fclose(input);if(bad){free(text);return 0;}
    }
    text[size]=0;
    /* Refuse binary input rather than silently truncating unrelated settings. */
    if(memchr(text,0,size)){free(text);return 0;}
#ifdef _WIN32
    unsigned long process=GetCurrentProcessId();
#else
    unsigned long process=(unsigned long)getpid();
#endif
    int length=snprintf(temp,sizeof temp,"%s.smaa.%lu.tmp",path,process);
    if(length<0 || (size_t)length>=sizeof temp){free(text);return 0;}
    FILE *output=fopen(temp,"wbx");
    if(!output){free(text);return 0;}
    int ok=1,found=0;
    const char *at=text,*limit=text+size;
    while(at<limit && ok) {
        const char *end=memchr(at,'\n',(size_t)(limit-at));
        if(end)end++;else end=limit;
        if(pc_smaa_key(at,end)) {
            ok=fprintf(output,"smaa = %d%s",enabled,
                end-at>=2 && end[-2]=='\r'?"\r\n":"\n")>0;
            found=1;
        } else ok=fwrite(at,1,(size_t)(end-at),output)==(size_t)(end-at);
        at=end;
    }
    if(ok && !found) {
        if(size && text[size-1]!='\n')ok=fputc('\n',output)!=EOF;
        if(ok)ok=fprintf(output,"smaa = %d\n",enabled)>0;
    }
    free(text);
    if(fflush(output))ok=0;
#ifndef _WIN32
    if(fsync(fileno(output)))ok=0;
#endif
    if(fclose(output))ok=0;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileA(temp);
    if(ok)InterlockedExchange(&pc_smaa_live,enabled);
#else
    if(ok)ok=rename(temp,path)==0;
    if(!ok)remove(temp);
    if(ok)__atomic_store_n(&pc_smaa_live,enabled,__ATOMIC_RELEASE);
#endif
    return ok;
}
#endif
#endif
