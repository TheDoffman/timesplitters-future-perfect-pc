#ifndef TSFP_MOD_PROCESS_HPP
#define TSFP_MOD_PROCESS_HPP
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

// Trusted parent only. No Lua is linked into this module. Paths come from the
// verified game/SDK installation, never from an uploaded mod manifest.
namespace tsfp { namespace mods {
struct ProcessUsage { uint64_t resident_bytes=0,cpu_microseconds=0;bool exited=false;int exit_code=0; };
class Process {
public:
    static std::unique_ptr<Process> start(const std::string &executable,const std::string &cache,std::string &error);
    virtual ~Process()=default;
    // Nonblocking parent pipe operations: -1 error,0 would block/EOF,>0 bytes.
    virtual int write(const void *,size_t)=0;
    virtual int read(void *,size_t,bool diagnostic=false)=0;
    virtual bool usage(ProcessUsage &)=0;
    virtual void close_input()=0;
    // Kill/reap only this owned process/job; never a name-based process search.
    virtual void terminate()=0;
};
}}
#endif
