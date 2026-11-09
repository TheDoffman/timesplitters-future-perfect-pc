/* Native file ownership with injected filesystem capability/error responses.
 * Resize records production geometry but caps disposable files at 4 KiB. */
#include "kernel.h"
#include "guest_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"fallback line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static DWORD sparse_error;static int low_space,query_error,size_error;
static unsigned sparse_calls,space_calls,size_calls;
static uint64_t requested[6];
void xbox_log(int a,int b,const char *s,...) {(void)a;(void)b;(void)s;}
void xbox_watch_note_path(const char *s) {(void)s;}
uint32_t recomp_config_title_id(void) {return 0x45410066;}
static BOOL WINAPI test_sparse(HANDLE h,DWORD code,LPVOID in,DWORD ins,LPVOID out,DWORD outs,LPDWORD n,LPOVERLAPPED overlap) {
    (void)h;(void)code;(void)in;(void)ins;(void)out;(void)outs;(void)n;(void)overlap;
    ++sparse_calls;SetLastError(sparse_error);return FALSE;
}
static BOOL WINAPI test_space(LPCWSTR root,PULARGE_INTEGER available,PULARGE_INTEGER total,PULARGE_INTEGER free_bytes) {
    (void)total;(void)free_bytes;CHECK(root && root[0]);++space_calls;
    if(query_error){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    available->QuadPart=low_space?0:UINT64_C(10000000000);return TRUE;
}
static BOOL WINAPI test_size(HANDLE h) {
    LARGE_INTEGER zero,end;zero.QuadPart=0;CHECK(SetFilePointerEx(h,zero,&end,FILE_CURRENT));
    CHECK(size_calls<6);requested[size_calls++]=(uint64_t)end.QuadPart;
    if(size_error && end.QuadPart>1048576){SetLastError(ERROR_DISK_FULL);return FALSE;}
    if(end.QuadPart>4096){end.QuadPart=4096;CHECK(SetFilePointerEx(h,end,NULL,FILE_BEGIN));}
    return SetEndOfFile(h);
}
#define DeviceIoControl test_sparse
#define GetDiskFreeSpaceExW test_space
#define SetEndOfFile test_size
#include "guest_kernel_storage.c"
#undef DeviceIoControl
#undef GetDiskFreeSpaceExW
#undef SetEndOfFile
int main(int argc,char **argv) {
    char game[4096],save[4096],out[4096],path[4096];unsigned i;int success;
    CHECK(argc==3);sparse_error=(DWORD)strtoul(argv[2],NULL,10);
    if(sparse_error==112){sparse_error=ERROR_NOT_SUPPORTED;low_space=1;}
    if(sparse_error==999){sparse_error=ERROR_NOT_SUPPORTED;query_error=1;}
    if(sparse_error==998){sparse_error=ERROR_NOT_SUPPORTED;size_error=1;}
    success=(sparse_error==ERROR_INVALID_FUNCTION || sparse_error==ERROR_NOT_SUPPORTED) && !low_space && !query_error && !size_error;
    CHECK(snprintf(game,sizeof(game),"%s/game",argv[1])>0);CHECK(snprintf(save,sizeof(save),"%s/save",argv[1])>0);
    xbox_path_init(game,save);CHECK(!!xbox_save_root_utf8(out,sizeof(out))==success);
    CHECK(sparse_calls==5);CHECK(space_calls==(sparse_error==ERROR_ACCESS_DENIED?0u:5u));
    for(i=0;i<6;++i) {
        HANDLE file;DWORD n;char value='K';
        CHECK(snprintf(path,sizeof(path),"%s/Partition%u.img",save,i)>0);
        CHECK((GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES)==(!i || success));
        if(success){uint64_t size;CHECK(tsfp_storage_image_bytes(i,&size) && requested[i]==size);}
        /* Seed preexisting files for the second init; these must bypass every
         * allocation/capability operation, preserving their exact bytes/size. */
        file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(file!=INVALID_HANDLE_VALUE);
        CHECK(WriteFile(file,&value,1,&n,NULL) && n==1 && CloseHandle(file));
    }
    sparse_calls=space_calls=size_calls=0;
    xbox_path_init(game,save);CHECK(xbox_save_root_utf8(out,sizeof(out)) && !strcmp(out,save));
    CHECK(!sparse_calls && !space_calls && !size_calls);
    for(i=0;i<6;++i){HANDLE file;DWORD n;char value=0;LARGE_INTEGER size;
        CHECK(snprintf(path,sizeof(path),"%s/Partition%u.img",save,i)>0);
        file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(file!=INVALID_HANDLE_VALUE);
        CHECK(GetFileSizeEx(file,&size) && size.QuadPart==1 && ReadFile(file,&value,1,&n,NULL) && n==1 && value=='K' && CloseHandle(file));
    }
    puts("PASS: filesystem fallback, geometry, failures and existing-file preservation");return 0;
}
