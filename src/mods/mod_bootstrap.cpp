#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "mod_bootstrap.h"
#include "mod_capsule.hpp"
#include "mod_saves.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
extern "C" {
int xbox_require_save_root_utf8(const char *);
int xbox_save_root_utf8(char *,unsigned);
int pc_storage_resolve_root(const char *,char *,size_t);
void pc_storage_lock_locations(void);
}
namespace {
enum class Phase { Fresh, Ordinary, Preparing, Prepared, Ready, Failed };
Phase phase=Phase::Fresh;
struct Context {
    std::unique_ptr<tsfp::mods::SourceCapsule> capsule;
    std::unique_ptr<tsfp::mods::SaveNamespace> saves;
};
// Process-lifetime ownership is intentional: guest background storage may still
// be live during C++ static destruction. The OS releases the lock on final exit.
// Never release this lease in an atexit callback or on a failed startup retry.
Context *context=nullptr;
struct Environment {bool present=false;std::string value;};
Environment environment(const char *name,size_t limit) {
    Environment result;
#ifdef _WIN32
    wchar_t key[64];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,key,64))throw std::runtime_error("Environment key");
    const wchar_t *value=_wgetenv(key);
    result.present=value!=nullptr;if(!value)return result;
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,nullptr,0,nullptr,nullptr);
    if(n<=0 || (size_t)n>limit+1)throw std::runtime_error("Environment value");
    result.value.resize((size_t)n);
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,result.value.data(),n,nullptr,nullptr))throw std::runtime_error("Environment UTF8");
    result.value.pop_back();
#else
    const char *value=std::getenv(name);
    result.present=value!=nullptr;if(!value)return result;
    size_t n=0;while(n<=limit && value[n])++n;
    if(n>limit)throw std::runtime_error("Environment value");
    result.value.assign(value,n);
#endif
    return result;
}
int fail(const char *reason) noexcept {
    phase=Phase::Failed;
    pc_storage_lock_locations();
    // An invalid/late request poisons an unarmed fence. An already valid fence
    // cannot be reset; either way there can be no fallback to personal saves.
    (void)xbox_require_save_root_utf8(nullptr);
    std::fprintf(stderr,"[MOD-BOOT] %s; scripted startup refused\n",reason);
    return 0;
}
bool ready() noexcept {return phase==Phase::Ready && context && context->capsule && context->saves;}
}
extern "C" int pc_mod_bootstrap_prepare(const char *original_saves) {
    if(phase!=Phase::Fresh)return fail("Startup was already selected");
    phase=Phase::Preparing;
    try {
        auto path=environment("TSFP_MOD_SCRIPT_CAPSULE",4095),pin=environment("TSFP_MOD_SCRIPT_CAPSULE_SHA256",64);
        if(!path.present && !pin.present){phase=Phase::Ordinary;return 1;}
        if(!path.present || path.value.empty() || !pin.present || pin.value.empty())return fail("Incomplete source handoff");
        std::string original=original_saves?original_saves:environment("RECOMP_SAVE_DIR",4095).value;
        if(original.empty())return fail("An explicit original save root is required");
        context=new Context;
        std::string error;
        context->capsule=tsfp::mods::SourceCapsule::read(path.value,pin.value,error);
        if(!context->capsule)return fail("Source verification failed");
        char selected[4096];
        int preference=pc_storage_resolve_root(original.c_str(),selected,sizeof selected);
        if(preference<0)return fail("Selected original save drive is unavailable");
        context->saves=tsfp::mods::SaveNamespace::open(preference?selected:original.c_str(),context->capsule->profile(),error);
        if(!context->saves || !context->saves->recheck(error))return fail("Isolated save lease failed");
        // Resolve the ordinary preference above, before locking it. The picker
        // must be locked before it can create folders or change any preference.
        pc_storage_lock_locations();
        if(!xbox_require_save_root_utf8(context->saves->root().c_str()))return fail("Storage fence registration failed");
        phase=Phase::Prepared;return 1;
    }catch(...){return fail("Source/save preparation failed");}
}
extern "C" int pc_mod_bootstrap_commit(void) {
    if(phase==Phase::Ordinary)return 1;
    if(phase!=Phase::Prepared)return fail("Storage was not prepared");
    try {
        char actual[4096];std::string error;
        if(!xbox_save_root_utf8(actual,sizeof actual) || context->saves->root()!=actual || !context->saves->recheck(error))
            return fail("Guest storage did not commit the isolated root");
        phase=Phase::Ready;return 1;
    }catch(...){return fail("Storage commit verification failed");}
}
extern "C" const char *pc_mod_bootstrap_source(size_t *length) {
    if(length)*length=ready()?context->capsule->source().size():0;
    return ready()?context->capsule->source().data():nullptr;
}
extern "C" const char *pc_mod_bootstrap_profile(void) {return ready()?context->capsule->profile().c_str():nullptr;}
extern "C" int pc_mod_bootstrap_recheck(void) {
    if(!ready())return 0;
    try {std::string error;if(context->saves->recheck(error))return 1;}
    catch(...){}
    return fail("Isolated save lease changed");
}
