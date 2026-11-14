#include "mod_runtime.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
using Clock=std::chrono::steady_clock;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Runtime line%d: %s\n",__LINE__,#x);return 3;}}while(0)
static void pause_short(){std::this_thread::sleep_for(std::chrono::milliseconds(1));}
struct Drain {
    tsfp::mods::ScriptRuntime *runtime;std::atomic<bool> &release;
    ~Drain(){release.store(true);runtime->close();auto end=Clock::now()+std::chrono::seconds(3);
        while(!runtime->stopped() && Clock::now()<end)pause_short();if(!runtime->stopped())std::abort();}
};
static int run(const std::string &image,const std::string &cache,const std::string &mode,const std::string &pin,const std::string &guardian) {
    std::string source="return {api=1,on_start=function() game.set_score(1,4) end}";
    if(mode=="callback-fail")source="return {api=1,on_start=function() error('mode failed') end}";
    TsfpModMatchConfig config{image.c_str(),cache.c_str(),source.data(),source.size(),19,15,pin.c_str(),guardian=="-"?"":guardian.c_str()};
    std::atomic<bool> release{false},entered{false},on_main{false};std::atomic<unsigned> calls{0};auto main=std::this_thread::get_id();
    auto probe=[&] {
        if(std::this_thread::get_id()==main)on_main.store(true);
        ++calls;entered.store(true);
        while(!release.load())pause_short();
        if(mode=="probe-throw")throw std::runtime_error("host fixture failure");
        return mode!="probe-fail";
    };
    std::string error;auto runtime=tsfp::mods::ScriptRuntime::create(config,probe,error);CHECK(runtime && error.empty());
    Drain drain{runtime.get(),release};
    TsfpModWorld world{};world.epoch=1;world.authoritative=world.offline=1;world.tick_rate=60;world.player_count=1;
    world.players[0]={1,0,0,100,0,0,0};world.identities[0]=91;
    long long maximum=0;
    auto update=[&](const TsfpModWorld *w) {
        auto start=Clock::now();auto *match=runtime->update(w);
        auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
        if(elapsed>maximum)maximum=elapsed;return match;
    };
    CHECK(!update(&world));auto end=Clock::now()+std::chrono::seconds(3);
    while(!entered.load() && Clock::now()<end){CHECK(!update(&world));pause_short();}
    CHECK(entered.load() && !on_main.load() && calls.load()==1);
    // Holding a deliberately blocked filesystem collaborator cannot block the
    // simulation API, publish a match or create a second concurrent probe.
    for(unsigned i=0;i<20;++i)CHECK(!update(&world));
    CHECK(!runtime->stopped() && calls.load()==1);
    if(mode=="close-pending") {
        runtime->close();CHECK(!update(&world) && !runtime->faulted());release.store(true);
        std::puts("PASS close pending");return 0;
    }
    if(mode=="stale-preflight"){CHECK(!update(nullptr));}
    if(mode=="old-epoch"){++world.epoch;CHECK(!update(&world));--world.epoch;CHECK(!update(&world));}
    release.store(true);
    if(mode=="probe-fail" || mode=="probe-throw") {
        end=Clock::now()+std::chrono::seconds(3);
        while(!runtime->faulted() && Clock::now()<end){CHECK(!update(&world));pause_short();}
        CHECK(runtime->faulted() && !runtime->error().empty());++world.epoch;CHECK(!update(&world));
        std::puts("PASS probe refusal");return 0;
    }
    if(mode=="stale-preflight" || mode=="old-epoch") {
        for(unsigned i=0;i<20;++i){CHECK(!update(&world));pause_short();}
        world.epoch=mode=="old-epoch"?3:2;
    }
    TsfpModMatch *match=nullptr;end=Clock::now()+std::chrono::seconds(3);
    while(!match && Clock::now()<end){match=update(&world);if(!match)pause_short();}
    CHECK(match && !runtime->faulted());
    auto wait=[&] {
        auto until=Clock::now()+std::chrono::seconds(3);
        while(Clock::now()<until){TsfpModCommit commit{};int status=tsfp_mod_match_pump(match,&world,&commit);if(status)return status;pause_short();}return 0;
    };
    if(mode=="callback-fail") {
        CHECK(wait()==-1);CHECK(!update(&world) && runtime->faulted());++world.epoch;CHECK(!update(&world));
    }else {
        CHECK(wait()==1);int32_t score=0;CHECK(tsfp_mod_match_score(match,&world,1,&score) && score==4);
        auto initial=world;
        bool unsupported_fresh=mode=="network-fresh" || mode=="authority-fresh";
        if(unsupported_fresh){++world.epoch;initial=world;}
        if(mode=="roster")++world.identities[0];
        else if(mode=="network" || mode=="network-fresh")world.offline=0;
        else if(mode=="authority" || mode=="authority-fresh")world.authoritative=0;
        else CHECK(!update(nullptr));
        CHECK(!update(&world));world=initial;
        for(unsigned i=0;i<10;++i){CHECK(!update(&world));pause_short();}
        CHECK(!runtime->faulted());++world.epoch;match=nullptr;end=Clock::now()+std::chrono::seconds(3);
        while(!match && Clock::now()<end){match=update(&world);if(!match)pause_short();}
        CHECK(match && wait()==1 && calls.load()>=2);
        if(unsupported_fresh)CHECK(calls.load()==2);
    }
    CHECK(!on_main.load() && maximum<100000);
    std::printf("PASS runtime max_api_us=%lld\n",maximum);return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=6)return 2;std::string args[5];
    for(int i=0;i<5;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;
        args[i].resize((size_t)n);if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,args[i].data(),n,nullptr,nullptr))return 2;args[i].pop_back();}
    return run(args[0],args[1],args[2],args[3],args[4]);
}
#else
int main(int argc,char **argv){return argc==6?run(argv[1],argv[2],argv[3],argv[4],argv[5]):2;}
#endif
