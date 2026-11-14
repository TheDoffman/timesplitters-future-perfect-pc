// Test-only integration driver: real Session/Process/OS workers, synthetic events.
#include "mod_session.hpp"
#include <cstdio>
#include <chrono>
#include <thread>
#include <limits>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif
using namespace tsfp::mods;
using Clock=std::chrono::steady_clock;
static uint64_t maximum=0;
template<class F> auto timed(F function){auto start=Clock::now();auto result=function();auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();if((uint64_t)elapsed>maximum)maximum=(uint64_t)elapsed;return result;}
static void hex(const char *text,size_t size){for(size_t i=0;i<size;++i)printf("%02x",(unsigned char)text[i]);}
static void output(uint64_t seq,const tsfp_script_result &result) {
    printf("{\"sequence\":%llu,\"commands\":[",(unsigned long long)seq);
    for(uint32_t i=0;i<result.count;++i){const auto &c=result.commands[i];if(i)putchar(',');printf("{\"kind\":%u,\"player\":%u,\"value\":%d,\"text_hex\":\"",c.kind,c.player,c.value);hex(c.text,c.text_size);printf("\"}");}
    puts("]}");
}
static int run(const std::string &image,const std::string &cache,const std::string &scenario) {
    std::string source,error;char buffer[4096];size_t count;
    while((count=fread(buffer,1,sizeof(buffer),stdin))!=0){source.append(buffer,count);if(source.size()>2u*1024*1024)return 2;}
    auto session=timed([&]{return Session::open(image,cache,std::move(source),123,error);});
    if(!session){puts("{\"refused\":true}");return 0;}
    tsfp_script_result result{};uint64_t sequence=0;bool fault=false;size_t polls=0;bool busy=false;
    auto poll=[&]{++polls;return timed([&]{return session->poll(result,sequence,error);});};
    auto deadline=Clock::now()+std::chrono::seconds(30);
    auto wait=[&](SessionStatus target) {
        auto request_deadline=Clock::now()+std::chrono::seconds(3);
        while(Clock::now()<deadline && Clock::now()<request_deadline){auto status=poll();if(status==target)return true;if(status==SessionStatus::Faulted){fault=true;return false;}if(result.count)return false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        return false;
    };
    auto submit=[&](const tsfp_script_event &event) {
        // A try-lock collision is backpressure, never an accepted/consumed event.
        // Retry the same immutable event on a later simulated host iteration.
        Submission status=Submission::Busy;
        while(Clock::now()<deadline) {
            status=timed([&]{return session->dispatch(event);});
            if(status!=Submission::Busy)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return status;
    };
    if(wait(SessionStatus::Ready)) {
        if(scenario=="idle") {
            if(!wait(SessionStatus::Faulted))return 10;
            fault=true;
        }else {
            if(scenario=="match")std::this_thread::sleep_for(std::chrono::milliseconds(2100));
            if(scenario=="paced-idle")std::this_thread::sleep_for(std::chrono::milliseconds(4200));
            tsfp_script_event event{};event.sequence=1;event.kind=TSFP_SCRIPT_START;event.tick_rate=60;event.player_count=2;
            event.players[0]={1,0,0,100,0,1,2};event.players[1]={2,1,0,100,2,3,4};
            if(scenario=="invalid-sequence")event.sequence=3;
            if(scenario=="invalid-roster")event.players[1].id=1;
            if(scenario=="invalid-position")event.players[0].x=std::numeric_limits<float>::quiet_NaN();
            if(scenario=="invalid-phase")event.kind=TSFP_SCRIPT_TICK;
            auto submission=submit(event);
            if(scenario.rfind("invalid-",0)==0) {
                if(submission!=Submission::Rejected)return 3;
                if(!wait(SessionStatus::Faulted))return 11;
                fault=true;
            }else if(submission!=Submission::Accepted)return 4;
            else if(scenario=="close")session->close();
            else {
                busy=timed([&]{return session->dispatch(event);})==Submission::Busy;
                if(scenario=="backpressure")std::this_thread::sleep_for(std::chrono::milliseconds(60));
                if(scenario=="backpressure" && timed([&]{return session->dispatch(event);})!=Submission::Busy)return 5;
                if(wait(SessionStatus::Reply)) {
                    output(sequence,result);
                    if(scenario=="match" || scenario=="persistent") {
                        unsigned ticks=scenario=="match"?3:100;
                        for(unsigned i=0;i<ticks;++i) {
                            event.sequence=i+2;event.tick=i+1;event.kind=scenario=="match"?TSFP_SCRIPT_DEATH:TSFP_SCRIPT_TICK;
                            event.actor=1;event.target=2;
                            if(submit(event)!=Submission::Accepted || !wait(SessionStatus::Reply)) {
                                fprintf(stderr,"Session wait failed: %s; fault=%d; stopped=%d\n",error.c_str(),fault,session->stopped());return 6;
                            }
                            output(sequence,result);
                            if(scenario=="persistent")std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        }
                        event.sequence=ticks+2;event.tick=ticks+1;event.kind=TSFP_SCRIPT_STOP;event.actor=event.target=0;
                        if(submit(event)!=Submission::Accepted || !wait(SessionStatus::Reply))return 7;
                        output(sequence,result);
                        if(poll()!=SessionStatus::Finished)return 8;
                    }
                }
            }
        }
    }
    if(fault){printf("{\"fault\":true,\"count\":%u,\"error_hex\":\"",result.count);hex(error.data(),error.size());puts("\"}");}
    timed([&]{session->close();return true;});
    while(!session->stopped() && Clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    printf("{\"stopped\":%s,\"polls\":%llu,\"busy\":%s,\"max_api_us\":%llu}\n",session->stopped()?"true":"false",(unsigned long long)polls,busy?"true":"false",(unsigned long long)maximum);
    return session->stopped()?0:9;
}
#ifdef _WIN32
int wmain(int argc,wchar_t **argv) {
    if(argc!=4 || _setmode(_fileno(stdin),_O_BINARY)<0)return 2;
    auto utf8=[](const wchar_t *text){int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);if(size<=0)return std::string();std::string result((size_t)size,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,&result[0],size,nullptr,nullptr);result.pop_back();return result;};
    return run(utf8(argv[1]),utf8(argv[2]),utf8(argv[3]));
}
#else
int main(int argc,char **argv){return argc==4?run(argv[1],argv[2],argv[3]):2;}
#endif
