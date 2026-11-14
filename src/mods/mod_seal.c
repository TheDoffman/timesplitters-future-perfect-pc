#define _GNU_SOURCE
#include "mod_seal.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int tsfp_mod_seal(void) {
    HANDLE token=NULL;DWORD app=0,size=0;BOOL in_job=FALSE;
    TOKEN_GROUPS capabilities;PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynamic;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 0;
    /* LPAC opt-out is set by the trusted broker at creation. Class46 is not a
       supported GetTokenInformation query on current Windows user-mode builds. */
    int okay=GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&size) && app &&
        GetTokenInformation(token,TokenCapabilities,&capabilities,sizeof(capabilities),&size) && capabilities.GroupCount==0;
    CloseHandle(token);
    if(!okay || !IsProcessInJob(GetCurrentProcess(),NULL,&in_job) || !in_job ||
       !QueryInformationJobObject(NULL,JobObjectExtendedLimitInformation,&limits,sizeof(limits),NULL))return 0;
    if(!(limits.BasicLimitInformation.LimitFlags&JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) ||
       !(limits.BasicLimitInformation.LimitFlags&JOB_OBJECT_LIMIT_ACTIVE_PROCESS) || limits.BasicLimitInformation.ActiveProcessLimit!=1 ||
       !(limits.BasicLimitInformation.LimitFlags&JOB_OBJECT_LIMIT_PROCESS_MEMORY) || limits.ProcessMemoryLimit>128u*1024*1024)return 0;
    return GetProcessMitigationPolicy(GetCurrentProcess(),ProcessDynamicCodePolicy,&dynamic,sizeof(dynamic)) && dynamic.ProhibitDynamicCode;
}
#elif defined(__linux__) && defined(__x86_64__)
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#define ALLOW(number) BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,number,0,1),BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW)
int tsfp_mod_seal(void) {
    struct rlimit core={0,0},files={3,3},address={128u*1024*1024,128u*1024*1024};
    if(setrlimit(RLIMIT_CORE,&core) || setrlimit(RLIMIT_NOFILE,&files) || setrlimit(RLIMIT_AS,&address))return 0;
    if(syscall(SYS_close_range,3u,~0u,0) || prctl(PR_SET_DUMPABLE,0) || prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return 0;
    const struct sock_filter rules[]={
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,arch)),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,AUDIT_ARCH_X86_64,1,0),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,nr)),
        /* mmap/mprotect may never introduce executable writable data. */
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,SYS_mmap,1,0),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,SYS_mprotect,0,4),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,args[2])),
        BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K,PROT_EXEC,0,1),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ERRNO|EPERM),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW),
        /* Existing private pipes only: read stdin; write stdout/stderr. */
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,SYS_read,0,4),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,args[0])),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,0,0,1),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ERRNO|EPERM),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,SYS_write,0,5),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS,offsetof(struct seccomp_data,args[0])),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,1,1,0),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,2,0,1),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ERRNO|EPERM),
        ALLOW(SYS_brk),ALLOW(SYS_munmap),ALLOW(SYS_madvise),ALLOW(SYS_mremap),
        ALLOW(SYS_clock_gettime),ALLOW(SYS_clock_nanosleep),ALLOW(SYS_nanosleep),
        ALLOW(SYS_futex),ALLOW(SYS_rt_sigreturn),ALLOW(SYS_rt_sigprocmask),ALLOW(SYS_rt_sigaction),
        ALLOW(SYS_exit),ALLOW(SYS_exit_group),ALLOW(SYS_sched_yield),ALLOW(SYS_restart_syscall),
        ALLOW(SYS_getrandom),ALLOW(SYS_getpid),ALLOW(SYS_gettid),
        BPF_STMT(BPF_RET|BPF_K,SECCOMP_RET_ERRNO|EPERM)
    };
    struct sock_fprog program={(unsigned short)(sizeof(rules)/sizeof(rules[0])),(struct sock_filter *)rules};
    return syscall(SYS_seccomp,SECCOMP_SET_MODE_FILTER,SECCOMP_FILTER_FLAG_TSYNC,&program)==0;
}
#elif defined(__APPLE__)
#include <sandbox.h>
#include <sys/resource.h>
#include <unistd.h>
int tsfp_mod_seal(void) {
    struct rlimit previous,core={0,0},files={3,3};char *error=NULL;int status;
    if(getrlimit(RLIMIT_NOFILE,&previous) || previous.rlim_cur>1048576)return 0;
    for(int fd=3;(rlim_t)fd<previous.rlim_cur;++fd)close(fd);
    if(setrlimit(RLIMIT_CORE,&core) || setrlimit(RLIMIT_NOFILE,&files))return 0;
    const char *profile="(version 1)(deny default)(allow file-read-metadata)";
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    status=sandbox_init(profile,0,&error);
    if(error)sandbox_free_error(error);
#pragma clang diagnostic pop
    return status==0;
}
#else
int tsfp_mod_seal(void){return 0;}
#endif
