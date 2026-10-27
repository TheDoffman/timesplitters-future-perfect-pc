/* Original call-boundary oracle: architectural values and expression counts,
 * plus synthetic HLE stack frames, including nested and wrapped cleanup. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "under_test.h"
RECOMP_TLS uint32_t g_eax, g_ebx, g_esi, g_edi, g_esp;
ptrdiff_t g_xbox_mem_offset;
static uint32_t ram[1024];
static unsigned bad, fn_reads, va_reads, pop_reads, reports, calls;
static unsigned corrupt, getter_corrupt;
static uint32_t initial_stack, delta, logged_pop;
#define CHECK(c) do { if (!(c)) { printf("line %d\n", __LINE__); return 1; } } while (0)

static void call_body(void) {
    ++calls;
    if (corrupt & 1) g_ebx ^= 1;
    if (corrupt & 2) g_esi ^= 1;
    if (corrupt & 4) g_edi ^= 1;
    g_esp += delta;
    g_eax = 0xAABB;
}
static void (*target_expression(void))(void) {
    ++fn_reads;
    if (getter_corrupt) g_ebx ^= 0x80;
    return call_body;
}
static uint32_t address_expression(void) { ++va_reads; return 0x9000; }
static uint32_t cleanup_expression(void) { ++pop_reads; return 8; }
void recomp_abi_violation_log(uint32_t address, uint32_t b, uint32_t s, uint32_t d, uint32_t p) {
    if (address != 0x9000 || b != 11 || s != 22 || d != 33 || p != initial_stack) bad = 1;
    ++reports; g_eax = 0xBEEF; g_esp ^= 1;
}
void recomp_abi_pop_violation_log(uint32_t address, uint32_t b, uint32_t s, uint32_t d,
                                uint32_t p, uint32_t pop) {
    logged_pop = pop; recomp_abi_violation_log(address,b,s,d,p);
}

static int checks(void) {
    const uint32_t deltas[] = {0, 3, 4, 8, 12, 16, UINT32_MAX};
    for (unsigned exact = 0; exact < 2; ++exact)
    for (unsigned wrap = 0; wrap < 2; ++wrap)
    for (unsigned mask = 0; mask < 16; ++mask)
    for (unsigned i = 0; i < sizeof(deltas)/sizeof(deltas[0]); ++i) {
        unsigned expected_report = 0, expected_pop = 0;
        initial_stack = wrap ? UINT32_MAX - 3 : 100;
        delta = deltas[i]; corrupt = mask & 7; getter_corrupt = mask & 8;
        g_esp = initial_stack; g_ebx = 11; g_esi = 22; g_edi = 33;
        bad = fn_reads = va_reads = pop_reads = reports = calls = 0; logged_pop = 99;
        if (exact) RECOMP_ABI_CALL_POP(address_expression(), target_expression(), cleanup_expression());
        else RECOMP_ABI_CALL(address_expression(), target_expression());
#ifdef RECOMP_ABI_CHECK
        expected_report = mask != 0 || (exact ? (uint32_t)(initial_stack + delta) != (uint32_t)(initial_stack + 12) :
                                                  (uint32_t)(initial_stack + delta) < (uint32_t)(initial_stack + 4));
        expected_pop = exact ? (mask ? 1u : 1u + expected_report) : 0u;
#endif
        CHECK(!bad && fn_reads == 1 && calls == 1);
        CHECK(reports == expected_report && va_reads == expected_report && pop_reads == expected_pop);
        CHECK(g_eax == (expected_report ? 0xBEEF : 0xAABB));
        CHECK(g_esp == ((uint32_t)(initial_stack + delta) ^ (expected_report ? 1u : 0u)));
        CHECK(g_ebx == (11u ^ (getter_corrupt ? 0x80u : 0u) ^ ((corrupt & 1) ? 1u : 0u)));
        CHECK(g_esi == (22u ^ ((corrupt & 2) ? 1u : 0u)));
        CHECK(g_edi == (33u ^ ((corrupt & 4) ? 1u : 0u)));
        if (expected_report && exact) CHECK(logged_pop == 8);
    }
    return 0;
}

static uint32_t frame_count, frame_entry, frame_work, frame_return, frame_cleanup;
static unsigned sequence, n_reads, hle_pop_reads, hle_fn_reads, nested;
static uint32_t frame_n(void) { ++n_reads; if (sequence++ != 0) bad = 1; return frame_count; }
static uint32_t frame_pop(void) {
    ++hle_pop_reads; if (sequence++ != 1) bad = 1;
    g_esp += 64; return frame_cleanup;
}
static void inner_hle(void) {
    if (MEM32(g_esp) != frame_return || MEM32(g_esp + 4) != 0x12345678) bad = 1;
    g_esp = 3; g_edi = 0x7777;
}
static void hle_body(void) {
    ++calls;
    if (g_esp != frame_work || MEM32(g_esp) != frame_return) bad = 1;
    for (uint32_t i = 0; i < frame_count && i < 24; ++i)
        if (MEM32(g_esp + 4u * (i + 1u)) != 0xAB000000u + i) bad = 1;
    if (nested) {
        uint32_t outer = g_esp;
        RECOMP_HLE_LTCG_CALL(1, inner_hle, 4)
            a[0] = 0x12345678;
        RECOMP_HLE_LTCG_END
        if (g_esp != outer + 4u || g_edi != 0x7777) bad = 1;
    }
    g_esp = 7; g_eax = 0xCC55;
}
static void (*frame_target(void))(void) {
    ++hle_fn_reads; if (sequence++ != 2) bad = 1;
    g_esp += 32; return hle_body;
}
static int frames(void) {
    const uint32_t counts[] = {0, 1, 4, 24, 25, UINT32_MAX};
    for (unsigned high = 0; high < 2; ++high)
    for (unsigned nest = 0; nest < 2; ++nest)
    for (unsigned j = 0; j < sizeof(counts)/sizeof(counts[0]); ++j) {
        uint32_t base = high ? 0xFFFFF000u : 0;
        frame_entry = base + 0xF00u; frame_count = counts[j]; frame_cleanup = high ? 512 : 12;
        frame_work = frame_entry - 4u * (frame_count + 4u); frame_return = 0xABCDE123;
        g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)ram - (ptrdiff_t)(uint64_t)base;
        memset(ram, 0xCD, sizeof(ram)); g_esp = frame_entry;
        MEM32(frame_entry) = frame_return; MEM32(frame_entry + 4) = 0xAB000000;
        sequence = n_reads = hle_pop_reads = hle_fn_reads = calls = bad = 0; nested = nest;
        RECOMP_HLE_LTCG_CALL(frame_n(), frame_target(), frame_pop())
            if (sequence++ != 3 || RECOMP_HLE_STACK(0) != 0xAB000000 || g_esp != (uint32_t)(frame_entry + 96)) bad = 1;
            for (unsigned i = 0; i < 24; ++i) a[i] = 0xAB000000u + i;
        RECOMP_HLE_LTCG_END
        CHECK(!bad && calls == 1 && n_reads == 1 && hle_pop_reads == 1 && hle_fn_reads == 1 && sequence == 4);
        CHECK(g_esp == (uint32_t)(frame_entry + frame_cleanup) && g_eax == 0xCC55);
        if (!nested) {
            uint32_t written = frame_count < 24 ? frame_count : 24;
            /* A canary after the maximum supported argument frame is untouched
             * except where it overlaps the caller's pre-existing return/arg. */
            uint32_t after = frame_work + 4u * (written + 1u);
            if (after != frame_entry && after != frame_entry + 4)
                CHECK(MEM32(after) == 0xCDCDCDCDu);
        }
    }
    return 0;
}
int main(void) {
    if (checks() || frames()) return 1;
    puts("448 ABI cases and 24 HLE frame cases passed");
    return 0;
}
