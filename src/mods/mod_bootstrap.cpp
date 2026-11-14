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
    const char *path=std::getenv("TSFP_MOD_SCRIPT_CAPSULE");
    const char *pin=std::getenv("TSFP_MOD_SCRIPT_CAPSULE_SHA256");
    if(!path && !pin){phase=Phase::Ordinary;return 1;}
    phase=Phase::Preparing;
    try {
        if(!path || !*path || !pin || !*pin)return fail("Incomplete source handoff");
        if(!original_saves || !*original_saves)return fail("An explicit original save root is required");
        context=new Context;
        std::string error;
        context->capsule=tsfp::mods::SourceCapsule::read(path,pin,error);
        if(!context->capsule)return fail("Source verification failed");
        char selected[4096];
        int preference=pc_storage_resolve_root(original_saves,selected,sizeof selected);
        if(preference<0)return fail("Selected original save drive is unavailable");
        context->saves=tsfp::mods::SaveNamespace::open(preference?selected:original_saves,context->capsule->profile(),error);
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
