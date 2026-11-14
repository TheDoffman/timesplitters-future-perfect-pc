#ifndef TSFP_PC_MOD_GAME_H
#define TSFP_PC_MOD_GAME_H
#include "mod_match.h"
/* Trusted startup owns the match. No environment/file/bootstrap entry point is
   provided here; public script activation remains unavailable. */
int pc_mod_game_capture(TsfpModWorld *);
int pc_mod_game_bind(TsfpModMatch *);
void pc_mod_game_frame(void (*original)(void));
void pc_mod_game_death(void (*original)(void));
void pc_mod_game_score(void (*original)(void));
void pc_mod_game_ranking(void (*original)(void));
void pc_mod_game_limits(void (*original)(void));
#endif
