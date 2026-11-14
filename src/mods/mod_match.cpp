#include "mod_match.h"
#include "mod_session.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

using tsfp::mods::Session;
using tsfp::mods::SessionStatus;
using tsfp::mods::Submission;
namespace {
bool valid(const TsfpModWorld &w) {
    if(!w.epoch || !w.authoritative || !w.offline || !w.player_count || w.player_count>32 || !w.tick_rate || w.tick_rate>1000 || w.tick>TSFP_SCRIPT_INTEGER_MAX)return false;
    for(uint32_t i=0;i<w.player_count;++i) {
        const auto &p=w.players[i];
        if(!p.id || p.id>32 || !w.identities[i] || p.team>8 || p.score < -1000000 || p.score>1000000 || p.health<0 || p.health>1000000 ||
            !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || std::fabs(p.x)>10000000 || std::fabs(p.y)>10000000 || std::fabs(p.z)>10000000)return false;
        for(uint32_t j=0;j<i;++j)if(w.players[j].id==p.id || w.identities[j]==w.identities[i])return false;
    }
    return true;
}
bool same_roster(const TsfpModWorld &a,const TsfpModWorld &b) {
    if(a.epoch!=b.epoch || a.player_count!=b.player_count || a.tick_rate!=b.tick_rate)return false;
    for(uint32_t i=0;i<a.player_count;++i) {
        bool found=false;
        for(uint32_t j=0;j<b.player_count;++j)if(a.players[i].id==b.players[j].id && a.identities[i]==b.identities[j])found=true;
        if(!found)return false;
    }
    return true;
}
bool has(const TsfpModWorld &w,uint32_t id){for(uint32_t i=0;i<w.player_count;++i)if(w.players[i].id==id)return true;return false;}
}
struct TsfpModMatch {
    std::unique_ptr<Session> session;
    TsfpModWorld initial{},pending_world{};
    struct Item {TsfpModWorld world;uint32_t kind,actor,target;};
    std::array<Item,TSFP_MOD_EVENT_QUEUE> queue{};
    size_t head=0,count=0;uint64_t sequence=0,last_tick=0,last_tick_event=0;
    bool pending=false,fault=false,closing=false,stop_queued=false,tick_seen=false,ended=false;
    uint32_t winner=0;
    std::array<int32_t,32> scores{};char error[256]{};
    int fail(const char *message) {
        fault=true;count=0;pending=false;
        size_t size=std::min(std::strlen(message),sizeof(error)-1);std::memcpy(error,message,size);error[size]=0;
        if(session)session->close();
        return -1;
    }
    bool world_ok(const TsfpModWorld &world)const{return valid(world) && same_roster(initial,world) && world.tick>=last_tick;}
};
extern "C" TsfpModMatch *tsfp_mod_match_open(const TsfpModMatchConfig *config,const TsfpModWorld *world) {
    constexpr uint32_t required=TSFP_MOD_IMAGES_VERIFIED|TSFP_MOD_SOURCE_VERIFIED|TSFP_MOD_SEPARATE_SAVES|TSFP_MOD_OFFLINE;
    if(!config || !world || !valid(*world) || config->verified!=required || !config->worker || !config->cache || !config->source ||
        !config->source_size || config->source_size>TSFP_SCRIPT_SOURCE_MAX)return nullptr;
    try {
        auto match=std::make_unique<TsfpModMatch>();std::string error;
        match->session=Session::open(config->worker,config->cache,std::string(config->source,config->source_size),config->seed,error);
        if(!match->session)return nullptr;
        match->initial=*world;match->last_tick=world->tick;
        for(uint32_t i=0;i<world->player_count;++i)match->scores[world->players[i].id-1]=world->players[i].score;
        match->queue[0]={*world,TSFP_SCRIPT_START,0,0};match->count=1;
        return match.release();
    }catch(...){return nullptr;}
}
extern "C" int tsfp_mod_match_enqueue(TsfpModMatch *match,const TsfpModWorld *world,uint32_t kind,uint32_t actor,uint32_t target) {
    if(!match || match->fault || match->closing)return 0;
    if(!world || !match->world_ok(*world) || kind<TSFP_SCRIPT_TICK || kind>TSFP_SCRIPT_STOP || match->stop_queued ||
        (match->ended && kind!=TSFP_SCRIPT_STOP) || (actor && !has(*world,actor)) || (target && !has(*world,target)) ||
        (kind==TSFP_SCRIPT_DEATH && !target) || (kind==TSFP_SCRIPT_TICK && match->tick_seen && world->tick<=match->last_tick_event))return match->fail("Invalid or stale title event");
    if(kind==TSFP_SCRIPT_TICK && match->count) {
        auto &tail=match->queue[(match->head+match->count-1)%match->queue.size()];
        if(tail.kind==TSFP_SCRIPT_TICK){tail={*world,kind,actor,target};match->last_tick=world->tick;match->last_tick_event=world->tick;match->tick_seen=true;return 1;}
    }
    if(match->count==match->queue.size())return match->fail("Script event queue exhausted");
    match->queue[(match->head+match->count)%match->queue.size()]={*world,kind,actor,target};++match->count;
    match->last_tick=world->tick;
    if(kind==TSFP_SCRIPT_TICK){match->tick_seen=true;match->last_tick_event=world->tick;}
    if(kind==TSFP_SCRIPT_STOP)match->stop_queued=true;
    return 1;
}
extern "C" int tsfp_mod_match_pump(TsfpModMatch *match,const TsfpModWorld *world,TsfpModCommit *commit) {
    if(commit)std::memset(commit,0,sizeof(*commit));
    if(!match || !commit || match->fault || match->closing)return -1;
    try {
        if(!world || !match->world_ok(*world))return match->fail("Match authority, epoch or roster changed");
        tsfp_script_result result{};uint64_t sequence=0;std::string error;
        auto status=match->session->poll(result,sequence,error);
        if(status==SessionStatus::Faulted)return match->fail(error.empty()?"Script session failed":error.c_str());
        int published=0;
        if(status==SessionStatus::Reply) {
            if(!match->pending || sequence!=match->sequence || !same_roster(match->pending_world,*world))return match->fail("Stale script transaction");
            auto next_scores=match->scores;bool next_ended=match->ended;uint32_t next_winner=match->winner;
            for(uint32_t i=0;i<result.count;++i) {
                const auto &c=result.commands[i];
                if(c.kind==TSFP_SCRIPT_SCORE) {
                    if(next_ended || !has(*world,c.player))return match->fail("Score target changed");
                    next_scores[c.player-1]=c.value;
                }else if(c.kind==TSFP_SCRIPT_END) {
                    if(next_ended || (c.player && !has(*world,c.player)))return match->fail("Winner target changed");
                    next_ended=true;next_winner=c.player;
                }else if(c.kind!=TSFP_SCRIPT_MESSAGE)return match->fail("Unknown game transaction");
            }
            match->scores=next_scores;match->ended=next_ended;match->winner=next_winner;match->pending=false;
            commit->epoch=world->epoch;commit->sequence=sequence;commit->transaction=result;published=1;
            // No more queued gameplay after End: preserve a queued Stop only.
            if(next_ended && match->count) {
                auto stop=match->queue[(match->head+match->count-1)%match->queue.size()];
                match->head=0;match->count=stop.kind==TSFP_SCRIPT_STOP?1:0;
                if(match->count)match->queue[0]=stop;
            }
        }
        if(published)return 1;
        if(!match->pending && match->count && !match->session->stopped()) {
            const auto &item=match->queue[match->head];tsfp_script_event event{};
            event.sequence=match->sequence+1;event.kind=item.kind;event.tick=item.world.tick;event.tick_rate=item.world.tick_rate;
            event.actor=item.actor;event.target=item.target;event.player_count=item.world.player_count;
            std::memcpy(event.players,item.world.players,sizeof(event.players));
            for(uint32_t i=0;i<event.player_count;++i)event.players[i].score=match->scores[event.players[i].id-1];
            auto accepted=match->session->dispatch(event);
            if(accepted==Submission::Accepted) {
                match->pending=true;match->pending_world=item.world;match->sequence=event.sequence;
                match->head=(match->head+1)%match->queue.size();--match->count;
            }else if(accepted!=Submission::Busy) {
                match->fail("Script event dispatch failed");
                return -1;
            }
        }
        return published;
    }catch(...){std::memset(commit,0,sizeof(*commit));return match->fail("Game script bridge failure");}
}
extern "C" int tsfp_mod_match_matches(const TsfpModMatch *match,const TsfpModWorld *world) {
    return match && !match->fault && !match->closing && world && match->world_ok(*world);
}
extern "C" int tsfp_mod_match_score(const TsfpModMatch *match,const TsfpModWorld *world,uint32_t id,int32_t *score) {
    if(!tsfp_mod_match_matches(match,world) || !score || !has(match->initial,id))return 0;
    *score=match->scores[id-1];return 1;
}
extern "C" int tsfp_mod_match_ranking(const TsfpModMatch *match,const TsfpModWorld *world,int32_t ranks[32],uint32_t order[32]) {
    if(!tsfp_mod_match_matches(match,world) || !ranks || !order)return 0;
    std::fill(ranks,ranks+32,INT32_MIN+1);std::fill(order,order+32,0);
    std::array<uint32_t,32> ids{};size_t count=match->initial.player_count;
    for(size_t i=0;i<count;++i)ids[i]=match->initial.players[i].id;
    std::sort(ids.begin(),ids.begin()+count,[&](uint32_t a,uint32_t b){
        if(a==b)return false;
        if(match->ended && match->winner && (a==match->winner || b==match->winner))return a==match->winner;
        return match->scores[a-1]!=match->scores[b-1]?match->scores[a-1]>match->scores[b-1]:a<b;
    });
    for(size_t i=0;i<count;++i) {
        uint32_t id=ids[i];order[i]=id;
        if(match->ended && !match->winner)ranks[id-1]=1;
        else if(match->ended && id==match->winner)ranks[id-1]=1;
        else {
            int32_t rank=match->ended?2:1;
            for(size_t j=0;j<count;++j)if(ids[j]!=match->winner && match->scores[ids[j]-1]>match->scores[id-1])++rank;
            ranks[id-1]=rank;
        }
    }
    return 1;
}
extern "C" int tsfp_mod_match_ended(const TsfpModMatch *match,const TsfpModWorld *world,uint32_t *winner) {
    if(!tsfp_mod_match_matches(match,world) || !match->ended)return 0;
    if(winner)*winner=match->winner;
    return 1;
}
extern "C" int tsfp_mod_match_faulted(const TsfpModMatch *match){return !match || match->fault;}
extern "C" const char *tsfp_mod_match_error(const TsfpModMatch *match){return match?match->error:"No game script match";}
extern "C" void tsfp_mod_match_close(TsfpModMatch *match){if(match){match->closing=true;match->count=0;match->session->close();}}
extern "C" int tsfp_mod_match_stopped(const TsfpModMatch *match){return !match || match->session->stopped();}
extern "C" void tsfp_mod_match_destroy(TsfpModMatch *match){if(match){match->session->close();delete match;}}
