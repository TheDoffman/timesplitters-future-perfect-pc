/* Project branch-observation interface. Disabled hooks discard their tokens. */
#ifndef TSFP_BRANCH_FEEDBACK_H
#define TSFP_BRANCH_FEEDBACK_H
#include <stdint.h>

#define RECOMP_ICALL_FB_BASE UINT32_C(0x00010000)
#define RECOMP_ICALL_FB_SIZE UINT32_C(0x00800000)
#define RECOMP_ICALL_SEEN_RESOLVED 1u
#define RECOMP_ICALL_SEEN_UNRESOLVED 2u
#ifndef RECOMP_ICALL_FEEDBACK_PATH
#define RECOMP_ICALL_FEEDBACK_PATH "icall_targets.dump"
#endif

#ifdef RECOMP_ICALL_FEEDBACK
extern volatile uint64_t g_icall_guard_hits, g_icall_guard_misses;
void tsfp_feedback_target(uint32_t address, unsigned char flags);
void tsfp_feedback_guard(unsigned matched);
void recomp_icall_observe_site(uint32_t site, uint32_t target);
void recomp_icall_feedback_dump(const char *path);
void recomp_icall_feedback_init(void);
const char *tsfp_feedback_output_path(void);

static inline int tsfp_feedback_accepts(uint32_t address) {
    return address >= RECOMP_ICALL_FB_BASE &&
           address < RECOMP_ICALL_FB_BASE + RECOMP_ICALL_FB_SIZE;
}
/* Flags stay lazy for an address outside the observation window. */
#define RECOMP_ICALL_OBSERVE(address, flags) do { \
    const uint32_t tsfp_observed_address_ = (uint32_t)(address); \
    if (tsfp_feedback_accepts(tsfp_observed_address_)) \
        tsfp_feedback_target(tsfp_observed_address_, (unsigned char)(flags)); \
} while (0)
/* Preserve the evaluation order even when a caller supplies side effects. */
#define RECOMP_ICALL_OBSERVE_SITE(site, target) do { \
    const uint32_t tsfp_observed_site_ = (uint32_t)(site); \
    const uint32_t tsfp_observed_target_ = (uint32_t)(target); \
    recomp_icall_observe_site(tsfp_observed_site_, tsfp_observed_target_); \
} while (0)
#define RECOMP_ICALL_FEEDBACK_INIT() recomp_icall_feedback_init()
#define RECOMP_ICALL_FEEDBACK_DUMP() recomp_icall_feedback_dump(tsfp_feedback_output_path())
#else
#define RECOMP_ICALL_OBSERVE(address, flags) ((void)0)
#define RECOMP_ICALL_OBSERVE_SITE(site, target) ((void)0)
#define RECOMP_ICALL_FEEDBACK_INIT() ((void)0)
#define RECOMP_ICALL_FEEDBACK_DUMP() ((void)0)
#endif
#endif
