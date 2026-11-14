#include "mod_manager.hpp"
#include "mod_files.hpp"
#include "mod_images.hpp"
#include <cstring>
#include <stdexcept>
namespace tsfp { namespace mods {
namespace {
std::string bounded(const char *p,size_t maximum) {
    if(!p)throw std::runtime_error("Missing verified match configuration");
    size_t n=0;while(n<=maximum && p[n])++n;
    if(!n || n>maximum)throw std::runtime_error("Invalid verified match configuration");
    return std::string(p,n);
}
}
std::unique_ptr<MatchManager> MatchManager::create(const TsfpModMatchConfig &config,std::string &error) {
    error.clear();
    try {
        constexpr auto policy=TSFP_MOD_IMAGES_VERIFIED|TSFP_MOD_SOURCE_VERIFIED|TSFP_MOD_SEPARATE_SAVES|TSFP_MOD_OFFLINE;
        if(config.verified!=policy || !config.source || !config.source_size || config.source_size>TSFP_SCRIPT_SOURCE_MAX)
            throw std::runtime_error("Unverified script match configuration");
        auto result=std::unique_ptr<MatchManager>(new MatchManager);
        result->worker_=bounded(config.worker,4095);result->cache_=bounded(config.cache,4095);
        result->worker_pin_=bounded(config.worker_sha256,64);
        result->guardian_pin_=(config.guardian_sha256 && *config.guardian_sha256)?bounded(config.guardian_sha256,64):"";
        if(!ImagePins{result->worker_pin_,result->guardian_pin_}.valid())throw std::runtime_error("Invalid compiled helper pins");
        result->source_.assign(config.source,config.source_size);
        result->seed_=config.seed;result->verified_=config.verified;return result;
    }catch(...){error="Script match manager configuration was refused";return nullptr;}
}
void MatchManager::retire() noexcept {
    if(match_ && !retiring_){retiring_=true;tsfp_mod_match_close(match_);}
}
TsfpModMatch *MatchManager::update(const TsfpModWorld *world) {
    if(match_ && !retiring_) {
        if(closed_ || !world || !tsfp_mod_match_matches(match_,world) || tsfp_mod_match_faulted(match_)) {
            if(tsfp_mod_match_faulted(match_))error_=tsfp_mod_match_error(match_);
            retire();
        }
    }
    if(retiring_) {
        if(!tsfp_mod_match_stopped(match_))return nullptr;
        tsfp_mod_match_destroy(match_);match_=nullptr;retiring_=false;
    }
    if(closed_ || !world)return nullptr;
    if(match_)return match_;
    // Epoch is host-owned and monotonically increasing. An invalidated roster
    // or transient unsupported/networked world cannot resurrect the same epoch.
    if(!world->epoch || world->epoch<=last_epoch_)return nullptr;
    last_epoch_=world->epoch;
    TsfpModMatchConfig config{worker_.c_str(),cache_.c_str(),source_.data(),source_.size(),seed_,verified_,worker_pin_.c_str(),guardian_pin_.c_str()};
    match_=tsfp_mod_match_open(&config,world);
    if(!match_)error_="Script match startup was refused";
    return match_;
}
void MatchManager::close() noexcept {closed_=true;retire();}
bool MatchManager::stopped()const noexcept {return !match_ || tsfp_mod_match_stopped(match_);}
MatchManager::~MatchManager() {
    // Match/session destruction signals cancellation; production shutdown must
    // keep this module loaded until stopped(), then destroy the owner.
    if(match_)tsfp_mod_match_destroy(match_);
}
}}
