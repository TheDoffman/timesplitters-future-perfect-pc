#include "mod_manager.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
using Clock=std::chrono::steady_clock;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Manager line%d: %s\n",__LINE__,#x);return 3;}}while(0)
static void pause_short(){std::this_thread::sleep_for(std::chrono::milliseconds(1));}
struct Drain {
    tsfp::mods::MatchManager *manager;
    ~Drain(){manager->close();auto end=Clock::now()+std::chrono::seconds(3);
        while(!manager->stopped() && Clock::now()<end)pause_short();if(!manager->stopped())std::abort();}
};
static int run(const std::string &image,const std::string &cache,const std::string &scenario,const std::string &worker_hash,const std::string &guardian_hash) {
    std::string source="local n=0; return {api=1,on_start=function() n=n+1;assert(n==1);game.set_score(1,4) end}";
    if(scenario=="fault")source="return {api=1,on_start=function() error('mode failed') end}";
    if(scenario=="in-flight")source="return {api=1,on_start=function(e) if e.tick==3 then while true do end end;game.set_score(1,4) end}";
    TsfpModMatchConfig config{image.c_str(),cache.c_str(),source.data(),source.size(),11,15,worker_hash.c_str(),guardian_hash=="-"?"":guardian_hash.c_str()};
    if(scenario=="policy")config.verified=14;
    if(scenario=="pin")config.worker_sha256="invalid";
    std::string error;auto manager=tsfp::mods::MatchManager::create(config,error);
    if(scenario=="policy" || scenario=="pin"){CHECK(!manager && !error.empty());std::puts("PASS refused");return 0;}
    CHECK(manager && error.empty());Drain drain{manager.get()};
    // Configuration must retain its own immutable source, independent of a
    // launcher's capsule file or a bootstrap scratch buffer being reused.
    source.assign("this is deliberately invalid Lua after snapshot");
    TsfpModWorld world{};world.epoch=123;world.tick=3;world.authoritative=world.offline=1;world.tick_rate=60;world.player_count=2;
    world.players[0]={1,0,0,100,0,0,0};world.players[1]={2,0,0,100,1,2,3};world.identities[0]=71;world.identities[1]=72;
    auto *match=manager->update(&world);CHECK(match && manager->update(&world)==match);
    auto wait=[&](TsfpModMatch *active) {
        auto end=Clock::now()+std::chrono::seconds(3);TsfpModCommit commit{};
        while(Clock::now()<end){int result=tsfp_mod_match_pump(active,&world,&commit);if(result)return result;pause_short();}return 0;
    };
    if(scenario=="fault") {
        CHECK(wait(match)==-1);CHECK(!manager->update(&world));CHECK(!manager->error().empty());
    }else {
        if(scenario=="in-flight") {
            // Cancel while startup/dispatch is live; do not wait for a reply.
            for(unsigned i=0;i<20;++i){TsfpModCommit pending{};tsfp_mod_match_pump(match,&world,&pending);pause_short();}
        }else {
            CHECK(wait(match)==1);int32_t score=0;CHECK(tsfp_mod_match_score(match,&world,1,&score) && score==4);
        }
        if(scenario=="finished") {
            CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_STOP,0,0)==1 && wait(match)==1);
            CHECK(manager->update(&world)==match); // Keep results after reaping.
        }
        auto changed=world;
        if(scenario=="roster")++changed.identities[0];
        if(scenario=="authority")changed.authoritative=0;
        if(scenario=="network")changed.offline=0;
        if(scenario=="tick")changed.tick=2;
        bool successor=scenario=="new-epoch" || scenario=="in-flight";
        if(successor)++changed.epoch;
        if(scenario=="in-flight")++changed.tick;
        if(scenario=="close")manager->close();
        CHECK(!manager->update(scenario=="leave" || scenario=="finished"?nullptr:&changed) || successor);
        if(successor) {
            // update may already have opened the successor if cleanup finished.
            world=changed;auto end=Clock::now()+std::chrono::seconds(3);
            do{match=manager->update(&world);if(!match)pause_short();}while(!match && Clock::now()<end);
            CHECK(match && wait(match)==1);std::puts("PASS successor");return 0;
        }
    }
    auto end=Clock::now()+std::chrono::seconds(3);
    while(!manager->stopped() && Clock::now()<end){CHECK(!manager->update(&world));pause_short();}
    CHECK(manager->stopped() && !manager->update(&world));
    // Restoring flags/roster cannot reopen this invalidated epoch.
    for(unsigned i=0;i<5;++i)CHECK(!manager->update(&world));
    ++world.epoch;
    if(scenario=="close"){CHECK(!manager->update(&world));std::puts("PASS closed");return 0;}
    if(scenario=="fault"){std::puts("PASS fault");return 0;}
    match=manager->update(&world);CHECK(match && wait(match)==1);
    int32_t score=0;CHECK(tsfp_mod_match_score(match,&world,1,&score) && score==4);
    std::puts("PASS lifecycle");return 0;
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
