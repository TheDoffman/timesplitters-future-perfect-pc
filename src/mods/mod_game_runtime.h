#ifndef TSFP_MOD_GAME_RUNTIME_H
#define TSFP_MOD_GAME_RUNTIME_H
#include "mod_match.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Once, after bootstrap storage commit, before the guest starts. NULL selects
 * the native OS cache parent; an explicit parent is a trusted host/test input,
 * never a mod field. No capsule request leaves ordinary gameplay unchanged. */
int pc_mod_runtime_prepare(const char *cache_parent);
int pc_mod_runtime_selected(void);
/* Simulation-thread only. can_start is false for results; keep an already bound
 * model there, but never start a new script after a stock match has ended. */
TsfpModMatch *pc_mod_runtime_update(const TsfpModWorld *,int can_start);
int pc_mod_runtime_faulted(void);
const char *pc_mod_runtime_error(void);
/* Latest validated message, displayed for8s through the shared native overlay.
 * Rendering shows a bounded UTF8 preview; no script text is a format string. */
void pc_mod_runtime_message(const char *,size_t);
int pc_mod_runtime_overlay(char *,size_t);
/* Signal-only across threads. Shutdown from the simulation thread (or before
 * any frame) drains up to5s. A different-thread/timeout exit retains resources
 * until OS teardown, with child Job/guardian lifetime containment still active. */
void pc_mod_runtime_request_shutdown(void);
int pc_mod_runtime_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
