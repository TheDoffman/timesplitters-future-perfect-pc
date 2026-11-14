#ifndef TSFP_MOD_RUNTIME_HPP
#define TSFP_MOD_RUNTIME_HPP
#include "mod_manager.hpp"
#include <functional>
#include <memory>
#include <string>
#include <thread>
namespace tsfp { namespace mods {
// Simulation-thread facade with a dedicated background save/cache preflight.
// The trusted caller retains all probe resources until stopped(), then destroys
// this owner before releasing them. The probe never touches guest/game state.
class ScriptRuntime {
public:
    static std::unique_ptr<ScriptRuntime> create(const TsfpModMatchConfig &,std::function<bool()> probe,std::string &error);
    ~ScriptRuntime();
    // No filesystem calls, child IO, blocking mutex acquisition or thread joins.
    TsfpModMatch *update(const TsfpModWorld *);
    void close() noexcept;
    bool stopped()const noexcept;
    bool faulted()const noexcept{return !error_.empty();}
    const std::string &error()const noexcept{return error_;}
private:
    struct State;
    ScriptRuntime()=default;
    std::shared_ptr<State> state_;
    std::unique_ptr<MatchManager> manager_;
    std::thread thread_;
    uint64_t seen_=0,floor_=0,approved_=0,bound_=0;
    std::string error_;
    void retire();
};
}}
#endif
