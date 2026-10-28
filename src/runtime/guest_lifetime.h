/* Optional lifecycle observers for native guest continuations. Configure only
 * before guest threads start, or while they are quiescent. Restore must release
 * abandoned operation scopes before longjmp and must not execute guest code. */
#ifndef TSFP_GUEST_LIFETIME_H
#define TSFP_GUEST_LIFETIME_H
#include <stdint.h>
typedef uint64_t (*tsfp_guest_lifetime_capture)(void);
typedef int (*tsfp_guest_lifetime_restore)(uint64_t);
int tsfp_guest_lifetime_hooks(tsfp_guest_lifetime_capture,tsfp_guest_lifetime_restore);
#endif
