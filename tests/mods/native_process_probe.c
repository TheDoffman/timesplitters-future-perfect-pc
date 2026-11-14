/* Trusted native escape/resource canary; built only with TSFP_MOD_TESTING. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mod_seal.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>
#ifdef __APPLE__
#include <mach/mach.h>
#else
#include <sys/ptrace.h>
#endif
#endif
/* Strict handle checking can raise STATUS_INVALID_HANDLE instead of returning
   ERROR_INVALID_HANDLE. Catch only that denial in this test-only canary. */
#ifdef _WIN32
static int inherited_file_readable(unsigned long long value) {
    int readable=0;
#ifdef _MSC_VER
    __try {
#endif
        HANDLE handle=(HANDLE)(uintptr_t)value;DWORD flags=0;
        if(GetHandleInformation(handle,&flags) && GetFileType(handle)==FILE_TYPE_DISK) {
            char data[16];DWORD count=0;readable=ReadFile(handle,data,sizeof(data),&count,NULL) && count;
        }
#ifdef _MSC_VER
    } __except(GetExceptionCode()==0xc0000008u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {}
#endif
    return readable;
}
#endif
static void line(char *text,size_t size){if(!fgets(text,(int)size,stdin))text[0]=0;text[strcspn(text,"\r\n")]=0;}
int main(void) {
    char mode[32],path[2048],numbers[128],registry[512],write_path[2100];
    int read_ok=0,write_ok=0,network_ok=0,spawn_ok=0,parent_ok=0,descriptor_ok=0,registry_ok=0,acl_write=0;
    unsigned long parent=0,port=0;unsigned long long hidden=0;
    setvbuf(stdin,NULL,_IONBF,0);setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    line(mode,sizeof(mode));line(path,sizeof(path));line(numbers,sizeof(numbers));line(registry,sizeof(registry));
    sscanf(numbers,"%lu %lu %llu",&parent,&port,&hidden);
    if(strcmp(mode,"baseline") && !tsfp_mod_seal()){puts("{\"sealed\":false}");return 126;}
    if(!strcmp(mode,"hang")){for(;;){}}
    if(!strcmp(mode,"flood")){char data[4096];memset(data,'x',sizeof(data));for(;;)fwrite(data,1,sizeof(data),stdout);}
    if(!strcmp(mode,"diagnostic")){char data[4096];memset(data,'x',sizeof(data));for(;;)fwrite(data,1,sizeof(data),stderr);}
    if(!strcmp(mode,"memory")) {
        size_t total=0;while(total<1024u*1024*1024){volatile unsigned char *block=malloc(4u*1024*1024);if(!block)break;
            for(size_t at=0;at<4u*1024*1024;at+=4096)block[at]=0x4d;total+=4u*1024*1024;}
        printf("{\"allocated\":%llu}\n",(unsigned long long)total);return 0;
    }
    FILE *file=fopen(path,"rb");if(file){char data[32];read_ok=fread(data,1,sizeof(data),file)>0;fclose(file);}
    snprintf(write_path,sizeof(write_path),"%s.blocked",path);file=fopen(write_path,"wb");if(file){write_ok=1;fputs("escape",file);fclose(file);}
#ifdef _WIN32
    WSADATA ws;if(WSAStartup(MAKEWORD(2,2),&ws)==0){SOCKET sock=socket(AF_INET,SOCK_STREAM,0);
        if(sock!=INVALID_SOCKET){struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_port=htons((unsigned short)port);address.sin_addr.s_addr=htonl(0x7f000001);
            network_ok=connect(sock,(struct sockaddr *)&address,sizeof(address))==0;closesocket(sock);}WSACleanup();}
    HANDLE process=OpenProcess(PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_CREATE_THREAD,FALSE,(DWORD)parent);if(process){parent_ok=1;CloseHandle(process);}
    descriptor_ok=inherited_file_readable(hidden);
    HKEY key;if(RegOpenKeyExA(HKEY_CURRENT_USER,registry,0,KEY_READ,&key)==ERROR_SUCCESS){registry_ok=1;RegCloseKey(key);}
    wchar_t self[32768];DWORD needed=0;if(GetModuleFileNameW(NULL,self,32768)) {
        GetFileSecurityW(self,DACL_SECURITY_INFORMATION,NULL,0,&needed);
        void *descriptor=needed && needed<65536?malloc(needed):NULL;
        if(descriptor){if(GetFileSecurityW(self,DACL_SECURITY_INFORMATION,descriptor,needed,&needed))acl_write=SetFileSecurityW(self,DACL_SECURITY_INFORMATION,descriptor)!=FALSE;free(descriptor);}
    }
    wchar_t command[]=L"C:\\Windows\\System32\\cmd.exe /c exit 0";STARTUPINFOW startup;PROCESS_INFORMATION info;
    memset(&startup,0,sizeof(startup));startup.cb=sizeof(startup);memset(&info,0,sizeof(info));
    if(CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&info)){spawn_ok=1;WaitForSingleObject(info.hProcess,2000);CloseHandle(info.hThread);CloseHandle(info.hProcess);}
#else
    int sock=socket(AF_INET,SOCK_STREAM,0);if(sock>=0){struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_port=htons((unsigned short)port);address.sin_addr.s_addr=htonl(0x7f000001);
        network_ok=connect(sock,(struct sockaddr *)&address,sizeof(address))==0;close(sock);}
    descriptor_ok=fcntl((int)hidden,F_GETFD)>=0;
    pid_t child=fork();if(child==0)_exit(0);if(child>0){spawn_ok=1;waitpid(child,NULL,0);}
#ifdef __APPLE__
    mach_port_t task=MACH_PORT_NULL;if(task_for_pid(mach_task_self(),(int)parent,&task)==KERN_SUCCESS){parent_ok=1;mach_port_deallocate(mach_task_self(),task);}
#else
    parent_ok=ptrace(PTRACE_TRACEME,0,NULL,NULL)==0;
#endif
#endif
    printf("{\"sealed\":%s,\"read\":%d,\"write\":%d,\"network\":%d,\"spawn\":%d,\"parent\":%d,\"descriptor\":%d,\"registry\":%d,\"acl_write\":%d,\"secret\":%d}\n",
        strcmp(mode,"baseline")?"true":"false",read_ok,write_ok,network_ok,spawn_ok,parent_ok,descriptor_ok,registry_ok,acl_write,getenv("TSFP_SUPERVISOR_SECRET")!=NULL);
    return 0;
}
