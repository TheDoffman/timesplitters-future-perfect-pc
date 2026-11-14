#include "mod_game_runtime.h"
#include "mod_saves.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
using Clock=std::chrono::steady_clock;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Factory line%d: %s\n",__LINE__,#x);return 3;}}while(0)
static std::unique_ptr<tsfp::mods::SaveNamespace> lease;
static std::string source;
static std::atomic<bool> verified{true};
static std::atomic<unsigned> probes{0};
static bool requested=true,network_locked=false;
static bool local_cache(const std::string &value) {
#ifdef _WIN32
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,nullptr,0);
    if(count<=0)return false;
    std::wstring wide((size_t)count,L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),-1,wide.data(),count))return false;
    return SetEnvironmentVariableW(L"TSFP_MOD_SCRIPT_CACHE_PARENT",wide.c_str())!=FALSE;
#else
    return setenv("TSFP_MOD_SCRIPT_CACHE_PARENT",value.c_str(),1)==0;
#endif
}
extern "C" const char *pc_mod_bootstrap_source(size_t *length) {
    bool ready=requested && verified.load();if(length)*length=ready?source.size():0;return ready?source.data():nullptr;
}
extern "C" int pc_mod_bootstrap_probe(void) {
    ++probes;std::string error;return verified.load() && lease && lease->recheck(error);
}
extern "C" void pc_mod_bootstrap_invalidate(void){verified.store(false);}
extern "C" void pc_network_lock_offline(void){network_locked=true;}
struct Shutdown {~Shutdown(){if(!pc_mod_runtime_shutdown())std::abort();}};
static int run(const std::string &mode,const std::string &cache,const std::string &original) {
    std::vector<unsigned char> ballast;
    if(mode=="large-parent") {
        ballast.resize(256u*1024*1024);
        volatile unsigned char *pages=ballast.data();
        for(size_t i=0;i<ballast.size();i+=4096)pages[i]=0x5a;
    }
    source="return {api=1,on_start=function() game.set_score(1,4) end}";
    if(mode=="fault")source="return {api=1,on_start=function() error('fixture mode stopped') end}";
    if(mode=="ordinary") {
        requested=false;CHECK(pc_mod_runtime_prepare(cache.c_str()));CHECK(!pc_mod_runtime_selected() && !network_locked);
        CHECK(pc_mod_runtime_shutdown());std::puts("PASS ordinary");return 0;
    }
    std::string error;lease=tsfp::mods::SaveNamespace::open(original,std::string(64,'a'),error);CHECK(lease);
    bool from_environment=mode=="cache-env" || mode=="cache-missing" || mode=="cache-empty";
    if(from_environment)CHECK(local_cache(mode=="cache-missing"?cache+"/absent":mode=="cache-empty"?"":cache));
    int prepared=pc_mod_runtime_prepare(from_environment?nullptr:cache.c_str());Shutdown shutdown;
    CHECK(pc_mod_runtime_selected() && network_locked);
    if(mode=="refused" || mode=="cache-missing" || mode=="cache-empty"){CHECK(!prepared && pc_mod_runtime_faulted() && !verified.load());std::puts("PASS refused");return 0;}
    CHECK(prepared && !pc_mod_runtime_faulted());
    char overlay[192];CHECK(pc_mod_runtime_overlay(overlay,sizeof overlay) && std::strstr(overlay,"Offline Arcade"));
    const char label[]="100% ready \xce\xa9";
    pc_mod_runtime_message(label,sizeof(label)-1);CHECK(pc_mod_runtime_overlay(overlay,sizeof overlay) && std::strstr(overlay,label));
    pc_mod_runtime_message("bad\nmessage",11);CHECK(pc_mod_runtime_overlay(overlay,sizeof overlay) && std::strstr(overlay,label));
    if(mode=="repeat"){CHECK(!pc_mod_runtime_prepare(cache.c_str()) && pc_mod_runtime_faulted());std::puts("PASS repeat");return 0;}
    if(mode=="tamper")std::ofstream(std::filesystem::u8path(lease->root())/".tsfp-mod-saves")<<"changed marker";
    TsfpModWorld world{};world.epoch=1;world.authoritative=world.offline=1;world.tick_rate=60;world.player_count=1;
    world.players[0]={1,0,0,100,0,0,0};world.identities[0]=7;
    if(mode=="results") {
        for(unsigned i=0;i<20;++i){CHECK(!pc_mod_runtime_update(&world,0));std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        CHECK(!probes.load());++world.epoch;
    }
    auto end=Clock::now()+std::chrono::seconds(4);TsfpModMatch *match=nullptr;
    while(!match && !pc_mod_runtime_faulted() && Clock::now()<end) {
        match=pc_mod_runtime_update(&world,1);if(!match)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(mode=="tamper") {
        CHECK(!match && pc_mod_runtime_faulted() && !verified.load());++world.epoch;CHECK(!pc_mod_runtime_update(&world,1));
        std::puts("PASS tamper");return 0;
    }
    CHECK(match && probes.load()==1);
    int status=0;end=Clock::now()+std::chrono::seconds(3);
    while(!status && Clock::now()<end){TsfpModCommit commit{};status=tsfp_mod_match_pump(match,&world,&commit);if(!status)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    if(mode=="fault") {
        CHECK(status==-1);CHECK(!pc_mod_runtime_update(&world,1));CHECK(pc_mod_runtime_faulted() && !verified.load());
        CHECK(*pc_mod_runtime_error());std::puts("PASS fault");return 0;
    }
    CHECK(status==1);int32_t score=0;CHECK(tsfp_mod_match_score(match,&world,1,&score) && score==4);
    CHECK(pc_mod_runtime_update(&world,0)==match); // Existing result model retained.
    if(mode=="other-thread") {
        int cleaned=1;std::thread other([&]{cleaned=pc_mod_runtime_shutdown();});other.join();CHECK(!cleaned);
        CHECK(!pc_mod_runtime_update(&world,1));
    }
    std::puts("PASS factory");return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=4)return 2;std::string args[3];
    for(int i=0;i<3;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;
        args[i].resize((size_t)n);if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i+1],-1,args[i].data(),n,nullptr,nullptr))return 2;args[i].pop_back();}
    return run(args[0],args[1],args[2]);
}
#else
int main(int argc,char **argv){return argc==4?run(argv[1],argv[2],argv[3]):2;}
#endif
