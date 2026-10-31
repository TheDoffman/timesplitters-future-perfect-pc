/* Original native storage adapter. Existing kernel.h supplies transitional
 * public ABI types; no legacy path implementation is compiled into this unit. */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "kernel.h"
#include "xbox_watchpoint.h"
#include "recomp_config.h"
#include "guest_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <shlobj.h>
#include <winioctl.h>
static SRWLOCK initialization=SRWLOCK_INIT;
static void init_enter(void) {AcquireSRWLockExclusive(&initialization);}
static void init_leave(void) {ReleaseSRWLockExclusive(&initialization);}
static int wide(const char *s,WCHAR *out) {return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out,MAX_PATH)>0;}
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
static pthread_mutex_t initialization=PTHREAD_MUTEX_INITIALIZER;
static void init_enter(void) {if(pthread_mutex_lock(&initialization))abort();}
static void init_leave(void) {if(pthread_mutex_unlock(&initialization))abort();}
#endif
static tsfp_storage_state storage;
static int bridge_path_snapshot(tsfp_storage_snapshot *out) {return tsfp_storage_read(&storage,out);}
static size_t host_length(const char *s) {size_t n=0;if(!s)return TSFP_PATH_LIMIT;while(n<TSFP_PATH_LIMIT && s[n])++n;return n;}
static int host_directory(const char *path) {
#ifdef _WIN32
    WCHAR name[MAX_PATH];DWORD attributes;if(!wide(path,name))return 0;
    attributes=GetFileAttributesW(name);return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
#else
    struct stat info;return !stat(path,&info) && S_ISDIR(info.st_mode);
#endif
}
static int host_mkdir(const char *path) {
#ifdef _WIN32
    WCHAR name[MAX_PATH];int result;if(!wide(path,name))return 0;
    result=SHCreateDirectoryExW(NULL,name,NULL);
    return (result==ERROR_SUCCESS || result==ERROR_ALREADY_EXISTS || result==ERROR_FILE_EXISTS) && host_directory(path);
#else
    char name[TSFP_PATH_LIMIT];size_t n=host_length(path),i;
    if(!n || n>=sizeof(name))return 0;
    memcpy(name,path,n+1);
    for(i=1;i<=n;++i)if(name[i]=='/' || !name[i]) {
        char saved=name[i];name[i]=0;
        if(name[0] && mkdir(name,0755) && (errno!=EEXIST || !host_directory(name)))return 0;
        name[i]=saved;
    }
    return host_directory(path);
#endif
}
#ifndef _WIN32
static void mkdir_p(const char *path) {(void)host_mkdir(path);}
#endif
typedef struct image_token {
    struct image_token *next;char path[TSFP_PATH_LIMIT];
#ifdef _WIN32
    HANDLE file;
#else
    int file;
#endif
} image_token;
/* Owned native resources/failed new-file cleanup retained on uncertainty. No
 * automatic close/delete retry, and no borrowed initialization-stack context. */
static image_token *quarantined;
static int image_create(void *context,unsigned part,void **out) {
    const char *root=context;image_token *token;char leaf[32];size_t bytes;
    token=calloc(1,sizeof(*token));if(!token)return 0;
    if(snprintf(leaf,sizeof(leaf),"Partition%u.img",part)<0 ||
       !tsfp_path_join(root,host_length(root),leaf,strlen(leaf),'/',token->path,sizeof(token->path),&bytes)) {free(token);return 0;}
#ifdef _WIN32
    {WCHAR name[MAX_PATH];DWORD error;
    if(!wide(token->path,name)) {free(token);return 0;}
    token->file=CreateFileW(name,GENERIC_READ|GENERIC_WRITE|DELETE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(token->file==INVALID_HANDLE_VALUE) {error=GetLastError();free(token);return error==ERROR_FILE_EXISTS || error==ERROR_ALREADY_EXISTS?TSFP_STORAGE_EXISTS:0;}}
#else
    token->file=open(token->path,O_CREAT|O_EXCL|O_RDWR|O_NOFOLLOW,0600);
    if(token->file<0) {int error=errno;free(token);return error==EEXIST?TSFP_STORAGE_EXISTS:0;}
#endif
    *out=token;return TSFP_STORAGE_NEW;
}
static int image_resize(void *context,void *data,uint64_t bytes) {
    image_token *token=data;(void)context;
#ifdef _WIN32
    LARGE_INTEGER end;DWORD returned;
    if(bytes>UINT64_C(1048576) && !DeviceIoControl(token->file,FSCTL_SET_SPARSE,NULL,0,NULL,0,&returned,NULL))return 0;
    end.QuadPart=(LONGLONG)bytes;return SetFilePointerEx(token->file,end,NULL,FILE_BEGIN) && SetEndOfFile(token->file);
#else
    return ftruncate(token->file,(off_t)bytes)==0;
#endif
}
static int image_write(void *context,void *data,uint64_t offset,const void *bytes,size_t count) {
    image_token *token=data;(void)context;
#ifdef _WIN32
    LARGE_INTEGER position;DWORD written;position.QuadPart=(LONGLONG)offset;
    return count<=UINT32_MAX && SetFilePointerEx(token->file,position,NULL,FILE_BEGIN) &&
        WriteFile(token->file,bytes,(DWORD)count,&written,NULL) && written==count;
#else
    return pwrite(token->file,bytes,count,(off_t)offset)==(ssize_t)count;
#endif
}
static int image_close(void *context,void *data,int commit) {
    image_token *token=data;int clean=1;(void)context;
#ifdef _WIN32
    if(!commit) {FILE_DISPOSITION_INFO disposition;disposition.DeleteFile=TRUE;
        if(!SetFileInformationByHandle(token->file,FileDispositionInfo,&disposition,sizeof(disposition)))clean=0;}
    if(CloseHandle(token->file))token->file=INVALID_HANDLE_VALUE;else clean=0;
#else
    if(!commit) {
        struct stat owned,named;
        /* Initialization owns this new name; never remove a replaced object. */
        if(fstat(token->file,&owned) || lstat(token->path,&named) || owned.st_dev!=named.st_dev || owned.st_ino!=named.st_ino || unlink(token->path))clean=0;
    }
    if(close(token->file))clean=0;else token->file=-1;
#endif
    if(clean)free(token);else {token->next=quarantined;quarantined=token;}
    return clean;
}
static int default_game(char *out) {
#ifdef _WIN32
    WCHAR cwd[MAX_PATH];DWORD n=GetCurrentDirectoryW(MAX_PATH,cwd);
    return n && n<MAX_PATH && WideCharToMultiByte(CP_UTF8,0,cwd,-1,out,TSFP_PATH_LIMIT,NULL,NULL)>0;
#else
    char cwd[TSFP_PATH_LIMIT];size_t n;
    return getcwd(cwd,sizeof(cwd)) && tsfp_path_join(cwd,strlen(cwd),"game",4,'/',out,TSFP_PATH_LIMIT,&n);
#endif
}
static int default_save(char *out) {
    char root[TSFP_PATH_LIMIT];size_t n;
#ifdef _WIN32
    WCHAR native[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,native)) &&
       WideCharToMultiByte(CP_UTF8,0,native,-1,root,sizeof(root),NULL,NULL)>0)
        return tsfp_path_join(root,strlen(root),"xboxrecomp",10,'/',out,TSFP_PATH_LIMIT,&n);
    if(!default_game(root))return 0;
    return tsfp_path_join(root,strlen(root),"SaveData",8,'/',out,TSFP_PATH_LIMIT,&n);
#else
    const char *xdg=getenv("XDG_DATA_HOME"),*home=getenv("HOME");
    if(xdg && xdg[0])return tsfp_path_join(xdg,host_length(xdg),"xboxrecomp",10,'/',out,TSFP_PATH_LIMIT,&n);
    if(!home || !home[0])home=".";
    if(!tsfp_path_join(home,host_length(home),".local/share",12,'/',root,sizeof(root),&n))return 0;
    return tsfp_path_join(root,n,"xboxrecomp",10,'/',out,TSFP_PATH_LIMIT,&n);
#endif
}
void xbox_path_init(const char *game,const char *save) {
    tsfp_storage_snapshot paths;tsfp_storage_report report;unsigned source=TSFP_STORAGE_DEFAULT;
    char game_default[TSFP_PATH_LIMIT],save_default[TSFP_PATH_LIMIT],directory[TSFP_PATH_LIMIT];size_t bytes;
    const char *environment=getenv("RECOMP_SAVE_DIR");
    static const char *const folders[]={"TitleData","UserData","Cache","SystemData"};unsigned i;int ok=1;
    init_enter();
    if(!game && !default_game(game_default))ok=0;
    if(!save && (!environment || !environment[0]) && !default_save(save_default))ok=0;
    if(ok)ok=tsfp_storage_select(game,save,environment,game_default,save_default,&paths,&source);
#ifdef _WIN32
    if(ok) {WCHAR check[MAX_PATH];ok=wide(paths.game,check) && wide(paths.save,check);}
#endif
    if(ok)ok=host_mkdir(paths.save);
    for(i=0;ok && i<4;++i)ok=tsfp_path_join(paths.save,strlen(paths.save),folders[i],strlen(folders[i]),'/',directory,sizeof(directory),&bytes) && host_mkdir(directory);
    if(ok) {
        tsfp_storage_ops ops={paths.save,image_create,image_resize,image_write,image_close};
        ok=tsfp_storage_prepare(&ops,&report);
    }
    if(ok)ok=tsfp_storage_commit(&storage,&paths);
    if(ok) {fprintf(stderr,"[PATH] saves in %s (%s)\n",paths.save,source==TSFP_STORAGE_CALLER?"caller":source==TSFP_STORAGE_ENVIRONMENT?"RECOMP_SAVE_DIR":"default");fflush(stderr);}
    else xbox_log(XBOX_LOG_ERROR,XBOX_LOG_PATH,"Storage initialization failed; previous roots preserved");
    init_leave();
}
int xbox_set_profile_root_utf8(const char *root) {
    size_t n=host_length(root);if(n>=TSFP_PATH_LIMIT)return 0;
    if(n) {
#ifdef _WIN32
        WCHAR name[MAX_PATH];if(!wide(root,name) ||
            !((name[0] && name[1]==L':' && (name[2]==L'/' || name[2]==L'\\')) || (name[0]==L'\\' && name[1]==L'\\')))return 0;
#else
        if(root[0]!='/')return 0;
#endif
        if(!host_directory(root))return 0;
    }
    return tsfp_storage_profile(&storage,root);
}
int xbox_save_root_utf8(char *out,unsigned capacity) {
    tsfp_storage_snapshot snapshot;size_t n;
    if(!out || !bridge_path_snapshot(&snapshot))return 0;n=strlen(snapshot.save);
    if(n>=capacity)return 0;memcpy(out,snapshot.save,n+1);return 1;
}
#include "guest_kernel_paths.inc"
