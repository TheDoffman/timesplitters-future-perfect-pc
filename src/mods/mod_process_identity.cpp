#include "mod_process_identity.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string_view>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/proc.h>
#endif
#endif

namespace tsfp { namespace mods {
ProcessStampKind native_process_stamp_kind()noexcept {
#ifdef _WIN32
    return ProcessStampKind::windows_filetime;
#elif defined(__linux__)
    return ProcessStampKind::linux_start_ticks;
#elif defined(__APPLE__)
    return ProcessStampKind::darwin_start_time;
#else
    return ProcessStampKind::none;
#endif
}
bool ProcessStamp::valid()const noexcept {
    if(kind==ProcessStampKind::none || kind!=native_process_stamp_kind() || !pid || !start_a)return false;
#ifdef _WIN32
    if(pid>std::numeric_limits<uint32_t>::max())return false;
#else
    if(pid>static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))return false;
#endif
    return kind==ProcessStampKind::darwin_start_time?start_b<1000000:start_b==0;
}
bool ProcessStamps::valid()const noexcept {
    size_t expected=native_process_stamp_kind()==ProcessStampKind::darwin_start_time?2u:1u;
    if(count!=expected)return false;
    for(size_t at=0;at<count;++at)if(!values[at].valid())return false;
    return count!=2 || values[0].pid!=values[1].pid;
}
#ifdef _WIN32
ProcessState read_owned_process_stamp(uintptr_t borrowed,ProcessStamp &out)noexcept {
    out={};HANDLE handle=reinterpret_cast<HANDLE>(borrowed);
    if(!handle)return ProcessState::unknown;
    DWORD pid=GetProcessId(handle);FILETIME created{},exited{},kernel{},user{};
    if(!pid || !GetProcessTimes(handle,&created,&exited,&kernel,&user))return ProcessState::unknown;
    ProcessStamp sample{native_process_stamp_kind(),pid,(static_cast<uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime,0};
    if(!sample.valid())return ProcessState::unknown;
    DWORD state=WaitForSingleObject(handle,0);
    if(state!=WAIT_TIMEOUT && state!=WAIT_OBJECT_0)return ProcessState::unknown;
    out=sample;return state==WAIT_OBJECT_0?ProcessState::exited:ProcessState::alive;
}
#else
namespace {
ProcessState missing_task(uint64_t pid,int error)noexcept {
    if(error!=ENOENT && error!=ESRCH)return ProcessState::unknown;
    // A hidden/unavailable proc record is not death. Signal0 is only an
    // existence query; EPERM or a visible task conservatively preserves it.
    if(kill(static_cast<pid_t>(pid),0)==-1 && errno==ESRCH)return ProcessState::exited;
    return ProcessState::unknown;
}
#ifdef __linux__
bool decimal(std::string_view text,uint64_t &value)noexcept {
    if(text.empty() || text.size()>20)return false;
    value=0;
    for(unsigned char c:text) {
        if(c<'0' || c>'9' || value>(std::numeric_limits<uint64_t>::max()-(c-'0'))/10)return false;
        value=value*10+(c-'0');
    }
    return true;
}
ProcessState linux_stamp(uint64_t pid,ProcessStamp &out) {
    // No heap allocation or inherited userspace lock in the post-fork prelude.
    char path[64]="/proc/",digits[20];size_t at=6,n=0;uint64_t number=pid;
    do{digits[n++]=static_cast<char>('0'+number%10);number/=10;}while(number);
    while(n)path[at++]=digits[--n];
    std::memcpy(path+at,"/stat",6);
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return missing_task(pid,errno);
    std::array<char,4096> bytes{};ssize_t size;
    do{size=read(fd,bytes.data(),bytes.size());}while(size<0 && errno==EINTR);
    close(fd);
    if(size<=0 || static_cast<size_t>(size)==bytes.size())return ProcessState::unknown;
    std::string_view data(bytes.data(),static_cast<size_t>(size));
    size_t left=data.find('('),right=data.rfind(')');
    uint64_t named_pid=0;
    if(left==std::string_view::npos || right==std::string_view::npos || left<2 || left>=right || right+2>=data.size()
        || data[left-1]!=' ' || !decimal(data.substr(0,left-1),named_pid) || named_pid!=pid || data[right+1]!=' ')return ProcessState::unknown;
    auto tail=data.substr(right+2);std::string_view state;uint64_t ticks=0;
    for(unsigned field=3;field<=22;++field) {
        size_t begin=tail.find_first_not_of(" \t\r\n");if(begin==std::string_view::npos)return ProcessState::unknown;
        tail.remove_prefix(begin);size_t end=tail.find_first_of(" \t\r\n");
        auto token=tail.substr(0,end);
        if(field==3)state=token;
        if(field==22 && !decimal(token,ticks))return ProcessState::unknown;
        if(end==std::string_view::npos){if(field!=22)return ProcessState::unknown;tail={};}
        else tail.remove_prefix(end);
    }
    ProcessStamp sample{native_process_stamp_kind(),pid,ticks,0};
    if(!sample.valid() || state.size()!=1 || !state[0] || !std::strchr("RSDTtZXxKWPI",state[0]))return ProcessState::unknown;
    out=sample;
    return state[0]=='Z' || state[0]=='X' || state[0]=='x'?ProcessState::exited:ProcessState::alive;
}
#endif
}
#endif
ProcessState read_process_stamp(uint64_t pid,ProcessStamp &out)noexcept {
    out={};ProcessStamp admission{native_process_stamp_kind(),pid,1,0};
    if(!admission.valid())return ProcessState::unknown;
    try {
#ifdef _WIN32
        HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(pid));
        if(!handle)return GetLastError()==ERROR_INVALID_PARAMETER?ProcessState::exited:ProcessState::unknown;
        auto state=read_owned_process_stamp(reinterpret_cast<uintptr_t>(handle),out);CloseHandle(handle);return state;
#elif defined(__linux__)
        return linux_stamp(pid,out);
#elif defined(__APPLE__)
        proc_bsdinfo info{};errno=0;
        int count=proc_pidinfo(static_cast<int>(pid),PROC_PIDTBSDINFO,0,&info,static_cast<int>(sizeof(info)));
        if(count!=static_cast<int>(sizeof(info)))return count==0?missing_task(pid,errno):ProcessState::unknown;
        ProcessStamp sample{native_process_stamp_kind(),pid,info.pbi_start_tvsec,info.pbi_start_tvusec};
        if(info.pbi_pid!=pid || !sample.valid())return ProcessState::unknown;
        out=sample;return info.pbi_status==SZOMB?ProcessState::exited:ProcessState::alive;
#else
        return ProcessState::unknown;
#endif
    }catch(...){out={};return ProcessState::unknown;}
}
ProcessState process_stamp_state(const ProcessStamp &record)noexcept {
    if(!record.valid())return ProcessState::unknown;
    ProcessStamp current;auto state=read_process_stamp(record.pid,current);
    if(state!=ProcessState::alive)return state;
    if(!current.valid())return ProcessState::unknown;
    return current.kind==record.kind && current.start_a==record.start_a && current.start_b==record.start_b
        ?ProcessState::alive:ProcessState::exited;
}
}}
