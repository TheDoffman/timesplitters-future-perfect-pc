#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "mod_process.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <thread>
#include <pthread.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#else
#include <libproc.h>
#include <spawn.h>
#endif
namespace tsfp { namespace mods {
namespace {
void close_fd(int &fd){if(fd>=0)close(fd);fd=-1;}
class PosixProcess final:public Process {
public:
    pid_t pid=-1,worker_pid=-1;int input=-1,output=-1,diagnostic=-1,liveness=-1;bool reaped=false;int code=0;
    ~PosixProcess()override{terminate();close_fd(input);close_fd(output);close_fd(diagnostic);}
    int write(const void *bytes,size_t size)override {
        if(input<0)return -1;
        sigset_t blocked,previous,pending;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
        if(pthread_sigmask(SIG_BLOCK,&blocked,&previous))return -1;
        sigpending(&pending);bool already_pending=sigismember(&pending,SIGPIPE)==1;
        ssize_t count=::write(input,bytes,size);int write_error=errno;
        if(count<0 && write_error==EPIPE && !already_pending){sigpending(&pending);if(sigismember(&pending,SIGPIPE)==1){int number;sigwait(&blocked,&number);}}
        pthread_sigmask(SIG_SETMASK,&previous,nullptr);errno=write_error;
        if(count<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR))return 0;
        return (int)count;
    }
    int read(void *bytes,size_t size,bool error)override {
        ssize_t count=::read(error?diagnostic:output,bytes,size);
        if(count<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR))return 0;
        return (int)count;
    }
    void close_input()override{close_fd(input);}
    bool usage(ProcessUsage &result)override {
        if(pid<=0)return false;
        int status=0;if(!reaped) {
            pid_t found=waitpid(pid,&status,WNOHANG);
            if(found==pid){reaped=true;code=WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);}
            else if(found<0)return false;
        }
        result.exited=reaped;result.exit_code=code;if(reaped)return true;
#ifdef __linux__
        char path[80],data[4096];snprintf(path,sizeof(path),"/proc/%ld/stat",(long)pid);
        int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return errno==ENOENT;
        ssize_t count=::read(fd,data,sizeof(data)-1);close(fd);if(count<=0)return false;data[count]=0;
        char *end=strrchr(data,')');if(!end)return false;
        // Skip state(field3)..cmajflt(field13); then parse CPU and resident pages.
        unsigned long long user=0,kernel=0;long resident=0;
        int fields=sscanf(end+2,"%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %*d %*d %*d %*d %*d %*d %*u %*u %ld",&user,&kernel,&resident);
        if(fields!=3)return false;
        result.cpu_microseconds=(user+kernel)*1000000/(uint64_t)sysconf(_SC_CLK_TCK);
        result.resident_bytes=(uint64_t)(resident<0?0:resident)*(uint64_t)sysconf(_SC_PAGESIZE);
#else
        rusage_info_v4 info{};
        if(proc_pid_rusage(worker_pid,RUSAGE_INFO_V4,(rusage_info_t *)&info)!=0)return errno==ESRCH;
        result.resident_bytes=info.ri_phys_footprint;result.cpu_microseconds=(info.ri_user_time+info.ri_system_time)/1000;
#endif
        return true;
    }
    void terminate()override {
#ifdef __APPLE__
        // The separate trusted guardian owns/reaps the worker and watches this
        // pipe even when this process is abruptly killed. Do not kill it first.
        close_fd(liveness);close_input();
        if(pid>0 && !reaped) {
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            do {
                int status;pid_t found=waitpid(pid,&status,WNOHANG);
                if(found==pid || (found<0 && errno==ECHILD)){reaped=true;return;}
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }while(std::chrono::steady_clock::now()<deadline);
        }
#endif
        if(pid>0 && !reaped){kill(-pid,SIGKILL);kill(pid,SIGKILL);int status;while(waitpid(pid,&status,0)<0 && errno==EINTR){}reaped=true;}
    }
};
bool pipe_pair(int (&fds)[2]) {
    if(pipe(fds))return false;
    return fcntl(fds[0],F_SETFD,FD_CLOEXEC)==0 && fcntl(fds[1],F_SETFD,FD_CLOEXEC)==0;
}
}
std::unique_ptr<Process> Process::start(const std::string &image,const std::string &cache,std::string &error) {
    (void)cache;std::error_code fs;
    if(!std::filesystem::path(image).is_absolute() || image.find('\0')!=std::string::npos || std::filesystem::is_symlink(image,fs) ||
        !std::filesystem::is_regular_file(image,fs) || fs){error="Invalid trusted worker path";return {};}
    auto child=std::make_unique<PosixProcess>();int in[2]={-1,-1},out[2]={-1,-1},err[2]={-1,-1};
    if(!pipe_pair(in) || !pipe_pair(out) || !pipe_pair(err)){for(int *p:{in,out,err}){close_fd(p[0]);close_fd(p[1]);}error="Cannot create worker pipes";return {};}
    char *environment[]={const_cast<char *>("LANG=C"),const_cast<char *>("LC_ALL=C"),const_cast<char *>("TZ=UTC"),nullptr};
#ifdef __APPLE__
    // Darwin has no closefrom(). CLOEXEC_DEFAULT closes *all* inherited FDs,
    // including high-numbered ones, except these explicit stdio dup actions.
    auto guardian=(std::filesystem::path(image).parent_path()/"tsfp-mod-guardian").string();
    if(std::filesystem::is_symlink(guardian,fs) || !std::filesystem::is_regular_file(guardian,fs) || fs) {
        for(int *p:{in,out,err}){close_fd(p[0]);close_fd(p[1]);}error="Trusted lifetime guardian unavailable";return {};
    }
    int life[2]={-1,-1},identity[2]={-1,-1};
    if(!pipe_pair(life) || !pipe_pair(identity)) {
        for(int *p:{in,out,err,life,identity}){close_fd(p[0]);close_fd(p[1]);}error="Cannot create guardian pipes";return {};
    }
    // Keep these sources above the fixed guardian FD3/FD4 targets so spawn dup
    // actions cannot overwrite another control source, even in a busy game.
    int life_source=fcntl(life[0],F_DUPFD_CLOEXEC,5),identity_source=fcntl(identity[1],F_DUPFD_CLOEXEC,5);
    char *guard_arguments[]={const_cast<char *>(guardian.c_str()),const_cast<char *>(image.c_str()),nullptr};
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    bool have_actions=posix_spawn_file_actions_init(&actions)==0;
    bool have_attributes=posix_spawnattr_init(&attributes)==0;
    pid_t pid=-1;
    int status=have_actions && have_attributes && life_source>=0 && identity_source>=0?0:ENOMEM;
    if(!status)status=posix_spawn_file_actions_adddup2(&actions,in[0],0);
    if(!status)status=posix_spawn_file_actions_adddup2(&actions,out[1],1);
    if(!status)status=posix_spawn_file_actions_adddup2(&actions,err[1],2);
    if(!status)status=posix_spawn_file_actions_adddup2(&actions,life_source,3);
    if(!status)status=posix_spawn_file_actions_adddup2(&actions,identity_source,4);
    if(!status)status=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT|POSIX_SPAWN_SETPGROUP);
    if(!status)status=posix_spawnattr_setpgroup(&attributes,0);
    if(!status)status=posix_spawn(&pid,guardian.c_str(),&actions,&attributes,guard_arguments,environment);
    if(have_actions)posix_spawn_file_actions_destroy(&actions);
    if(have_attributes)posix_spawnattr_destroy(&attributes);
    close_fd(life_source);close_fd(identity_source);close_fd(life[0]);close_fd(identity[1]);
    child->liveness=life[1];life[1]=-1;
    if(status)pid=-1;
#else
    char *arguments[]={const_cast<char *>(image.c_str()),nullptr};
    pid_t parent=getpid();pid_t pid=fork();
    if(pid==0) {
        if(setpgid(0,0) || dup2(in[0],0)<0 || dup2(out[1],1)<0 || dup2(err[1],2)<0)_exit(126);
        if(prctl(PR_SET_PDEATHSIG,SIGKILL) || getppid()!=parent || syscall(SYS_close_range,3u,~0u,0))_exit(126);
        struct rlimit core{0,0},files{64,64};if(setrlimit(RLIMIT_CORE,&core) || setrlimit(RLIMIT_NOFILE,&files))_exit(126);
        execve(image.c_str(),arguments,environment);_exit(127);
    }
#endif
    close_fd(in[0]);close_fd(out[1]);close_fd(err[1]);
    if(pid<0){
        close_fd(in[1]);close_fd(out[0]);close_fd(err[0]);
#ifdef __APPLE__
        close_fd(identity[0]);
#endif
        error="Cannot start worker";return {};
    }
    child->pid=pid;child->input=in[1];child->output=out[0];child->diagnostic=err[0];
#ifdef __APPLE__
    int64_t worker_identity=0;size_t received=0;
    if(fcntl(identity[0],F_SETFL,fcntl(identity[0],F_GETFL)|O_NONBLOCK)<0){close_fd(identity[0]);error="Cannot inspect guardian startup";return {};}
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(received<sizeof(worker_identity) && std::chrono::steady_clock::now()<deadline) {
        ssize_t count=::read(identity[0],(unsigned char *)&worker_identity+received,sizeof(worker_identity)-received);
        if(count>0)received+=(size_t)count;
        else if(!count || (errno!=EAGAIN && errno!=EINTR))break;
        else std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    close_fd(identity[0]);
    if(received!=sizeof(worker_identity) || worker_identity<=0 || worker_identity>INT32_MAX){error="Lifetime guardian startup failed";return {};}
    child->worker_pid=(pid_t)worker_identity;
#endif
    for(int fd:{child->input,child->output,child->diagnostic})if(fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK)<0){error="Cannot set worker pipe mode";return {};}
    return child;
}
}}
#endif
