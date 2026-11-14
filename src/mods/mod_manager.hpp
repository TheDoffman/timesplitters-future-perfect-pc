#ifndef TSFP_MOD_MANAGER_HPP
#define TSFP_MOD_MANAGER_HPP
#include "mod_match.h"
#include <memory>
#include <string>
namespace tsfp { namespace mods {
// Simulation-thread owner. Configure once with bootstrap-owned policy/bytes.
// update/close never wait for a child. Retains retiring matches until reaped;
// never opens two workers or reopens a failed/invalidated match epoch.
class MatchManager {
public:
    static std::unique_ptr<MatchManager> create(const TsfpModMatchConfig &,std::string &error);
    ~MatchManager();
    TsfpModMatch *update(const TsfpModWorld *);
    void close() noexcept;
    bool stopped() const noexcept;
    const std::string &error()const noexcept{return error_;}
    MatchManager(const MatchManager &)=delete;
    MatchManager &operator=(const MatchManager &)=delete;
private:
    MatchManager()=default;
    std::string worker_,cache_,source_,worker_pin_,guardian_pin_,error_;
    uint32_t seed_=0,verified_=0;
    uint64_t last_epoch_=0;
    TsfpModMatch *match_=nullptr;
    bool retiring_=false,closed_=false;
    void retire() noexcept;
};
}}
#endif
