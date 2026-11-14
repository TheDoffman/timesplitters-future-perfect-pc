#include "mod_match.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <string>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif
using Clock=std::chrono::steady_clock;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Match check failed at line %d: %s\n",__LINE__,#x);return 3;}}while(0)
static int run(const std::string &image,const std::string &cache,const std::string &scenario,const std::string &worker_hash,const std::string &guardian_hash) {
    std::string source;char block[4096];size_t size;
    while((size=fread(block,1,sizeof(block),stdin))!=0){source.append(block,size);if(source.size()>1024u*1024)return 2;}
    TsfpModWorld world{};world.epoch=123;world.authoritative=world.offline=1;world.tick_rate=60;world.player_count=2;
    world.players[0]={1,0,0,100,0,0,0};world.players[1]={2,1,0,100,1,2,3};world.identities[0]=71;world.identities[1]=72;
    TsfpModMatchConfig config{image.c_str(),cache.c_str(),source.data(),source.size(),123,15,worker_hash.c_str(),guardian_hash=="-"?"":guardian_hash.c_str()};
    if(scenario=="policy")config.verified=14;
    if(scenario=="image-policy")config.worker_sha256=nullptr;
    if(scenario=="image-digest")config.worker_sha256="0000000000000000000000000000000000000000000000000000000000000000";
    if(scenario=="initial-world")world.identities[1]=world.identities[0];
    auto *match=tsfp_mod_match_open(&config,&world);
    if(scenario=="policy" || scenario=="image-policy" || scenario=="initial-world"){CHECK(!match);puts("{\"refused\":true}");return 0;}
    CHECK(match);
    struct Drain {TsfpModMatch *match;~Drain(){tsfp_mod_match_close(match);auto end=Clock::now()+std::chrono::seconds(3);
        while(!tsfp_mod_match_stopped(match) && Clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));tsfp_mod_match_destroy(match);}} drain{match};
    unsigned commits=0,messages=0;TsfpModCommit commit{};
    auto wait=[&] {
        auto end=Clock::now()+std::chrono::seconds(3);
        while(Clock::now()<end) {
            int status=tsfp_mod_match_pump(match,&world,&commit);
            if(status<0)return false;
            if(status>0){++commits;for(uint32_t i=0;i<commit.transaction.count;++i)if(commit.transaction.commands[i].kind==TSFP_SCRIPT_MESSAGE)++messages;return true;}
            if(commit.sequence || commit.epoch || commit.transaction.count)return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    };
    if(scenario=="image-digest") {CHECK(!wait() && tsfp_mod_match_faulted(match) && !commits);puts("{\"invalid\":true}");return 0;}
    if(scenario=="overflow") {
        for(unsigned i=0;i<63;++i)CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_DEATH,1,2)==1);
        CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_DEATH,1,2)==-1);
        CHECK(tsfp_mod_match_faulted(match));puts("{\"overflow\":true}");return 0;
    }
    if(scenario.rfind("stale-",0)==0) {
        CHECK(tsfp_mod_match_pump(match,&world,&commit)==0); // Start in flight.
        if(scenario=="stale-epoch")++world.epoch;
        if(scenario=="stale-roster")++world.identities[0];
        if(scenario=="stale-authority")world.authoritative=0;
        if(scenario=="stale-network")world.offline=0;
        std::memset(&commit,0x7f,sizeof(commit));
        CHECK(tsfp_mod_match_pump(match,&world,&commit)==-1);
        CHECK(!commit.epoch && !commit.sequence && !commit.transaction.count);
        int32_t score=0;CHECK(!tsfp_mod_match_score(match,&world,1,&score));puts("{\"stale\":true}");return 0;
    }
    if(scenario=="coalesce") {
        for(unsigned i=1;i<=100;++i){world.tick=i;CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_TICK,0,0)==1);}
        CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_DEATH,1,2)==1);
        for(unsigned i=101;i<=200;++i){world.tick=i;CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_TICK,0,0)==1);}
        world.tick=201;CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_STOP,0,0)==1);
        for(unsigned i=0;i<5;++i)CHECK(wait());
        CHECK(commits==5 && messages==3);puts("{\"coalesced\":true,\"commits\":5,\"messages\":3}");return 0;
    }
    CHECK(wait());
    if(scenario=="reordered") {std::swap(world.players[0],world.players[1]);std::swap(world.identities[0],world.identities[1]);}
    if(scenario=="health") {world.players[0].health=0;world.players[0].x=100;}
    if(scenario=="invalid-target") {
        CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_DEATH,1,3)==-1);
        CHECK(tsfp_mod_match_faulted(match));puts("{\"invalid\":true}");return 0;
    }
    unsigned deaths=scenario=="normal"?3:1;
    for(unsigned i=0;i<deaths;++i){world.tick=i+1;CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_DEATH,1,2)==1);CHECK(wait());}
    int32_t score=0;CHECK(tsfp_mod_match_score(match,&world,1,&score));
    uint32_t winner=99;int ended=tsfp_mod_match_ended(match,&world,&winner);
    int32_t ranks[32];uint32_t order[32];CHECK(tsfp_mod_match_ranking(match,&world,ranks,order));
    if(scenario=="normal"){CHECK(score==3 && ended && winner==1 && ranks[0]==1 && ranks[1]==2 && order[0]==1);}
    if(scenario=="winner"){CHECK(ended && winner==2 && ranks[1]==1 && ranks[0]==2 && order[0]==2);}
    if(scenario=="draw"){CHECK(ended && winner==0 && ranks[0]==1 && ranks[1]==1);}
    world.tick=deaths+1;CHECK(tsfp_mod_match_enqueue(match,&world,TSFP_SCRIPT_STOP,0,0)==1);CHECK(wait());
    printf("{\"score\":%d,\"ended\":%s,\"winner\":%u,\"commits\":%u,\"rank1\":%d,\"rank2\":%d}\n",score,ended?"true":"false",winner,commits,ranks[0],ranks[1]);
    return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=6 || _setmode(_fileno(stdin),_O_BINARY)<0)return 2;
    auto utf8=[](const wchar_t *text){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);if(n<=0)return std::string();std::string out((size_t)n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,&out[0],n,nullptr,nullptr);out.pop_back();return out;};
    return run(utf8(argv[1]),utf8(argv[2]),utf8(argv[3]),utf8(argv[4]),utf8(argv[5]));
}
#else
int main(int argc,char **argv){return argc==6?run(argv[1],argv[2],argv[3],argv[4],argv[5]):2;}
#endif
