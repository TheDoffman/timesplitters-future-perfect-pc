/* Targeted atomic runtime.conf edits preserve unknown keys and comments. */
#ifndef TSFP_PC_RUNTIME_SETTINGS_H
#define TSFP_PC_RUNTIME_SETTINGS_H
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
static int pc_runtime_bool_key(const char *line,const char *end,const char *key)
{
    while(line<end && (*line==' ' || *line=='\t'))line++;
    size_t length=strlen(key);
    if((size_t)(end-line)<length || memcmp(line,key,length))return 0;
    line+=length;
    while(line<end && (*line==' ' || *line=='\t'))line++;
    return line<end && *line=='=';
}
static int pc_runtime_bool_save(const char *key,int enabled)
{
    char path[32768],temp[32768];struct stat info;
    if((enabled!=0 && enabled!=1) || !key || !*key || strlen(key)>63)return 0;
    for(const char *k=key;*k;k++)if(!((*k>='a' && *k<='z') || *k=='_'))return 0;
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
    int length=snprintf(temp,sizeof temp,"%s.%s.%lu.tmp",path,key,process);
    if(length<0 || (size_t)length>=sizeof temp){free(text);return 0;}
    FILE *output=fopen(temp,"wbx");
    if(!output){free(text);return 0;}
    int ok=1,found=0;
    const char *at=text,*limit=text+size;
    while(at<limit && ok) {
        const char *end=memchr(at,'\n',(size_t)(limit-at));
        if(end)end++;else end=limit;
        if(pc_runtime_bool_key(at,end,key)) {
            ok=fprintf(output,"%s = %d%s",key,enabled,
                end-at>=2 && end[-2]=='\r'?"\r\n":"\n")>0;
            found=1;
        } else ok=fwrite(at,1,(size_t)(end-at),output)==(size_t)(end-at);
        at=end;
    }
    if(ok && !found) {
        if(size && text[size-1]!='\n')ok=fputc('\n',output)!=EOF;
        if(ok)ok=fprintf(output,"%s = %d\n",key,enabled)>0;
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
#else
    if(ok)ok=rename(temp,path)==0;
    if(!ok)remove(temp);
#endif
    return ok;
}
#endif
