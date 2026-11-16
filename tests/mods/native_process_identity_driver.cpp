#include "mod_process_identity.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif
using namespace tsfp::mods;
static bool number(const char *text,uint64_t &value) {
    if(!text || !*text || std::strlen(text)>20)return false;
    value=0;
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text);*p;++p) {
        if(*p<'0' || *p>'9' || value>(std::numeric_limits<uint64_t>::max()-(*p-'0'))/10)return false;
        value=value*10+(*p-'0');
    }
    return true;
}
static const char *name(ProcessState state) {
    return state==ProcessState::alive?"alive":state==ProcessState::exited?"exited":"unknown";
}
int main(int argc,char **argv) {
    ProcessStamp stamp;ProcessState state=ProcessState::unknown;
    if(argc==3 && !std::strcmp(argv[1],"read")) {
        uint64_t pid;if(!number(argv[2],pid))return 2;state=read_process_stamp(pid,stamp);
    }else if(argc==6 && !std::strcmp(argv[1],"state")) {
        uint64_t kind;
        if(!number(argv[2],kind) || kind>std::numeric_limits<uint32_t>::max() || !number(argv[3],stamp.pid)
            || !number(argv[4],stamp.start_a) || !number(argv[5],stamp.start_b))return 2;
        stamp.kind=static_cast<ProcessStampKind>(kind);state=process_stamp_state(stamp);
    }else if(argc==2 && !std::strcmp(argv[1],"self")) {
#ifdef _WIN32
        state=read_process_stamp(GetCurrentProcessId(),stamp);
#else
        state=read_process_stamp(static_cast<uint64_t>(getpid()),stamp);
#endif
#ifdef _WIN32
    }else if(argc==2 && !std::strcmp(argv[1],"owned")) {
        state=read_owned_process_stamp(reinterpret_cast<uintptr_t>(GetCurrentProcess()),stamp);
#endif
    }else return 2;
    std::printf("{\"state\":\"%s\",\"kind\":%u,\"pid\":%llu,\"start_a\":%llu,\"start_b\":%llu,\"valid\":%s}\n",
        name(state),static_cast<unsigned>(stamp.kind),static_cast<unsigned long long>(stamp.pid),
        static_cast<unsigned long long>(stamp.start_a),static_cast<unsigned long long>(stamp.start_b),stamp.valid()?"true":"false");
    return 0;
}
