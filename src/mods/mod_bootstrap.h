#ifndef TSFP_MOD_BOOTSTRAP_H
#define TSFP_MOD_BOOTSTRAP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Startup thread only. Both absent capsule variables select ordinary startup.
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
/* Recheck the retained namespace before each new scripted match. */
int pc_mod_bootstrap_recheck(void);
#ifdef __cplusplus
}
#endif
#endif
