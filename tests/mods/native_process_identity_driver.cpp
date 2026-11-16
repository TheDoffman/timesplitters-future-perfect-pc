#include "mod_process_identity.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif
using namespace tsfp::mods;
#ifdef _WIN32
static bool debug_enabled(HANDLE token,const LUID &debug,bool &enabled) {
    DWORD size=0;GetTokenInformation(token,TokenPrivileges,nullptr,0,&size);
    if(size<sizeof(DWORD) || size>65536)return false;
    std::vector<unsigned char> bytes(size);
    if(!GetTokenInformation(token,TokenPrivileges,bytes.data(),size,&size))return false;
    auto *privileges=reinterpret_cast<TOKEN_PRIVILEGES *>(bytes.data());
    if(privileges->PrivilegeCount>128 || offsetof(TOKEN_PRIVILEGES,Privileges)+static_cast<size_t>(privileges->PrivilegeCount)*sizeof(LUID_AND_ATTRIBUTES)>size)return false;
    enabled=false;
    for(DWORD at=0;at<privileges->PrivilegeCount;++at) {
        const auto &entry=privileges->Privileges[at];
        if(entry.Luid.HighPart==debug.HighPart && entry.Luid.LowPart==debug.LowPart)enabled=(entry.Attributes&SE_PRIVILEGE_ENABLED)!=0;
    }
    return true;
}
struct ThreadRestore {bool active=false;~ThreadRestore(){if(active)RevertToSelf();}};
static bool disable_debug(bool &previously_enabled,bool &impersonating) {
    if(!ImpersonateSelf(SecurityImpersonation))return false;
    impersonating=true;
    HANDLE token=nullptr;if(!OpenThreadToken(GetCurrentThread(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,TRUE,&token))return false;
    LUID debug{};bool result=false;
    if(LookupPrivilegeValueW(nullptr,L"SeDebugPrivilege",&debug) && debug_enabled(token,debug,previously_enabled)) {
        TOKEN_PRIVILEGES desired{};desired.PrivilegeCount=1;desired.Privileges[0].Luid=debug;
        bool enabled=true;
        // An absent privilege is already disabled. Independently inspect the
        // resulting token instead of assuming AdjustTokenPrivileges changed it.
        if(AdjustTokenPrivileges(token,FALSE,&desired,0,nullptr,nullptr) && debug_enabled(token,debug,enabled))result=!enabled;
    }
    CloseHandle(token);return result;
}
#endif
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
    bool debug_disabled=false,debug_was_enabled=false;
#ifdef _WIN32
    ThreadRestore restore;
    if(argc>1 && !std::strcmp(argv[1],"no-debug")) {
        if(!disable_debug(debug_was_enabled,restore.active))return 3;
        debug_disabled=true;--argc;++argv;
    }
#endif
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
    std::printf("{\"state\":\"%s\",\"kind\":%u,\"pid\":%llu,\"start_a\":%llu,\"start_b\":%llu,\"valid\":%s,\"debug_disabled\":%s,\"debug_was_enabled\":%s}\n",
        name(state),static_cast<unsigned>(stamp.kind),static_cast<unsigned long long>(stamp.pid),
        static_cast<unsigned long long>(stamp.start_a),static_cast<unsigned long long>(stamp.start_b),stamp.valid()?"true":"false",
        debug_disabled?"true":"false",debug_disabled?(debug_was_enabled?"true":"false"):"null");
    return 0;
}
