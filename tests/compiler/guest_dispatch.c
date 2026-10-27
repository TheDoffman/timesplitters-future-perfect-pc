/* Original dispatch contract: an independent event/state oracle, including
 * diagnostic callbacks that deliberately alter the guest stack. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "under_test.h"

RECOMP_TLS uint32_t g_eax, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_esp;
RECOMP_TLS uint32_t g_icall_saved_esp, g_icall_dispatch_form;
uint32_t g_xbox_code_lo, g_xbox_code_hi;
volatile uint32_t g_icall_trace[16], g_icall_trace_idx;
volatile uint64_t g_icall_count;
volatile uint64_t g_icall_guard_hits, g_icall_guard_misses;
static char events[64];
static unsigned used, mask, calls, stack_reads, site_reads, target_reads;
static unsigned damage, diagnostic_damage, bad;
static uint32_t logged_esp, logged_eax, logged_restore, logged_va, abi_esp;
static unsigned current_form;
#define CHECK(c) do { if (!(c)) { printf("line %d\n", __LINE__); return 1; } } while (0)
static void event(char value) {
    /* The call must already be recorded when any observer/resolver runs. */
    if (g_icall_trace_idx != (current_form == 3 ? UINT32_MAX : 0) ||
        g_icall_count != (current_form == 3 ? UINT64_MAX : 0) ||
        g_icall_trace[15] != (current_form == 3 ? 0xDEAD : logged_va)) bad = 1;
    events[used++] = value; events[used] = 0;
}
void observe(uint32_t target, unsigned flags) {
    if (target != logged_va) bad = 1;
    event(flags == 1 ? 'R' : 'U');
}
void observe_site(uint32_t site, uint32_t target) {
    if (site != 0x1234 || target != logged_va) bad = 1;
    event('S');
}
static void callee(void) {
    event('C'); ++calls; g_esp += 12; g_eax = 0xAA55;
    if (damage) ++g_ebx;
}
recomp_func_t recomp_lookup_manual(uint32_t target) {
    if (target != logged_va) bad = 1;
    event('M'); return mask & 1 ? callee : 0;
}
recomp_func_t recomp_lookup(uint32_t target) {
    if (target != logged_va) bad = 1;
    event('G'); return mask & 2 ? callee : 0;
}
recomp_func_t recomp_lookup_kernel(uint32_t target) {
    if (target != logged_va) bad = 1;
    event('K'); return mask & 4 ? callee : 0;
}
void recomp_icall_fail_log(uint32_t target) {
    if (target != logged_va) bad = 1;
    event('F'); logged_esp = g_esp; logged_eax = g_eax;
    if (diagnostic_damage) g_esp += 20;
}
void recomp_icall_not_code_log(uint32_t target, uint32_t restored) {
    if (target != logged_va) bad = 1;
    event('N'); logged_esp = g_esp; logged_eax = g_eax;
    logged_restore = restored;
    if (diagnostic_damage) g_esp += 20;
}
void recomp_abi_violation_log(uint32_t target, uint32_t b, uint32_t s,
                              uint32_t d, uint32_t p) {
    if (target != logged_va || b != 11 || s != 22 || d != 33) bad = 1;
    abi_esp = p; event('A');
}
static uint64_t target_value(void) { ++target_reads; return UINT64_C(0x300000000) + logged_va; }
static uint32_t stack_value(void) { ++stack_reads; return g_esp + 40; }
static uint32_t site_value(void) { ++site_reads; return 0x1234; }

static int run_case(unsigned form, uint32_t target, unsigned available,
                    unsigned unloaded, unsigned clobber, unsigned diag,
                    uint32_t initial) {
    char expected[64] = "";
    unsigned valid = unloaded || target >= 0xFE000000u || (target >= 0x1000 && target < 0x2000);
    unsigned dispatch = valid || form == 3;
    used = calls = stack_reads = site_reads = target_reads = bad = 0;
    events[0] = 0; mask = available; damage = clobber; diagnostic_damage = diag; current_form = form;
    logged_va = target; logged_esp = logged_eax = logged_restore = abi_esp = 0;
    g_esp = initial; g_eax = 77; g_ebx = 11; g_esi = 22; g_edi = 33;
    g_xbox_code_lo = 0x1000; g_xbox_code_hi = unloaded ? 0 : 0x2000;
    g_icall_saved_esp = 99; g_icall_dispatch_form = 99;
    g_icall_trace_idx = UINT32_MAX; g_icall_count = UINT64_MAX;
    for (unsigned i = 0; i < 16; ++i) g_icall_trace[i] = 0xDEAD;
    switch (form) {
    case 0: RECOMP_ICALL(target_value()); break;
    case 1: RECOMP_ICALL_SAFE(target_value(), stack_value()); break;
    case 2: RECOMP_ICALL_SAFE_AT(target_value(), stack_value(), site_value()); break;
    default: RECOMP_ITAIL(target_value()); break;
    }
    CHECK(target_reads == 1 && !bad);
    CHECK(g_icall_trace_idx == (form == 3 ? UINT32_MAX : 0));
    CHECK(g_icall_count == (form == 3 ? UINT64_MAX : 0));
    for (unsigned i = 0; i < 16; ++i)
        CHECK(g_icall_trace[i] == (i == 15 && form != 3 ? target : 0xDEAD));
#ifdef RECOMP_ICALL_FEEDBACK
    if (dispatch && form == 2) strcat(expected, "S");
    CHECK(site_reads == (unsigned)(dispatch && form == 2));
#else
    CHECK(site_reads == 0);
#endif
    if (dispatch) {
        strcat(expected, "M");
        if (!(available & 1)) {
            strcat(expected, "G");
            if (!(available & 2)) strcat(expected, "K");
        }
#ifdef RECOMP_ICALL_FEEDBACK
        strcat(expected, available ? "R" : "U");
#endif
        strcat(expected, available ? "C" : "F");
    } else strcat(expected, "N");
    if (dispatch && available) {
        CHECK(calls == 1 && g_eax == 0xAA55 && g_esp == (uint32_t)(initial + 12));
        CHECK(stack_reads == 0 && g_icall_dispatch_form == 99 && g_icall_saved_esp == 99);
#ifdef RECOMP_ABI_CHECK
        if (clobber || (uint32_t)(initial + 12) < (uint32_t)(initial + 4)) {
            strcat(expected, "A"); CHECK(abi_esp == initial);
        }
#endif
    } else {
        uint32_t after_log = initial + (diag ? 20u : 0u);
        CHECK(calls == 0 && g_eax == 0 && logged_esp == initial && logged_eax == 77);
        CHECK(g_esp == (uint32_t)(after_log + ((form == 1 || form == 2) ? 40u : 4u)));
        CHECK(stack_reads == ((form == 1 || form == 2) ? (dispatch ? 1u : 2u) : 0u));
        if (dispatch) {
            CHECK(g_icall_saved_esp == initial && g_icall_dispatch_form == (form == 3 ? 2u : 1u));
        } else {
            CHECK(g_icall_dispatch_form == 99 && g_icall_saved_esp == 99);
            CHECK(logged_restore == (uint32_t)(initial + (form ? 40u : 4u)));
        }
    }
    if (strcmp(events, expected)) { printf("events %s expected %s\n", events, expected); return 1; }
    return 0;
}

int main(void) {
    const uint32_t targets[] = {0, 0xFFF, 0x1000, 0x1FFF, 0x2000, 0xFDFFFFFF, 0xFE000000, UINT32_MAX};
    for (unsigned form = 0; form < 4; ++form)
        for (unsigned t = 0; t < sizeof(targets)/sizeof(targets[0]); ++t)
            for (unsigned available = 0; available < 8; ++available)
                for (unsigned options = 0; options < 16; ++options)
                    if (run_case(form, targets[t], available, options & 1,
                                 options & 2, options & 4, options & 8 ? UINT32_MAX - 7 : 100)) return 1;
    puts("4096 dispatch state/event cases passed");
    return 0;
}
