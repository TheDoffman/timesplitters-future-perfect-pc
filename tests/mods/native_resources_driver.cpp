#include "mod_resources.hpp"
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
static void record(const char *key,bool accepted,const std::string &root={}) {
    std::printf("{\"%s\":%s,\"root_hex\":\"",key,accepted?"true":"false");
    for(unsigned char c:root)std::printf("%02x",c);std::puts("\"}");std::fflush(stdout);
}
static int run(const std::string &mode,const std::string &parent,const std::string &subdir,const std::string &pin,const std::string &guardian) {
    std::string error;
    if(mode=="parent"){record("accepted",true,tsfp::mods::native_cache_parent());return 0;}
    if(mode=="private" || mode=="private-hold") {
        auto cache=tsfp::mods::PrivateDirectory::create(parent,error);record("accepted",!!cache,cache?cache->root():"");
        if(cache && mode=="private-hold"){(void)std::getchar();record("rechecked",cache->recheck(error),cache->root());}
        return 0;
    }
#ifdef _WIN32
    const char *name="tsfp-mod-worker.exe";
#else
    const char *name="tsfp-mod-worker";
#endif
    tsfp::mods::ImagePins pins{pin,guardian=="-"?"":guardian};
    auto resources=tsfp::mods::RuntimeResources::create(subdir,name,pins,parent,error);
    record("accepted",!!resources,resources?resources->cache():"");if(!resources)return 0;
    if(mode=="hold"){(void)std::getchar();record("rechecked",resources->recheck(error),resources->cache());return 0;}
    if(mode!="run")return 0;
    const char source[]="return {api=1,on_start=function() game.set_score(1,4) end}";
    TsfpModMatchConfig config{resources->worker().c_str(),resources->cache().c_str(),source,sizeof(source)-1,17,15,pin.c_str(),pins.guardian.c_str()};
    auto manager=tsfp::mods::MatchManager::create(config,error);if(!manager)return 3;
    TsfpModWorld world{};world.epoch=1;world.authoritative=world.offline=1;world.tick_rate=60;world.player_count=1;
    world.players[0]={1,0,0,100,0,0,0};world.identities[0]=9;
    auto *match=manager->update(&world);if(!match)return 3;
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);bool accepted=false;
    while(std::chrono::steady_clock::now()<end) {
        TsfpModCommit commit{};int status=tsfp_mod_match_pump(match,&world,&commit);
        if(status<0)break;
        if(status>0){int32_t score=0;accepted=tsfp_mod_match_score(match,&world,1,&score) && score==4;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    manager->close();end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!manager->stopped() && std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    bool stopped=manager->stopped();if(!stopped)std::abort();manager.reset();
    record("ran",accepted && stopped);return accepted && stopped?0:3;
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
