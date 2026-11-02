/* Host restart handoff. The replacement waits for this process to finish before
 * opening settings, audio or saves. It inherits the exact environment and cwd. */
#ifdef _WIN32
#include <wchar.h>
int pc_game_restart_bootstrap(int argc,char **argv)
{
    (void)argc;(void)argv;
    wchar_t value[64];DWORD n=GetEnvironmentVariableW(L"TSFP_RESTART_WAIT",value,64);
    if(!n)return 1;
    if(n>=64)return 0;
    wchar_t *end;unsigned long long number=wcstoull(value,&end,16);
    _putenv_s("TSFP_RESTART_WAIT","");
    HANDLE parent=(HANDLE)(uintptr_t)number;
    if(*end || !number || !GetProcessId(parent) || GetProcessId(parent)==GetCurrentProcessId())return 0;
    DWORD result=WaitForSingleObject(parent,60000);CloseHandle(parent);
    return result==WAIT_OBJECT_0;
}
int pc_game_restart(void)
{
    static int requested;
    if(requested)return 1;
    HWND window=xbox_D3D8Window();if(!window)return 0;
    HANDLE parent=NULL;
    if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&parent,
                        SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,TRUE,0))return 0;
    wchar_t exe[32768],command[32768],marker[96];
    DWORD length=GetModuleFileNameW(NULL,exe,32768);
    if(!length || length>=32768){CloseHandle(parent);return 0;}
    const wchar_t *original=GetCommandLineW();
    if(wcslen(original)>=32768){CloseHandle(parent);return 0;}
    wcscpy(command,original);
    swprintf(marker,96,L"TSFP_RESTART_WAIT=%llx",(unsigned long long)(uintptr_t)parent);
    wchar_t *old=GetEnvironmentStringsW();if(!old){CloseHandle(parent);return 0;}
    size_t count=0;for(const wchar_t *p=old;*p;p+=wcslen(p)+1)count+=wcslen(p)+1;
    wchar_t *env=calloc(count+wcslen(marker)+3,sizeof(wchar_t));
    if(!env){FreeEnvironmentStringsW(old);CloseHandle(parent);return 0;}
    size_t used=0;for(const wchar_t *p=old;*p;p+=wcslen(p)+1) {
        if(!_wcsnicmp(p,L"TSFP_RESTART_WAIT=",18))continue;
        size_t n=wcslen(p)+1;memcpy(env+used,p,n*sizeof(wchar_t));used+=n;
    }
    wcscpy(env+used,marker);FreeEnvironmentStringsW(old);
    SIZE_T bytes=0;InitializeProcThreadAttributeList(NULL,1,0,&bytes);
    STARTUPINFOEXW start={0};start.StartupInfo.cb=sizeof start;
    start.lpAttributeList=malloc(bytes);
    int ready=start.lpAttributeList && InitializeProcThreadAttributeList(start.lpAttributeList,1,0,&bytes);
    PROCESS_INFORMATION process={0};BOOL spawned=FALSE;
    if(ready && UpdateProcThreadAttribute(start.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                          &parent,sizeof parent,NULL,NULL))
        spawned=CreateProcessW(exe,command,NULL,NULL,TRUE,
            EXTENDED_STARTUPINFO_PRESENT|CREATE_UNICODE_ENVIRONMENT,env,NULL,&start.StartupInfo,&process);
    if(ready)DeleteProcThreadAttributeList(start.lpAttributeList);
    free(start.lpAttributeList);free(env);CloseHandle(parent);
    if(!spawned)return 0;
    requested=PostMessage(window,WM_CLOSE,0,0)!=0;
    if(!requested)TerminateProcess(process.hProcess,1);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return requested;
}
#else
#include <unistd.h>
#include <spawn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <signal.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char **environ;
static char **restart_arguments;
static char restart_executable[4096];
int pc_game_restart_bootstrap(int argc,char **argv)
{
    (void)argc;restart_arguments=argv;
#ifdef __APPLE__
    uint32_t size=sizeof restart_executable;
    if(_NSGetExecutablePath(restart_executable,&size))restart_executable[0]=0;
#else
    ssize_t n=readlink("/proc/self/exe",restart_executable,sizeof restart_executable-1);
    if(n>0)restart_executable[n]=0;
#endif
    const char *value=getenv("TSFP_RESTART_WAIT");if(!value)return 1;
    char *end;long number=strtol(value,&end,10);int valid=!*end && number>=3 && number<=INT_MAX;
    unsetenv("TSFP_RESTART_WAIT");if(!valid)return 0;
    int fd=(int)number;struct stat st;
    if(fstat(fd,&st) || !S_ISFIFO(st.st_mode))return 0;
    struct pollfd wait={fd,POLLIN,0};int result;
    do {result=poll(&wait,1,60000);}while(result<0 && errno==EINTR);
    close(fd);return result>0 && (wait.revents&POLLHUP);
}
int pc_game_restart(void)
{
    static int requested;
    if(requested)return 1;
    if(!restart_arguments || !restart_executable[0])return 0;
    int pipefd[2];if(pipe(pipefd))return 0;
    if(fcntl(pipefd[1],F_SETFD,FD_CLOEXEC)<0){close(pipefd[0]);close(pipefd[1]);return 0;}
    size_t count=0;while(environ[count])count++;
    char **env=calloc(count+2,sizeof *env);if(!env){close(pipefd[0]);close(pipefd[1]);return 0;}
    size_t used=0;for(size_t i=0;i<count;i++)if(strncmp(environ[i],"TSFP_RESTART_WAIT=",18))env[used++]=environ[i];
    char marker[64];snprintf(marker,sizeof marker,"TSFP_RESTART_WAIT=%d",pipefd[0]);env[used]=marker;
    pid_t child;int result=posix_spawn(&child,restart_executable,NULL,NULL,restart_arguments,env);
    free(env);close(pipefd[0]);
    if(result){close(pipefd[1]);return 0;}
    extern int tsfp_native_request_quit(void);
    requested=tsfp_native_request_quit();
    if(!requested){kill(child,SIGTERM);close(pipefd[1]);}
    /* On success keep the write end until process exit, including all cleanup. */
    return requested;
}
#endif
