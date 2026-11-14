#ifndef TSFP_MOD_SESSION_HPP
#define TSFP_MOD_SESSION_HPP
#include "mod_script.h"
#include <memory>
#include <string>

namespace tsfp { namespace mods {
enum class SessionStatus { Loading, Ready, Busy, Reply, Finished, Closing, Faulted, Contended };
enum class Submission { Accepted, Busy, Rejected, Faulted };

// Trusted host API. Creation/dispatch/poll/close perform no child IO or waits.
// The host must verify worker/helper images, package/source, authority and save
// namespace before opening. A future game adapter must revalidate the live match
// epoch/roster before applying a returned transaction, entirely or not at all.
class Session {
public:
    static std::unique_ptr<Session> open(const std::string &image,const std::string &cache,
        std::string source,uint32_t seed,std::string &error);
    ~Session();
    Submission dispatch(const tsfp_script_event &event);
    // Always clears commands/sequence on non-Reply. Never waits for a lock.
    SessionStatus poll(tsfp_script_result &commands,uint64_t &sequence,std::string &error);
    void close() noexcept;
    bool stopped() const noexcept; // True only after the owned process was reaped.
    Session(const Session &)=delete;
    Session &operator=(const Session &)=delete;
private:
    struct State;
    explicit Session(std::shared_ptr<State> state):state_(std::move(state)){}
    static void work(std::shared_ptr<State>,std::string image,std::string cache,std::string source,uint32_t seed) noexcept;
    std::shared_ptr<State> state_;
};
}}
#endif
