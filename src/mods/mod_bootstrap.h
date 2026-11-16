#ifndef TSFP_MOD_BOOTSTRAP_H
#define TSFP_MOD_BOOTSTRAP_H
#include <stddef.h>
#include "mod_wave.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Startup thread only. All source/audio variables absent select ordinary startup.
 * A request requires an explicit original save root; NULL reads RECOMP_SAVE_DIR
 * with native UTF8 conversion (never a personal default). Call before kernel init,
 * then commit after all host storage initialization, before starting the guest.
 * Failure MUST abort startup. There is deliberately no reset/unlock operation. */
int pc_mod_bootstrap_prepare(const char *original_saves);
int pc_mod_bootstrap_commit(void);
/* Owned immutable bytes, unavailable before successful commit or after failure.
 * Workers/matches still require their independent offline/authority/pin gates. */
const char *pc_mod_bootstrap_source(size_t *length);
const char *pc_mod_bootstrap_profile(void);
/* Optional PCM snapshots share process-lifetime ownership. Unavailable before
 * storage commit/after invalidation. No IO, allocation or decoding on lookup. */
const TsfpModWave *pc_mod_bootstrap_audio(uint32_t key);
/* Recheck the retained namespace before each new scripted match. */
int pc_mod_bootstrap_recheck(void);
/* Read-only background probe after commit; never changes UI/storage policy. */
int pc_mod_bootstrap_probe(void);
/* Permanently withhold source after runtime failure; retain fence and lease. */
void pc_mod_bootstrap_invalidate(void);
#ifdef __cplusplus
}
#endif
#endif
