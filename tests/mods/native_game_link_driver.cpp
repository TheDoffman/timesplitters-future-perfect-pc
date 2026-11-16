#include "mod_game_runtime.h"
#include "mod_process_identity.hpp"
#include "mod_audio_player.h"
#include <cstdio>
#include <cstdlib>
#ifdef TSFP_MOD_PROCESS_TESTING
#error The real game target must not inherit test process hooks
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
// The production game.cmake compiles ALL parent modules. Only the surrounding
// title's storage/network/audio callbacks are absent here; none may run without
// selecting a mode. Abort makes accidental ordinary-startup work observable.
extern "C" int xbox_require_save_root_utf8(const char *){std::abort();}
extern "C" int xbox_save_root_utf8(char *,unsigned){std::abort();}
extern "C" int pc_storage_resolve_root(const char *,char *,size_t){std::abort();}
extern "C" void pc_storage_lock_locations(void){std::abort();}
extern "C" void pc_network_lock_offline(void){std::abort();}
extern "C" void pc_debug_set_overlay_extension(int (*)(char *,size_t)){std::abort();}
extern "C" int pc_mod_audio_play(uint32_t,uint32_t,uint32_t,int32_t,int){std::abort();}
extern "C" int pc_mod_audio_pause(uint32_t,int){std::abort();}
extern "C" int pc_mod_audio_pitch(uint32_t,uint32_t){std::abort();}
extern "C" int pc_mod_audio_volume(uint32_t,int32_t){std::abort();}
extern "C" void pc_mod_audio_stop(uint32_t){std::abort();}
extern "C" void pc_mod_audio_stop_all(void){std::abort();}
int main() {
    using namespace tsfp::mods;
#ifdef _WIN32
    uint64_t pid=GetCurrentProcessId();
#else
    uint64_t pid=static_cast<uint64_t>(getpid());
#endif
    ProcessStamp stamp;ProcessStamps empty;
    if(empty.valid() || process_stamp_state(stamp)!=ProcessState::unknown ||
        read_process_stamp(pid,stamp)!=ProcessState::alive || !stamp.valid() ||
        pc_mod_runtime_selected() || !pc_mod_runtime_shutdown())return 3;
    std::puts("production game target linked; native identity queries passed; no mode selected");return 0;
}
