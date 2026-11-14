#ifndef TSFP_MOD_MATCH_H
#define TSFP_MOD_MATCH_H
#include "mod_script.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TSFP_MOD_EVENT_QUEUE 64u
/* These are trusted bootstrap assertions, never fields read from a mod.
   Public launcher activation stays unavailable until that bootstrap exists. */
#define TSFP_MOD_IMAGES_VERIFIED 1u
#define TSFP_MOD_SOURCE_VERIFIED 2u
#define TSFP_MOD_SEPARATE_SAVES 4u
#define TSFP_MOD_OFFLINE 8u
typedef struct TsfpModWorld {
    uint64_t epoch,tick;
    uint32_t authoritative,offline,player_count,tick_rate;
    tsfp_script_player players[TSFP_SCRIPT_PLAYER_MAX];
    /* Stable host-side roster identities, NEVER sent to Lua. Not body pointers:
       the same player may respawn with a different body during a match. */
    uint64_t identities[TSFP_SCRIPT_PLAYER_MAX];
} TsfpModWorld;
typedef struct TsfpModMatchConfig {
    const char *worker,*cache,*source;
    size_t source_size;
    uint32_t seed,verified;
} TsfpModMatchConfig;
typedef struct TsfpModCommit {
    uint64_t epoch,sequence;
    tsfp_script_result transaction;
} TsfpModCommit;
typedef struct TsfpModMatch TsfpModMatch;
/* All calls belong to the game simulation thread. No child IO/waits in pump.
   Bootstrap verifies images/package/saves and enforces offline use before open.
   Destroy cancels asynchronously; keep the executable alive until stopped. */
TsfpModMatch *tsfp_mod_match_open(const TsfpModMatchConfig *,const TsfpModWorld *);
int tsfp_mod_match_enqueue(TsfpModMatch *,const TsfpModWorld *,uint32_t kind,uint32_t actor,uint32_t target);
/* 1 complete committed transaction;0 pending;-1 terminal fault, zero output. */
int tsfp_mod_match_pump(TsfpModMatch *,const TsfpModWorld *,TsfpModCommit *);
int tsfp_mod_match_matches(const TsfpModMatch *,const TsfpModWorld *);
int tsfp_mod_match_score(const TsfpModMatch *,const TsfpModWorld *,uint32_t id,int32_t *score);
/* Read derived ranks/order only; never changes original kill/death statistics.
   Arrays are indexed by id-1; order entries are script IDs (guest subtracts1). */
int tsfp_mod_match_ranking(const TsfpModMatch *,const TsfpModWorld *,int32_t ranks[32],uint32_t order[32]);
int tsfp_mod_match_ended(const TsfpModMatch *,const TsfpModWorld *,uint32_t *winner);
int tsfp_mod_match_faulted(const TsfpModMatch *);
const char *tsfp_mod_match_error(const TsfpModMatch *);
void tsfp_mod_match_close(TsfpModMatch *);
int tsfp_mod_match_stopped(const TsfpModMatch *);
void tsfp_mod_match_destroy(TsfpModMatch *);
#ifdef __cplusplus
}
#endif
#endif
