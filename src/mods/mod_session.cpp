#include "mod_session.hpp"
#include "mod_process.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <stdexcept>

namespace tsfp { namespace mods {
namespace {
using Clock=std::chrono::steady_clock;
constexpr size_t reply_max=32u+TSFP_SCRIPT_COMMAND_MAX*(20u+TSFP_SCRIPT_TEXT_MAX);
bool player(const tsfp_script_event &event,uint32_t id) {
    for(uint32_t i=0;i<event.player_count;++i)if(event.players[i].id==id)return true;
    return false;
}
bool text_ok(const unsigned char *text,size_t length) {
    size_t at=0;
    while(at<length) {
        uint32_t code=text[at++],minimum=0;unsigned more=0;
        if(code<128){if(code<32 || code==127)return false;continue;}
        if(code>=0xc2 && code<=0xdf){code&=31;more=1;minimum=0x80;}
        else if(code>=0xe0 && code<=0xef){code&=15;more=2;minimum=0x800;}
        else if(code>=0xf0 && code<=0xf4){code&=7;more=3;minimum=0x10000;}
        else return false;
        if(length-at<more)return false;
        while(more--){unsigned byte=text[at++];if((byte&0xc0)!=0x80)return false;code=(code<<6)|(byte&63);}
        if(code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff) || (code>=0x80 && code<=0x9f))return false;
    }
    return true;
}
struct Reader {
    const unsigned char *data;size_t size,at=0;bool valid=true;
    uint32_t u32() {
        if(size-at<4){valid=false;return 0;}
        uint32_t n=uint32_t(data[at])|uint32_t(data[at+1])<<8|uint32_t(data[at+2])<<16|uint32_t(data[at+3])<<24;at+=4;return n;
    }
    uint64_t u64(){uint64_t low=u32(),high=u32();return low|(high<<32);}
    int32_t i32(){uint32_t n=u32();int32_t result;std::memcpy(&result,&n,4);return result;}
};
void put32(std::vector<unsigned char> &data,uint32_t n){for(unsigned i=0;i<4;++i)data.push_back((unsigned char)(n>>(8*i)));}
void put64(std::vector<unsigned char> &data,uint64_t n){put32(data,(uint32_t)n);put32(data,(uint32_t)(n>>32));}
void frame(std::vector<unsigned char> &data) {
    uint32_t size=(uint32_t)data.size()-4;
    for(unsigned i=0;i<4;++i)data[i]=(unsigned char)(size>>(8*i));
}
std::vector<unsigned char> encode(const tsfp_script_event &event) {
    std::vector<unsigned char> data(4);data.reserve(48+28*event.player_count);
    put32(data,2);put64(data,event.sequence);put32(data,event.kind);put64(data,event.tick);
    put32(data,event.tick_rate);put32(data,event.actor);put32(data,event.target);put32(data,event.player_count);
    for(uint32_t i=0;i<event.player_count;++i) {
        const auto &p=event.players[i];put32(data,p.id);put32(data,p.team);put32(data,(uint32_t)p.score);put32(data,(uint32_t)p.health);
        for(float value:{p.x,p.y,p.z}){uint32_t bits;static_assert(sizeof(value)==4,"IEEE754 float32 required");std::memcpy(&bits,&value,4);put32(data,bits);}
    }
    frame(data);return data;
}
bool decode(const unsigned char *data,size_t size,uint64_t sequence,const tsfp_script_event *event,tsfp_script_result &result,std::string &error) {
    Reader r{data,size};tsfp_script_result pending{};
    uint32_t version=r.u32(),status=r.u32();uint64_t received=r.u64();uint32_t memory=r.u32();pending.count=r.u32();
    if(!r.valid || version!=1 || status>2 || received!=sequence || memory>TSFP_SCRIPT_MEMORY_MAX || pending.count>TSFP_SCRIPT_COMMAND_MAX ||
        (!event && pending.count) || (status && pending.count))return false;
    bool ended=false;
    for(uint32_t i=0;i<pending.count;++i) {
        auto &c=pending.commands[i];c.kind=r.u32();c.player=r.u32();c.value=r.i32();c.text_size=r.u32();
        if(!r.valid || c.text_size>TSFP_SCRIPT_TEXT_MAX || r.size-r.at<c.text_size || !text_ok(r.data+r.at,c.text_size))return false;
        std::memcpy(c.text,r.data+r.at,c.text_size);r.at+=c.text_size;
        if(c.kind==TSFP_SCRIPT_MESSAGE) {if(c.player || c.value || !c.text_size)return false;}
        else {
            if(!event || event->kind==TSFP_SCRIPT_STOP || ended || c.text_size)return false;
            if(c.kind==TSFP_SCRIPT_SCORE) {
                if(!player(*event,c.player) || c.value < -1000000 || c.value>1000000)return false;
            }else if(c.kind==TSFP_SCRIPT_END) {
                if(c.value || (c.player && !player(*event,c.player)))return false;
                ended=true;
            }else return false;
        }
    }
    uint32_t diagnostic=r.u32();
    if(!r.valid || diagnostic>255 || r.size-r.at!=diagnostic || (status==0)!=(diagnostic==0))return false;
    for(size_t i=r.at;i<r.size;++i)if(r.data[i]<32 || r.data[i]>126)return false;
    if(status){error.assign((const char *)r.data+r.at,diagnostic);return false;}
    result=pending;return true;
}
}

struct Session::State {
    std::mutex mutex;
    std::condition_variable wake;
    std::atomic<bool> cancel{false},invalid{false},stopped{false};
    SessionStatus phase=SessionStatus::Loading;bool queued=false,terminal=false;
    bool started=false,ended=false,tick_seen=false;uint64_t sequence=0,tick=0,last_tick=0;
    tsfp_script_event event{};tsfp_script_result result{};std::string error;

    bool valid_event(const tsfp_script_event &e) const {
        if(e.kind<TSFP_SCRIPT_START || e.kind>TSFP_SCRIPT_STOP || e.sequence!=sequence+1 || e.sequence>TSFP_SCRIPT_INTEGER_MAX ||
            e.tick>TSFP_SCRIPT_INTEGER_MAX || e.tick<tick || !e.tick_rate || e.tick_rate>1000 || e.player_count>TSFP_SCRIPT_PLAYER_MAX ||
            (!started && e.kind!=TSFP_SCRIPT_START) || (started && e.kind==TSFP_SCRIPT_START) || (ended && e.kind!=TSFP_SCRIPT_STOP) ||
            (e.kind==TSFP_SCRIPT_TICK && tick_seen && e.tick<=last_tick))return false;
        for(uint32_t i=0;i<e.player_count;++i) {
            const auto &p=e.players[i];
            if(!p.id || p.id>32 || p.team>8 || p.score < -1000000 || p.score>1000000 || p.health<0 || p.health>1000000 ||
                !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || std::fabs(p.x)>10000000 || std::fabs(p.y)>10000000 || std::fabs(p.z)>10000000)return false;
            for(uint32_t j=0;j<i;++j)if(e.players[j].id==p.id)return false;
        }
        return (!e.actor || player(e,e.actor)) && (!e.target || player(e,e.target)) && (e.kind!=TSFP_SCRIPT_DEATH || e.target);
    }
};

std::unique_ptr<Session> Session::open(const std::string &image,const std::string &cache,std::string source,uint32_t seed,std::string &error) {
    if(source.empty() || source.size()>TSFP_SCRIPT_SOURCE_MAX || source[0]=='\x1b' || source.find('\0')!=std::string::npos){error="Invalid script source";return {};}
    try {
        auto state=std::make_shared<State>();auto session=std::unique_ptr<Session>(new Session(state));
        std::thread(work,state,image,cache,std::move(source),seed).detach();return session;
    }catch(const std::exception &){error="Cannot start script session";return {};}
}
Session::~Session(){close();}
void Session::close() noexcept{state_->cancel.store(true);state_->wake.notify_one();}
bool Session::stopped() const noexcept{return state_->stopped.load();}
Submission Session::dispatch(const tsfp_script_event &event) {
    if(state_->cancel.load() || state_->invalid.load())return Submission::Faulted;
    std::unique_lock<std::mutex> lock(state_->mutex,std::try_to_lock);
    if(!lock.owns_lock())return Submission::Busy;
    if(state_->phase==SessionStatus::Faulted || state_->phase==SessionStatus::Finished)return Submission::Faulted;
    if(state_->phase!=SessionStatus::Ready)return Submission::Busy;
    if(!state_->valid_event(event)) {
        state_->invalid.store(true);state_->wake.notify_one();state_->phase=SessionStatus::Faulted;state_->result={};state_->error="Invalid authoritative script event";return Submission::Rejected;
    }
    state_->event=event;state_->queued=true;state_->phase=SessionStatus::Busy;state_->wake.notify_one();return Submission::Accepted;
}
SessionStatus Session::poll(tsfp_script_result &commands,uint64_t &sequence,std::string &error) {
    commands={};sequence=0;error.clear();
    if(state_->cancel.load())return state_->stopped.load()?SessionStatus::Finished:SessionStatus::Closing;
    std::unique_lock<std::mutex> lock(state_->mutex,std::try_to_lock);
    if(!lock.owns_lock())return SessionStatus::Contended;
    if(state_->invalid.load() && state_->stopped.load() && state_->phase!=SessionStatus::Faulted)return SessionStatus::Faulted;
    if(state_->phase==SessionStatus::Reply) {
        commands=state_->result;sequence=state_->sequence;state_->result={};
        state_->phase=state_->terminal?SessionStatus::Finished:SessionStatus::Ready;return SessionStatus::Reply;
    }
    error=state_->error;return state_->phase;
}

void Session::work(std::shared_ptr<State> state,std::string image,std::string cache,std::string source,uint32_t seed) noexcept {
    std::unique_ptr<Process> child;std::string failure;
    try {
        child=Process::start(image,cache,failure);
        if(!child)throw std::runtime_error("Cannot start confined script worker");
        auto rate_start=Clock::now();ProcessUsage usage{};
        if(!child->usage(usage))throw std::runtime_error("Cannot inspect script worker");
        uint64_t rate_cpu=usage.cpu_microseconds,idle_cpu=usage.cpu_microseconds;
        auto inspect=[&](uint64_t cpu_base,uint64_t budget) {
            if(state->cancel.load() || state->invalid.load())throw std::runtime_error("Script session cancelled");
            if(!child->usage(usage) || usage.exited)throw std::runtime_error("Script worker terminated");
            if(usage.resident_bytes>128u*1024*1024 || usage.cpu_microseconds<cpu_base || usage.cpu_microseconds-cpu_base>budget)
                throw std::runtime_error("Script worker resource budget exceeded");
            if(usage.cpu_microseconds<rate_cpu || usage.cpu_microseconds-rate_cpu>1000000)throw std::runtime_error("Script session CPU rate exceeded");
            if(Clock::now()-rate_start>=std::chrono::seconds(10)){rate_start=Clock::now();rate_cpu=usage.cpu_microseconds;}
            unsigned char noise[1];int count=child->read(noise,sizeof(noise),true);
            if(count!=0)throw std::runtime_error("Unexpected script worker diagnostics");
        };
        auto transact=[&](const std::vector<unsigned char> &request,uint64_t sequence,const tsfp_script_event *event) {
            std::array<unsigned char,reply_max+4> bytes{};size_t sent=0,received=0,expected=4;
            tsfp_script_result result{};uint64_t cpu_base=usage.cpu_microseconds;
            auto deadline=Clock::now()+(event?std::chrono::milliseconds(500):std::chrono::milliseconds(2000));
            while(received<expected) {
                bool progress=false;
                if(Clock::now()>=deadline)throw std::runtime_error("Script request deadline exceeded");
                inspect(cpu_base,250000);
                if(sent<request.size()) {
                    int count=child->write(request.data()+sent,request.size()-sent);
                    if(count<0)throw std::runtime_error("Script input pipe failed");
                    sent+=(size_t)count;
                    progress=count>0;
                }
                // Never accept a reply before its complete request is written.
                if(sent==request.size()) {
                    int count=child->read(bytes.data()+received,expected-received);
                    if(count<0)throw std::runtime_error("Script reply pipe failed");
                    received+=(size_t)count;
                    progress=progress || count>0;
                    if(received==4 && expected==4) {
                        Reader header{bytes.data(),4};uint32_t size=header.u32();
                        if(size<28 || size>reply_max)throw std::runtime_error("Invalid script reply length");
                        expected=4+size;
                    }
                }
                if(received<expected && !progress)child->wait_for_activity(sent<request.size());
            }
            failure.clear();
            if(!decode(bytes.data()+4,expected-4,sequence,event,result,failure)) {
                if(failure.empty())failure="Invalid script transaction";
                throw std::runtime_error("Script transaction refused");
            }
            idle_cpu=usage.cpu_microseconds;return result;
        };
        std::vector<unsigned char> load(4);load.reserve(source.size()+12);put32(load,1);put32(load,seed);
        load.insert(load.end(),source.begin(),source.end());frame(load);
        (void)transact(load,0,nullptr);std::string().swap(source);std::vector<unsigned char>().swap(load);
        {std::lock_guard<std::mutex> lock(state->mutex);state->phase=SessionStatus::Ready;}
        for(;;) {
            inspect(idle_cpu,50000);
            unsigned char extra;int unsolicited=child->read(&extra,1);
            if(unsolicited!=0)throw std::runtime_error("Unsolicited script output");
            tsfp_script_event event{};bool queued=false;
            {std::lock_guard<std::mutex> lock(state->mutex);if(state->queued){event=state->event;state->queued=false;queued=true;}}
            if(!queued){
                std::unique_lock<std::mutex> lock(state->mutex);
                state->wake.wait_for(lock,std::chrono::milliseconds(10),[&]{return state->queued || state->cancel.load() || state->invalid.load();});
                continue;
            }
            auto result=transact(encode(event),event.sequence,&event);
            if(event.kind==TSFP_SCRIPT_STOP) {
                std::vector<unsigned char> quit;put32(quit,4);put32(quit,3);size_t sent=0;
                auto deadline=Clock::now()+std::chrono::milliseconds(500);
                for(;;) {
                    if(state->cancel.load() || state->invalid.load() || Clock::now()>=deadline)throw std::runtime_error("Script shutdown deadline exceeded");
                    if(!child->usage(usage))throw std::runtime_error("Cannot inspect script shutdown");
                    if(sent<quit.size() && !usage.exited){int n=child->write(quit.data()+sent,quit.size()-sent);if(n<0)throw std::runtime_error("Script shutdown pipe failed");sent+=(size_t)n;}
                    if(sent==quit.size())child->close_input();
                    unsigned char bytes[1];
                    if(child->read(bytes,1)!=0 || child->read(bytes,1,true)!=0)throw std::runtime_error("Unexpected script shutdown output");
                    if(usage.exited){if(usage.exit_code || sent!=quit.size())throw std::runtime_error("Script shutdown failed");break;}
                    if(usage.resident_bytes>128u*1024*1024 || usage.cpu_microseconds-idle_cpu>250000)throw std::runtime_error("Script shutdown resource budget exceeded");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                child.reset();
            }
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if(state->cancel.load() || state->invalid.load())throw std::runtime_error("Script session cancelled");
                state->sequence=event.sequence;state->tick=event.tick;state->started=true;
                if(event.kind==TSFP_SCRIPT_TICK){state->tick_seen=true;state->last_tick=event.tick;}
                for(uint32_t i=0;i<result.count;++i)if(result.commands[i].kind==TSFP_SCRIPT_END)state->ended=true;
                state->result=result;state->terminal=event.kind==TSFP_SCRIPT_STOP;state->phase=SessionStatus::Reply;
            }
            if(event.kind==TSFP_SCRIPT_STOP)break;
        }
    }catch(const std::exception &exception) {
        try {
            std::lock_guard<std::mutex> lock(state->mutex);state->result={};
            if(state->error.empty())state->error=(failure.empty()?exception.what():failure).substr(0,255);
            state->phase=SessionStatus::Faulted;
        }catch(...) {state->invalid.store(true);}
    }catch(...) {state->invalid.store(true);}
    child.reset();state->stopped.store(true);
}
}}
