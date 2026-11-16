#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "mod_process.hpp"
#include "mod_private.hpp"
#include "mod_process_stamp_wire.h"
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
#include <poll.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#else
#include <libproc.h>
#include <spawn.h>
#include <mach/mach_time.h>
#endif
namespace tsfp { namespace mods {
bool recover_process_artifacts(const ProcessArtifacts &artifacts,bool)noexcept {
    return artifacts.cache_leaf.empty() && artifacts.profile.empty() && artifacts.sid.empty() && !artifacts.volume && !artifacts.file;
}
namespace {
void close_fd(int &fd){if(fd>=0)close(fd);fd=-1;}
class PosixProcess final:public Process {
public:
    pid_t pid=-1,worker_pid=-1;int input=-1,output=-1,diagnostic=-1,liveness=-1;bool reaped=false;int code=0;
    ProcessStamps created;
    bool creation_stamps(ProcessStamps &out)const noexcept override {out=created;return out.valid();}
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
    void wait_for_activity(bool writing)override {
        struct pollfd descriptors[3]={{output,POLLIN,0},{diagnostic,POLLIN,0},{writing?input:-1,POLLOUT,0}};
        (void)::poll(descriptors,3,2);
    }
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
        // Darwin rusage CPU totals are Mach absolute ticks, not nanoseconds.
        // On Apple Silicon the timebase ratio need not be1:1.
        mach_timebase_info_data_t timebase{};
        if(mach_timebase_info(&timebase)!=KERN_SUCCESS || !timebase.numer || !timebase.denom)return false;
        __uint128_t ticks=(__uint128_t)info.ri_user_time+info.ri_system_time;
        __uint128_t microseconds=ticks*timebase.numer/((uint64_t)timebase.denom*1000);
        if(microseconds>UINT64_MAX)return false;
        result.resident_bytes=info.ri_phys_footprint;result.cpu_microseconds=(uint64_t)microseconds;
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
bool creation_wire(const TsfpModProcessBirthWire &wire,pid_t owner,ProcessStamps &out) {
    out={};if(wire.version!=1 || !wire.count || wire.count>2)return false;
    for(size_t at=0;at<wire.count;++at) {
        const auto &value=wire.values[at];if(value.reserved)return false;
        out.values[at]={static_cast<ProcessStampKind>(value.kind),value.pid,value.start_a,value.start_b};
    }
    for(size_t at=wire.count;at<2;++at) {
        const auto &value=wire.values[at];
        if(value.kind || value.reserved || value.pid || value.start_a || value.start_b)return false;
    }
    out.count=wire.count;return out.valid() && out.values[0].pid==static_cast<uint64_t>(owner);
}
}
std::unique_ptr<Process> Process::start(const std::string &image,const std::string &cache,std::string &error) {
    if(!cache_process_start_allowed(cache)){error="Owned cache requires its startup journal";return {};}
    std::error_code fs;
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
    // A game-sized parent can exceed the worker RSS cap before exec replaces
    // its inherited address space. EOF on this CLOEXEC pipe establishes that
    // replacement (or child exit) before any caller samples worker usage.
    int executed[2]={-1,-1};
    if(!pipe_pair(executed)) {
        for(int *p:{in,out,err,executed}){close_fd(p[0]);close_fd(p[1]);}
        error="Cannot establish worker exec boundary";return {};
    }
    pid_t parent=getpid();pid_t pid=fork();
    if(pid==0) {
        if(setpgid(0,0) || dup2(in[0],0)<0 || dup2(out[1],1)<0 || dup2(err[1],2)<0)_exit(126);
        if(dup2(executed[1],3)<0 || fcntl(3,F_SETFD,FD_CLOEXEC)<0)_exit(126);
        if(prctl(PR_SET_PDEATHSIG,SIGKILL) || getppid()!=parent || syscall(SYS_close_range,4u,~0u,0))_exit(126);
        struct rlimit core{0,0},files{64,64};if(setrlimit(RLIMIT_CORE,&core) || setrlimit(RLIMIT_NOFILE,&files))_exit(126);
        // This fixed, allocation-free self-query happens before exec. The
        // private CLOEXEC pipe proves provenance without reopening a reused PID.
        ProcessStamp stamp;
        if(read_process_stamp(static_cast<uint64_t>(getpid()),stamp)!=ProcessState::alive)_exit(126);
        TsfpModProcessBirthWire birth{};birth.version=1;birth.count=1;
        birth.values[0]={static_cast<uint32_t>(stamp.kind),0,stamp.pid,stamp.start_a,stamp.start_b};
        size_t sent=0;
        while(sent<sizeof(birth)) {
            ssize_t count=::write(3,reinterpret_cast<const unsigned char *>(&birth)+sent,sizeof(birth)-sent);
            if(count<0 && errno==EINTR)continue;
            if(count<=0)_exit(126);
            sent+=static_cast<size_t>(count);
        }
        execve(image.c_str(),arguments,environment);_exit(127);
    }
    close_fd(executed[1]);
#endif
    close_fd(in[0]);close_fd(out[1]);close_fd(err[1]);
    if(pid<0){
        close_fd(in[1]);close_fd(out[0]);close_fd(err[0]);
#ifdef __APPLE__
        close_fd(identity[0]);
#else
        close_fd(executed[0]);
#endif
        error="Cannot start worker";return {};
    }
    child->pid=pid;child->input=in[1];child->output=out[0];child->diagnostic=err[0];
#ifdef __APPLE__
    TsfpModProcessBirthWire worker_identity{};size_t received=0;
    if(fcntl(identity[0],F_SETFL,fcntl(identity[0],F_GETFL)|O_NONBLOCK)<0){close_fd(identity[0]);error="Cannot inspect guardian startup";return {};}
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(received<sizeof(worker_identity) && std::chrono::steady_clock::now()<deadline) {
        ssize_t count=::read(identity[0],(unsigned char *)&worker_identity+received,sizeof(worker_identity)-received);
        if(count>0)received+=(size_t)count;
        else if(!count || (errno!=EAGAIN && errno!=EINTR))break;
        else std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    close_fd(identity[0]);
    if(received!=sizeof(worker_identity) || !creation_wire(worker_identity,pid,child->created)
        || child->created.values[1].pid==static_cast<uint64_t>(getpid())){error="Lifetime guardian startup failed";return {};}
    child->worker_pid=static_cast<pid_t>(child->created.values[1].pid);
#else
    bool crossed=false;TsfpModProcessBirthWire birth{};size_t received=0;
    if(fcntl(executed[0],F_SETFL,fcntl(executed[0],F_GETFL)|O_NONBLOCK)>=0) {
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(std::chrono::steady_clock::now()<deadline) {
            unsigned char unexpected;
            ssize_t count=received<sizeof(birth)?::read(executed[0],reinterpret_cast<unsigned char *>(&birth)+received,sizeof(birth)-received)
                : ::read(executed[0],&unexpected,1);
            if(count>0 && received<sizeof(birth)){received+=static_cast<size_t>(count);continue;}
            if(!count){crossed=received==sizeof(birth);break;}
            if(count>0 || (errno!=EAGAIN && errno!=EINTR))break;
            struct pollfd pending{executed[0],POLLIN,0};(void)::poll(&pending,1,2);
        }
    }
    close_fd(executed[0]);
    if(!crossed || !creation_wire(birth,pid,child->created)){error="Worker exec/identity boundary failed";return {};}
#endif
    for(int fd:{child->input,child->output,child->diagnostic})if(fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK)<0){error="Cannot set worker pipe mode";return {};}
    return child;
}
}}
#endif
