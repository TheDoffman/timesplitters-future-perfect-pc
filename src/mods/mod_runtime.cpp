#include "mod_runtime.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
namespace tsfp { namespace mods {
struct ScriptRuntime::State {
    std::mutex mutex;std::condition_variable wake;
    std::atomic<bool> closing{false},done{false},failed{false};
    bool queued=false,busy=false,result=false,ok=false;
    uint64_t requested=0,finished=0;
    std::function<bool()> probe;
    explicit State(std::function<bool()> p):probe(std::move(p)){}
    void work() noexcept {
        try {
            for(;;) {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock,[&]{return closing.load() || queued;});
                if(closing.load())break;
                auto epoch=requested;queued=false;lock.unlock();
                bool accepted=false;try{accepted=probe();}catch(...){}
                lock.lock();finished=epoch;ok=accepted;result=true;
            }
        }catch(...){failed.store(true);closing.store(true);}
        done.store(true);
    }
};
std::unique_ptr<ScriptRuntime> ScriptRuntime::create(const TsfpModMatchConfig &config,std::function<bool()> probe,std::string &error) {
    error.clear();
    try {
        if(!probe){error="Missing native script preflight";return nullptr;}
        auto result=std::unique_ptr<ScriptRuntime>(new ScriptRuntime);
        result->manager_=MatchManager::create(config,error);if(!result->manager_)return nullptr;
        result->state_=std::make_shared<State>(std::move(probe));
        result->thread_=std::thread([state=result->state_]{state->work();});return result;
    }catch(...){error="Native script preflight could not start";return nullptr;}
}
void ScriptRuntime::retire() {
    floor_=std::max(floor_,bound_);bound_=0;
    (void)manager_->update(nullptr);
}
TsfpModMatch *ScriptRuntime::update(const TsfpModWorld *world) {
    if(state_->closing.load()) {
        if(!faulted() && state_->failed.load())error_="Native script preflight stopped unexpectedly";
        manager_->close();retire();return nullptr;
    }
    if(!world || !world->epoch || world->epoch<seen_ || !world->authoritative || !world->offline) {
        floor_=std::max(floor_,seen_);approved_=0;retire();world=nullptr;
    }else seen_=std::max(seen_,world->epoch);
    bool wake=false;
    {
        std::unique_lock<std::mutex> lock(state_->mutex,std::try_to_lock);
        if(lock.owns_lock()) {
            if(state_->result) {
                if(!state_->ok)error_="Isolated script saves or helper cache changed";
                else if(world && world->epoch==state_->finished && world->epoch>floor_)approved_=world->epoch;
                state_->result=false;state_->busy=false;
            }
            if(!faulted() && world && world->epoch>floor_ && approved_!=world->epoch && !state_->busy) {
                state_->requested=world->epoch;state_->busy=state_->queued=true;wake=true;
            }
        }
    }
    if(wake)state_->wake.notify_one();
    if(faulted()){close();retire();return nullptr;}
    if(!world || world->epoch<=floor_ || approved_!=world->epoch){retire();return nullptr;}
    auto previous=bound_;auto *match=manager_->update(world);
    if(!match && previous) {
        floor_=std::max(floor_,previous);
        if(!manager_->error().empty()){error_=manager_->error();close();}
    }else if(!match && !manager_->error().empty()) {
        error_=manager_->error();close();
    }
    bound_=match?world->epoch:0;return match;
}
void ScriptRuntime::close() noexcept {
    if(manager_)manager_->close();
    if(state_){state_->closing.store(true);state_->wake.notify_one();}
}
bool ScriptRuntime::stopped()const noexcept {
    return (!state_ || state_->done.load()) && (!manager_ || manager_->stopped());
}
ScriptRuntime::~ScriptRuntime() {
    close();if(thread_.joinable())thread_.join();
}
}}
