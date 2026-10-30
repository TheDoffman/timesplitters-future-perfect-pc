#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "kernel.h"
#include "guest_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"native storage line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
void xbox_log(int a,int b,const char *text,...) {(void)a;(void)b;(void)text;}
void xbox_watch_note_path(const char *p) {(void)p;}
uint32_t recomp_config_title_id(void) {return 0x45410066;}
typedef struct {uint64_t bytes,allocated,modified;} info;
static void path(char *out,const char *root,const char *leaf) {int n=snprintf(out,4096,"%s/%s",root,leaf);CHECK(n>0 && n<4096);}
#ifdef _WIN32
static HANDLE open_file(const char *name,int create) {
    WCHAR native[MAX_PATH];HANDLE file;CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,native,MAX_PATH));
    file=CreateFileW(native,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,create?CREATE_NEW:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(file!=INVALID_HANDLE_VALUE);return file;
}
static info metadata(const char *name) {
    HANDLE file=open_file(name,0);LARGE_INTEGER size;FILETIME modified;info out;WCHAR native[MAX_PATH];DWORD high,low;
    CHECK(GetFileSizeEx(file,&size) && GetFileTime(file,NULL,NULL,&modified));out.bytes=(uint64_t)size.QuadPart;
    out.modified=((uint64_t)modified.dwHighDateTime<<32)|modified.dwLowDateTime;CHECK(CloseHandle(file));
    CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,native,MAX_PATH));SetLastError(NO_ERROR);low=GetCompressedFileSizeW(native,&high);
    CHECK(low!=INVALID_FILE_SIZE || GetLastError()==NO_ERROR);out.allocated=((uint64_t)high<<32)|low;return out;
}
static void modify(const char *name,uint64_t size,uint64_t offset,const void *data,size_t bytes,int create) {
    HANDLE file=open_file(name,create);LARGE_INTEGER pos;DWORD written;
    if(size!=UINT64_MAX) {pos.QuadPart=(LONGLONG)size;CHECK(SetFilePointerEx(file,pos,NULL,FILE_BEGIN) && SetEndOfFile(file));}
    pos.QuadPart=(LONGLONG)offset;CHECK(SetFilePointerEx(file,pos,NULL,FILE_BEGIN) && WriteFile(file,data,(DWORD)bytes,&written,NULL) && written==bytes && CloseHandle(file));
}
static void read_bytes(const char *name,uint64_t offset,void *data,size_t bytes) {
    HANDLE file=open_file(name,0);LARGE_INTEGER pos;DWORD got;pos.QuadPart=(LONGLONG)offset;
    CHECK(SetFilePointerEx(file,pos,NULL,FILE_BEGIN) && ReadFile(file,data,(DWORD)bytes,&got,NULL) && got==bytes && CloseHandle(file));
}
#else
static info metadata(const char *name) {
    struct stat st;info out;CHECK(!stat(name,&st) && S_ISREG(st.st_mode));out.bytes=(uint64_t)st.st_size;out.allocated=(uint64_t)st.st_blocks*512;
#ifdef __APPLE__
    out.modified=(uint64_t)st.st_mtimespec.tv_sec*UINT64_C(1000000000)+(uint64_t)st.st_mtimespec.tv_nsec;
#else
    out.modified=(uint64_t)st.st_mtim.tv_sec*UINT64_C(1000000000)+(uint64_t)st.st_mtim.tv_nsec;
#endif
    return out;
}
static void modify(const char *name,uint64_t size,uint64_t offset,const void *data,size_t bytes,int create) {
    int fd=open(name,O_RDWR|(create?(O_CREAT|O_EXCL):0),0600);CHECK(fd>=0);
    if(size!=UINT64_MAX)CHECK(!ftruncate(fd,(off_t)size));
    CHECK(pwrite(fd,data,bytes,(off_t)offset)==(ssize_t)bytes && !close(fd));
}
static void read_bytes(const char *name,uint64_t offset,void *data,size_t bytes) {
    int fd=open(name,O_RDONLY);CHECK(fd>=0 && pread(fd,data,bytes,(off_t)offset)==(ssize_t)bytes && !close(fd));
}
#endif
static void expect(const char *guest,const char *root,const char *relative) {
    char wanted[4096],actual[4096];xbox_host_char host[MAX_PATH];size_t i;path(wanted,root,relative);
    CHECK(xbox_translate_path(guest,host,MAX_PATH));
#ifdef _WIN32
    CHECK(WideCharToMultiByte(CP_UTF8,0,host,-1,actual,sizeof(actual),NULL,NULL));
#else
    memcpy(actual,host,strlen(host)+1);
#endif
    for(i=0;wanted[i];++i)if(wanted[i]=='\\')wanted[i]='/';
    for(i=0;actual[i];++i)if(actual[i]=='\\')actual[i]='/';
    CHECK(!strcmp(actual,wanted));
}
int main(int argc,char **argv) {
    char game[4096],save[4096],other[4096],environment[4096],profile[4096],image0[4096],image1[4096],bad[4096],out[4096],before[4096];
    unsigned char bytes[512];info first0,first1,after;unsigned i;uint64_t expected;
    CHECK(argc==2);path(game,argv[1],"game");path(save,argv[1],"save");path(other,argv[1],"other");path(environment,argv[1],"environment");path(profile,argv[1],"profile");path(bad,argv[1],"not-a-directory");
    memset(out,0x77,sizeof(out));memcpy(before,out,sizeof(out));CHECK(!xbox_save_root_utf8(out,sizeof(out)) && !memcmp(out,before,sizeof(out)));
    CHECK(xbox_set_profile_root_utf8(profile));xbox_path_init(game,save);CHECK(xbox_save_root_utf8(out,sizeof(out)) && !strcmp(out,save));
    expect("U:\\a",profile,"UserData/a");CHECK(xbox_set_profile_root_utf8(""));expect("U:\\a",save,"UserData/a");
    path(image0,save,"Partition0.img");read_bytes(image0,0x800,bytes,sizeof(bytes));CHECK(!memcmp(bytes,"****PARTINFO****",16));
    for(i=0;i<6;++i) {char leaf[32],image[4096];snprintf(leaf,sizeof(leaf),"Partition%u.img",i);path(image,save,leaf);after=metadata(image);CHECK(tsfp_storage_image_bytes(i,&expected) && after.bytes==expected && after.allocated<1048576);}
    path(image1,save,"Partition1.img");modify(image1,7,0,"persist",7,0);modify(image0,UINT64_MAX,0x800,"V",1,0);first0=metadata(image0);first1=metadata(image1);
    xbox_path_init(game,save);after=metadata(image0);CHECK(after.bytes==first0.bytes && after.modified==first0.modified);read_bytes(image0,0x800,bytes,1);CHECK(bytes[0]=='V');
    after=metadata(image1);CHECK(after.bytes==7 && after.modified==first1.modified);read_bytes(image1,0,bytes,7);CHECK(!memcmp(bytes,"persist",7));
    CHECK(xbox_set_profile_root_utf8(profile));xbox_path_init(game,other);expect("U:\\a",profile,"UserData/a");expect("T:\\a",other,"TitleData/a");
    CHECK(!xbox_set_profile_root_utf8("relative-missing"));expect("U:\\a",profile,"UserData/a");CHECK(xbox_set_profile_root_utf8(""));
    modify(bad,1,0,"x",1,1);xbox_path_init(game,bad);CHECK(xbox_save_root_utf8(out,sizeof(out)) && !strcmp(out,other));
    memset(out,0x66,sizeof(out));memcpy(before,out,sizeof(out));CHECK(!xbox_save_root_utf8(out,2) && !memcmp(out,before,sizeof(out)));
#ifdef _WIN32
    CHECK(!_putenv_s("RECOMP_SAVE_DIR",environment));
#else
    CHECK(!setenv("RECOMP_SAVE_DIR",environment,1));
#endif
    xbox_path_init(game,NULL);CHECK(xbox_save_root_utf8(out,sizeof(out)) && !strcmp(out,environment));
    xbox_path_init(game,save);CHECK(xbox_save_root_utf8(out,sizeof(out)) && !strcmp(out,save));
    after=metadata(image1);CHECK(after.bytes==7 && after.modified==first1.modified);
    puts("PASS: actual native storage, six sparse images, exact existing bytes/size/mtime, selected profiles, environment precedence and failed-root rollback");return 0;
}
