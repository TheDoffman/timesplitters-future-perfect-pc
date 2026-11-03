#ifndef TSFP_SHADER_CACHE_CLEAR_H
#define TSFP_SHADER_CACHE_CLEAR_H
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int tsfp_cache_blob_name(const char *name)
{
    if(!strcmp(name,"pipelines.bin"))return 1;
    if(strlen(name)!=20 || strcmp(name+16,".bin"))return 0;
    for(int i=0;i<16;i++)if(!isxdigit((unsigned char)name[i]))return 0;
    return 1;
}
/* Only one backend-directory level, only recognized blobs, no links, no
 * recursive deletion. Root is the title-specific cache, never a save directory.
 * Return removed count or -1 if any operation failed. */
static int tsfp_cache_clear_level(const char *root,int backend)
{
    int removed=0,failed=0;
    char path[2048];
#ifdef _WIN32
    WIN32_FIND_DATAA data;HANDLE scan;
    if(GetFileAttributesA(root)==INVALID_FILE_ATTRIBUTES)
        return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND?0:-1;
    if(GetFileAttributesA(root)&FILE_ATTRIBUTE_REPARSE_POINT)return -1;
    if(snprintf(path,sizeof path,"%s/*",root)<0 || strlen(root)+3>=sizeof path)return -1;
    scan=FindFirstFileA(path,&data);
    if(scan==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?0:-1;
    do {
        int directory=(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        if(data.cFileName[0]=='.' || (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
        if((backend && directory) || (!backend && !directory))continue;
        if(backend && !tsfp_cache_blob_name(data.cFileName))continue;
        int n=snprintf(path,sizeof path,"%s/%s",root,data.cFileName);
        if(n<0 || (size_t)n>=sizeof path){failed=1;continue;}
        if(!backend){int nremoved=tsfp_cache_clear_level(path,1);if(nremoved<0)failed=1;else removed+=nremoved;}
        else if(DeleteFileA(path))removed++;else failed=1;
    } while(FindNextFileA(scan,&data));
    if(GetLastError()!=ERROR_NO_MORE_FILES)failed=1;
    FindClose(scan);
#else
    struct stat root_stat;
    if(lstat(root,&root_stat))return errno==ENOENT?0:-1;
    if(!S_ISDIR(root_stat.st_mode))return -1;
    DIR *scan=opendir(root);struct dirent *entry;
    if(!scan)return -1;
    for(;;) {
        struct stat st;int n;
        errno=0;entry=readdir(scan);if(!entry){if(errno)failed=1;break;}
        if(entry->d_name[0]=='.')continue;
        if(backend && !tsfp_cache_blob_name(entry->d_name))continue;
        n=snprintf(path,sizeof path,"%s/%s",root,entry->d_name);
        if(n<0 || (size_t)n>=sizeof path){failed=1;continue;}
        if(lstat(path,&st)){failed=1;continue;}
        if(!backend && S_ISDIR(st.st_mode)) {
            int nremoved=tsfp_cache_clear_level(path,1);if(nremoved<0)failed=1;else removed+=nremoved;
        } else if(backend && S_ISREG(st.st_mode)) {
            if(unlink(path)==0)removed++;else failed=1;
        }
    }
    closedir(scan);
#endif
    return failed?-1:removed;
}
#endif
