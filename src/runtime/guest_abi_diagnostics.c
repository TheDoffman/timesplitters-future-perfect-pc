/* Project diagnostics for the canonical guest call-check hooks.
 * Bounded first-occurrence reporting, serialized across native callers.
 * Ordinary diagnostic use only: not signal-safe or reentrant crash handling.
 */
#include "recomp_types.h"
#ifdef RECOMP_ABI_CHECK
#include <stdio.h>
#if defined(_MSC_VER)
#include <intrin.h>
static volatile long report_lock;
static void lock_reports(void) {
    while (_InterlockedCompareExchange(&report_lock, 1, 0)) { }
}
static void unlock_reports(void) { (void)_InterlockedExchange(&report_lock, 0); }
#else
static unsigned char report_lock;
static void lock_reports(void) {
    while (__atomic_test_and_set(&report_lock, __ATOMIC_ACQUIRE)) { }
}
static void unlock_reports(void) { __atomic_clear(&report_lock, __ATOMIC_RELEASE); }
#endif

typedef struct { uint32_t addresses[64]; unsigned used; } ReportSet;
static ReportSet ordinary_calls, known_cleanup;

/* Called with the reporting lock; address zero is a valid key. */
static int first_report(ReportSet *set, unsigned limit, uint32_t address) {
    for (unsigned i=0; i<set->used; ++i)
        if (set->addresses[i]==address) return 0;
    if (set->used==limit) return 0;
    set->addresses[set->used++]=address;
    return 1;
}

/* Interpret modular 32-bit deltas without overflowing signed arithmetic. */
static long long signed_delta(uint32_t bits) {
    return (bits & UINT32_C(0x80000000)) ?
        (long long)bits - 4294967296LL : (long long)bits;
}

static void recent_targets(void) {
    const uint32_t cursor=g_icall_trace_idx;
    fputs("      recent icall targets:",stderr);
    for (uint32_t age=4; age; --age)
        fprintf(stderr," %08X",(unsigned)g_icall_trace[(cursor-age) & (ICALL_TRACE_SIZE-1u)]);
    fputc('\n',stderr);
}

static void report(uint32_t address, uint32_t base, uint32_t source,
                   uint32_t destination, uint32_t stack, uint32_t cleanup,
                   int exact) {
    lock_reports();
    if (!first_report(exact ? &known_cleanup : &ordinary_calls,
                      exact ? 64u : 32u,address)) {
        unlock_reports();
        return;
    }
    const uint32_t actual=g_esp-stack;
    const uint32_t expected=cleanup+4u;
    const char *stack_note="";
    if (exact) {
        if (signed_delta(actual)>signed_delta(expected)) stack_note=" esp-too-high";
        if (signed_delta(actual)<signed_delta(expected)) stack_note=" esp-too-low";
    } else if (g_esp < stack+4u) stack_note=" esp(epilogue never ran)";
    fprintf(stderr,"[ABI] sub_%08X:%s%s%s%s\n",
        (unsigned)address, g_ebx!=base ? " ebx" : "",
        g_esi!=source ? " esi" : "",g_edi!=destination ? " edi" : "",stack_note);
    fprintf(stderr,"      ebx %08X->%08X esi %08X->%08X edi %08X->%08X esp %08X->%08X\n",
        (unsigned)base,(unsigned)g_ebx,(unsigned)source,(unsigned)g_esi,
        (unsigned)destination,(unsigned)g_edi,(unsigned)stack,(unsigned)g_esp);
    if (exact)
        fprintf(stderr,"      esp %+lld, expected %+lld (ret %u)\n",
            signed_delta(actual),signed_delta(expected),(unsigned)cleanup);
    else fprintf(stderr,"      esp delta %+lld\n",signed_delta(actual));
    recent_targets();
    fflush(stderr);
    unlock_reports();
}

void recomp_abi_violation_log(uint32_t address,uint32_t base,uint32_t source,
                             uint32_t destination,uint32_t stack) {
    report(address,base,source,destination,stack,0,0);
}
void recomp_abi_pop_violation_log(uint32_t address,uint32_t base,uint32_t source,
                                 uint32_t destination,uint32_t stack,uint32_t cleanup) {
    report(address,base,source,destination,stack,cleanup,1);
}
#endif
