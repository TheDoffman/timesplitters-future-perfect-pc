#include "mod_game_runtime.h"
#include "mod_bootstrap.h"
#include "mod_resources.hpp"
#include "mod_runtime.hpp"
#include "mod-runtime-images.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <algorithm>
extern "C" void pc_network_lock_offline(void);
namespace {
struct Context {
    std::unique_ptr<tsfp::mods::RuntimeResources> resources;
    std::unique_ptr<tsfp::mods::ScriptRuntime> runtime;
    std::thread::id owner;
    uint64_t bound=0;
};
// Never let static destruction release resources beneath a live guest/probe.
Context *context=nullptr;
std::mutex facade;
bool attempted=false;
std::atomic<bool> selected{false},failed{false},closing{false};
char diagnostic[256];
std::string hud_message;
std::chrono::steady_clock::time_point message_until;
void fail(const char *message) noexcept {
    if(!failed.load()) {
        size_t n=0;while(n<sizeof(diagnostic)-1 && message[n])++n;
        std::memcpy(diagnostic,message,n);diagnostic[n]=0;failed.store(true);
        std::fprintf(stderr,"[MOD-RUNTIME] %s\n",diagnostic);
    }
    pc_mod_bootstrap_invalidate();
    if(context && context->runtime)context->runtime->close();
}
void at_exit(){(void)pc_mod_runtime_shutdown();}
}
extern "C" int pc_mod_runtime_prepare(const char *parent) {
    std::lock_guard<std::mutex> lock(facade);
    if(attempted){fail("Script runtime startup was already selected");return 0;}
    attempted=true;
    size_t length=0;const char *source=pc_mod_bootstrap_source(&length);
    if(!source)return 1;
    selected.store(true);
    pc_network_lock_offline();
    try {
        context=new Context;
        std::string error;
        tsfp::mods::ImagePins pins{TSFP_MOD_RUNTIME_WORKER_SHA256,TSFP_MOD_RUNTIME_GUARDIAN_SHA256};
        context->resources=tsfp::mods::RuntimeResources::create(TSFP_MOD_RUNTIME_SUBDIR,TSFP_MOD_RUNTIME_WORKER,
            pins,parent?parent:tsfp::mods::native_cache_parent(),error);
        if(!context->resources){fail("Compiled helpers or private cache were unavailable");return 0;}
        constexpr uint32_t policy=TSFP_MOD_IMAGES_VERIFIED|TSFP_MOD_SOURCE_VERIFIED|TSFP_MOD_SEPARATE_SAVES|TSFP_MOD_OFFLINE;
        TsfpModMatchConfig config{context->resources->worker().c_str(),context->resources->cache().c_str(),source,length,
            0x54534650u,policy,TSFP_MOD_RUNTIME_WORKER_SHA256,TSFP_MOD_RUNTIME_GUARDIAN_SHA256};
        context->runtime=tsfp::mods::ScriptRuntime::create(config,[]{
            std::string reason;return pc_mod_bootstrap_probe() && context->resources->recheck(reason);
        },error);
        if(!context->runtime){fail("Script runtime could not start");return 0;}
        if(std::atexit(at_exit)){fail("Script cleanup could not be registered");return 0;}
        std::fprintf(stderr,"[MOD-RUNTIME] Verified source, isolated saves and compiled helpers ready; offline matches only\n");
        return 1;
    }catch(...){fail("Script runtime preparation failed");return 0;}
}
extern "C" int pc_mod_runtime_selected(void){return selected.load()?1:0;}
extern "C" TsfpModMatch *pc_mod_runtime_update(const TsfpModWorld *world,int can_start) {
    if(!selected.load())return nullptr;
    std::unique_lock<std::mutex> lock(facade,std::try_to_lock);if(!lock.owns_lock())return nullptr;
    if(!context || !context->runtime)return nullptr;
    if(context->owner==std::thread::id{})context->owner=std::this_thread::get_id();
    if(context->owner!=std::this_thread::get_id()) {closing.store(true);return nullptr;}
    try {
        if(failed.load() || closing.load()){context->runtime->close();context->bound=0;return nullptr;}
        if(!can_start && (!world || context->bound!=world->epoch))world=nullptr;
        auto *match=context->runtime->update(world);
        if(context->runtime->faulted()){fail(context->runtime->error().c_str());match=nullptr;}
        context->bound=match && world?world->epoch:0;return match;
    }catch(...){fail("Script frame admission failed");return nullptr;}
}
extern "C" int pc_mod_runtime_faulted(void){return failed.load()?1:0;}
extern "C" const char *pc_mod_runtime_error(void){return failed.load()?diagnostic:"";}
extern "C" void pc_mod_runtime_message(const char *text,size_t size) {
    if(!selected.load() || !text || !size || size>TSFP_SCRIPT_TEXT_MAX)return;
    for(size_t i=0;i<size;++i)if((unsigned char)text[i]<32 || text[i]==127)return;
    std::unique_lock<std::mutex> lock(facade,std::try_to_lock);if(!lock.owns_lock())return;
    try {hud_message.assign(text,size);message_until=std::chrono::steady_clock::now()+std::chrono::seconds(8);}catch(...){}
}
extern "C" int pc_mod_runtime_overlay(char *text,size_t bytes) {
    if(!text || !bytes || !selected.load())return 0;
    std::unique_lock<std::mutex> lock(facade,std::try_to_lock);if(!lock.owns_lock())return 0;
    try {
        std::string shown;
        if(failed.load())shown="Mod stopped: launch again or play without mods";
        else if(!hud_message.empty() && std::chrono::steady_clock::now()<message_until) {
            size_t n=std::min(hud_message.size(),size_t(96));
            while(n<hud_message.size() && n && ((unsigned char)hud_message[n]&0xc0)==0x80)--n;
            shown="Custom mode | "+hud_message.substr(0,n)+(n<hud_message.size()?"...":"");
        }else shown=context && context->bound?"Custom mode | Offline":"Script mode | Offline Arcade only";
        size_t n=std::min(shown.size(),bytes-1);
        while(n<shown.size() && n && ((unsigned char)shown[n]&0xc0)==0x80)--n;
        std::memcpy(text,shown.data(),n);text[n]=0;return n?1:0;
    }catch(...){text[0]=0;return 0;}
}
extern "C" void pc_mod_runtime_request_shutdown(void){closing.store(true);}
extern "C" int pc_mod_runtime_shutdown(void) {
    closing.store(true);
    std::lock_guard<std::mutex> lock(facade);
    if(!context)return 1;
    if(context->owner!=std::thread::id{} && context->owner!=std::this_thread::get_id())return 0;
    if(context->runtime) {
        context->runtime->close();auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!context->runtime->stopped() && std::chrono::steady_clock::now()<end)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(!context->runtime->stopped())return 0;
        context->runtime.reset();
    }
    context->resources.reset();context->bound=0;return 1;
}
