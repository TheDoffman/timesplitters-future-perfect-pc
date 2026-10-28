#include <stdio.h>
#include "recomp_types.h"
#include "recomp_types.h"

uint32_t g_xbox_code_lo, g_xbox_code_hi;
static unsigned events, wrong, names, addresses, tags;
static const char *next_name(void) { ++names; return "guest"; }
static const char *next_tag(void) { ++tags; return "stack"; }
static uint32_t next_address(void) { ++addresses; return 0x1020u; }
void recomp_trace_enter(const char *name, uint32_t address) {
    wrong |= strcmp(name,"guest") != 0 || address != 0x1020u || events != 0; ++events;
}
void recomp_trace_exit(const char *name, uint32_t address) {
    wrong |= strcmp(name,"guest") != 0 || address != 0x1020u || events != 1; ++events;
}
void recomp_trace_esp(const char *name, const char *tag) {
    wrong |= strcmp(name,"guest") != 0 || strcmp(tag,"stack") != 0 || events != 2; ++events;
}
void recomp_unimpl(const char *name, uint32_t address) {
    wrong |= strcmp(name,"guest") != 0 || address != 0x1020u || events != 3; ++events;
}
#ifdef RECOMP_ICALL_FEEDBACK
volatile uint64_t g_icall_guard_hits, g_icall_guard_misses;
#endif

/* An independent interval oracle uses widened comparisons; no inclusive upper
 * endpoint, subtraction trick, or signed reinterpretation is shared with code. */
static int reference(uint32_t address) {
    uint64_t a=address, low=g_xbox_code_lo, high=g_xbox_code_hi;
    if (a >= 4261412864ULL || high == 0) return 1;
    return a >= low && a < high;
}
int main(void) {
    static const uint32_t bounds[]={0,1,0x1000u,0x80000000u,0xfdffffffu,0xfe000000u,0xffffffffu};
    unsigned checks=0; uint32_t random=1;
    for(unsigned lo=0;lo<7;++lo) for(unsigned hi=0;hi<7;++hi) {
        g_xbox_code_lo=bounds[lo]; g_xbox_code_hi=bounds[hi];
        for(unsigned i=0;i<7;++i) {
            uint32_t probes[]={bounds[i],bounds[i]-1u,bounds[i]+1u};
            for(unsigned j=0;j<3;++j) {
                uint32_t address=probes[j], original=address;
                if(RECOMP_ICALL_IS_CODE(address++)!=reference(original) || address!=original+1u) return 1;
                ++checks;
            }
        }
        for(unsigned i=0;i<4096;++i) {
            random=random*1664525u+1013904223u;
            if(RECOMP_ICALL_IS_CODE(random)!=reference(random)) return 1;
            ++checks;
        }
    }
    g_xbox_code_lo=0x1000;g_xbox_code_hi=0x2000;
    if(!RECOMP_ICALL_IS_CODE(UINT64_C(0x100001020))) return 1;
    RECOMP_TRACE_ENTER(next_name(),next_address());
    RECOMP_TRACE_EXIT(next_name(),next_address());
    RECOMP_TRACE_ESP(next_name(),next_tag());
    RECOMP_UNIMPL(next_name(),next_address());
    if(wrong || events!=4 || names!=4 || addresses!=3 || tags!=1) return 2;
#ifdef RECOMP_ICALL_FEEDBACK
    g_icall_guard_hits=UINT64_MAX;g_icall_guard_misses=7;
    RECOMP_ICALL_GUARD_HIT();RECOMP_ICALL_GUARD_MISS();
    if(g_icall_guard_hits!=0 || g_icall_guard_misses!=8) return 3;
#else
    RECOMP_ICALL_OBSERVE(undefined_address(),undefined_flags);
    RECOMP_ICALL_OBSERVE_SITE(undefined_site(),undefined_target());
    RECOMP_ICALL_GUARD_HIT();RECOMP_ICALL_GUARD_MISS();
#endif
    printf("%u address cases and service contracts passed\n",checks);
    return 0;
}
