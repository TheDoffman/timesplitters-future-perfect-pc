/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>   /* getenv, exit: the spin verdict below */
#include <string.h>   /* strcmp: RECOMP_ICALL_FATAL */

/* One diagnostic report, one piece.
 *
 * Each report below is a dozen fprintf calls and the CRT takes its lock per
 * call, so with more than one guest thread running the lines interleave in
 * the middle of a report. Dino Crisis 3 runs six workers once it is past its
 * menus, and its log came out like this:
 *
 *   callers: 0x004DB424 <- 0x002705B4 <- 0x002705B4[ICALL] unresolved jump
 *   target 0x41B0B870 -- 1 time(s) (total calls: 6930990)
 *
 * -- two threads' reports spliced together, with a caller list that stops
 * mid-sentence and an address that belongs to neither line as read. These
 * diagnostics exist to be read during exactly the multi-threaded bring-up
 * that breaks them.
 *
 * The CRT exposes the lock these calls were already taking one at a time, so
 * hold it across the whole report instead. */
#if defined(_MSC_VER)
#  define RECOMP_DIAG_LOCK()   _lock_file(stderr)
#  define RECOMP_DIAG_UNLOCK() _unlock_file(stderr)
#elif defined(__unix__) || defined(__APPLE__)
#  define RECOMP_DIAG_LOCK()   flockfile(stderr)
#  define RECOMP_DIAG_UNLOCK() funlockfile(stderr)
#else
#  define RECOMP_DIAG_LOCK()   ((void)0)
#  define RECOMP_DIAG_UNLOCK() ((void)0)
#endif

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

/* These are defined thread-local in xbox_memory_layout.c. Declaring them
 * without the same storage class here does not fail to link -- it silently
 * resolves to different storage, so every read gets 0. That is why the log
 * below reported no call site: not because the guest esp was stale, but
 * because this file was not reading the guest esp at all. */
#if defined(_MSC_VER)
#  define RECOMP_MANUAL_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#  define RECOMP_MANUAL_TLS __thread
#else
#  define RECOMP_MANUAL_TLS _Thread_local
#endif

extern RECOMP_MANUAL_TLS uint32_t g_eax;
/* The guest stack pointer. At the moment an indirect call is refused, the
 * caller has already pushed its guest return address, so the top of the
 * guest stack is the call site -- the one thing the old log did not say. */
extern RECOMP_MANUAL_TLS uint32_t g_esp;
extern ptrdiff_t g_xbox_mem_offset;
/* The lifted code sections, so a value on the guest stack can be told
 * apart from data when naming the callers of a refused call. */
extern uint32_t g_xbox_code_lo;
extern uint32_t g_xbox_code_hi;
/* The esp the dispatch macro captured. Not g_esp, which is stale by the time
 * a refused call is reported. Zero when the title's generated header predates
 * this, and the log then says it has no callers rather than inventing them. */
extern RECOMP_MANUAL_TLS uint32_t g_icall_saved_esp;
/* Which dispatch form was refused: 0 unknown, 1 call, 2 jump. Unknown
 * means this title was lifted before the macros published it. */
extern RECOMP_MANUAL_TLS uint32_t g_icall_dispatch_form;

/* ── MvC2: shop points (RECOMP_MVC2_MAX_POINTS) ────────────── */

/*
 * Opt-in: set the shop's points balance to 9999 so the player can buy
 * characters, art and colours through the game's own shop. Off by default.
 * RECOMP_MVC2_MAX_POINTS=1, or `max_points = 1` in the title's settings file
 * (the environment wins). See docs/technical/mvc2-unlocks.md.
 *
 * The balance is a u16 at guest 0x676422; the shop spends from it and caps it
 * at 9999 (sub_0001A9D0). Two functions write it from outside the shop:
 *
 *   sub_001DEBA0  a new profile's defaults
 *   sub_001DFB40  save block -> live state; al == 1 on entry is the path taken
 *                 after a successful load, and the only one that writes the
 *                 balance. The other al values copy other things and are left
 *                 alone.
 *
 * Each wrapper runs the generated body and then overwrites the balance.
 * Nothing is done at save time (sub_001E06B0), so the balance and anything
 * bought with it are saved into the real profile.
 */
#include "recomp_config.h"

extern void sub_001DEBA0_gen(void);
extern void sub_001DFB40_gen(void);

#define MVC2_POINTS_VA  0x00676422u
#define MVC2_POINTS_MAX 9999u        /* the shop's own cap, 0x270F */

static int mvc2_max_points_enabled(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = recomp_config_bool("RECOMP_MVC2_MAX_POINTS", "max_points", 0);
    return enabled;
}

static void mvc2_apply_max_points(const char *where)
{
    *(volatile uint16_t *)((uintptr_t)MVC2_POINTS_VA + g_xbox_mem_offset) =
        (uint16_t)MVC2_POINTS_MAX;
    fprintf(stderr, "[MVC2] shop points set to %u (RECOMP_MVC2_MAX_POINTS, %s)\n",
            MVC2_POINTS_MAX, where);
    fflush(stderr);
}

/* A new profile's defaults. */
void sub_001DEBA0(void)
{
    sub_001DEBA0_gen();
    if (mvc2_max_points_enabled())
        mvc2_apply_max_points("new profile");
}

/* Save block -> live state. al is the mode; only al == 1 (after a
 * successful load) writes the balance. Read it before the body clobbers eax. */
void sub_001DFB40(void)
{
    uint8_t mode = (uint8_t)g_eax;
    sub_001DFB40_gen();
    if (mode == 1 && mvc2_max_points_enabled())
        mvc2_apply_max_points("profile loaded");
}

/* ── MvC2: CRI ADX's idle thread ───────────────────────────── */

/*
 * sub_001FFFF0 is the idle thread CRI's ADX middleware starts: it counts
 * (0x593BB0) until its quit flag (0x593BC8) is set, then exits. On the Xbox
 * it runs only when nothing else wants the one CPU, so it measures idle time.
 * Here it had a host core to itself and spun it at 100% for the whole run --
 * and, never reaching a kernel call, it could not be parked while another
 * guest thread was at DISPATCH_LEVEL (RECOMP_DISPATCH_LOCK), which is what
 * MvC2's ADX sound thread relies on.
 *
 * So the wait yields: one count and a millisecond's sleep per pass, outside
 * lifted code. The count is only ever an idle measure, so its rate does not
 * matter. Then the generated body runs, finds the flag set, and does the
 * thread's exit path as before.
 */
extern void sub_001FFFF0_gen(void);
void xbox_GuestLiftedEnter(void);
void xbox_GuestLiftedLeave(void);
__declspec(dllimport) void __stdcall Sleep(unsigned long ms);

#define MVC2_MEM32(va) (*(volatile uint32_t *)((uintptr_t)(va) + g_xbox_mem_offset))

void sub_001FFFF0(void)
{
    while (!MVC2_MEM32(0x00593BC8u)) {
        MVC2_MEM32(0x00593BB0u) += 1;
        xbox_GuestLiftedLeave();
        Sleep(1);
        xbox_GuestLiftedEnter();
    }
    sub_001FFFF0_gen();
}

/* ── MvC2: the ring-buffer error trap (diagnostic) ─────────── */

/*
 * sub_00200F20 is `jmp $`: CRI ADX's default error callback for its stream
 * ring buffers, which the title reaches with code -3 when a buffer's
 * positions disagree (sub_002011D0). Wrapped to say what it saw first.
 * Kept: it costs nothing until it fires, and a stream error is otherwise a
 * silent hang.
 */
extern void sub_00200F20_gen(void);
extern RECOMP_MANUAL_TLS uint32_t g_ecx, g_edx, g_ebx, g_esi, g_edi;

void sub_00200F20(void)
{
    uint32_t sp = g_esp, ctx = MVC2_MEM32(sp + 4), code = MVC2_MEM32(sp + 8);
    uint32_t objs[2] = { ctx, 0x00685740u };
    int i, k;

    fprintf(stderr, "[MVC2] ADX ring-buffer error trap: ctx 0x%08X code %d "
            "ret 0x%08X; ebx %08X esi %08X edi %08X ecx %08X edx %08X\n",
            ctx, (int)code, MVC2_MEM32(sp), g_ebx, g_esi, g_edi, g_ecx, g_edx);
    for (k = 0; k < 2; k++) {
        uint32_t o = objs[k];
        if (o < 0x10000u || o > 0x03FFFF00u)
            continue;
        fprintf(stderr, "[MVC2]   object 0x%08X:", o);
        for (i = 0; i < 0x40; i += 4)
            fprintf(stderr, " %08X", MVC2_MEM32(o + i));
        fputc('\n', stderr);
    }
    fprintf(stderr, "[MVC2]   guest stack:");
    for (i = 0; i < 24; i++)
        fprintf(stderr, " %08X", MVC2_MEM32(sp + i * 4));
    fputc('\n', stderr);
    fflush(stderr);
    sub_00200F20_gen();
}

/* ── MvC2: deferred frames on by default ───────────────────── */

/*
 * MvC2 writes each fighter's new animation tiles into its sprite sheet after
 * queueing the draws that sample it, which is correct on the console because
 * the NV2A reaches those draws later. Drawn at the call, the shadow renderer
 * showed the previous pose's tiles for a frame whenever a pose changed -- the
 * sprite flicker (measured: 874 textures changed after the frame drew with
 * them in 120 s; 0 with deferral). So this title runs deferred
 * (RECOMP_HLE_D3D8_DEFER, src/hle/hle_d3d8_record.h) unless the player has
 * said otherwise: set before main, and only if the variable is not already
 * set, so RECOMP_HLE_D3D8_DEFER=0 still turns it off.
 */
#if defined(_MSC_VER)
static void __cdecl mvc2_defaults(void)
{
    if (!getenv("RECOMP_HLE_D3D8_DEFER"))
        _putenv("RECOMP_HLE_D3D8_DEFER=1");
}
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (__cdecl *mvc2_defaults_init)(void) = mvc2_defaults;
#pragma comment(linker, "/include:mvc2_defaults_init")
#endif

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    /* MvC2 shop points wrappers (above), so an indirect call reaches them
     * as well as a direct one. */
    if (xbox_va == 0x001DEBA0u) return sub_001DEBA0;
    if (xbox_va == 0x001DFB40u) return sub_001DFB40;
    if (xbox_va == 0x001FFFF0u) return sub_001FFFF0;   /* ADX idle thread */
    if (xbox_va == 0x00200F20u) return sub_00200F20;   /* ADX error trap */

    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    /*
     * Rate-limited per target, reporting at 1, 10, 100 ...
     *
     * An unresolved target inside a loop is the normal case, not the rare
     * one: Mortal Kombat: Deadly Alliance produced 46843 of these for a
     * single address in one 30-second run, seventeen lines each. That buries
     * every other diagnostic in the log and says nothing the first report did
     * not. The progression is the useful part -- one line says a target was
     * never identified, a run of them says the title is stuck on it.
     */
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }

    RECOMP_DIAG_LOCK();
    fprintf(stderr, "[ICALL] unresolved %starget 0x%08X -- %llu time(s) "
                    "(total calls: %llu)\n",
            g_icall_dispatch_form == 1 ? "call " :
            g_icall_dispatch_form == 2 ? "jump " : "",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    if (g_icall_dispatch_form == 2 && hits[i] == 1)
        fprintf(stderr, "  a jump, not a call: if this address is inside a "
                        "function rather than at its start, the guest is doing "
                        "its own control flow (a coroutine, a longjmp, or a "
                        "switch arm) and seeding it as a function will not "
                        "help\n");

    /*
     * Who called it. The lifted caller pushed its return address on the guest
     * stack, so code-range values above esp name the chain -- the same scan
     * the memory watchpoints use. It is a heuristic: stale return addresses
     * from earlier frames show up too, and the first entry is the reliable
     * one. It is still the difference between an address with no context and
     * a function to open, which is what an unresolved target needs.
     */
    {
        const uint8_t *stack = (const uint8_t *)g_xbox_mem_offset
                             + g_icall_saved_esp;
        int shown = 0;
        fprintf(stderr, "  callers:");
        for (i = 0; g_xbox_mem_offset && g_icall_saved_esp
                    && i < 160 && shown < 6; i++) {
            uint32_t v;
            memcpy(&v, stack + (size_t)i * 4, sizeof v);
            if (v >= g_xbox_code_lo && v < g_xbox_code_hi) {
                fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
                shown++;
            }
        }
        if (!shown)
            fprintf(stderr, " (none on the stack)");
        fprintf(stderr, "\n");
    }

    /* The recent-target ring buffer, once per address. What ran just before
     * an unresolved call is context for the first report and noise after. */
    if (hits[i] == 1) {
        fprintf(stderr, "  Recent ICALL targets:\n");
        for (i = 0; i < 16; i++) {
            int idx = (g_icall_trace_idx - 16 + i) & 15;
            if (g_icall_trace[idx])
                fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
        }
    }
    RECOMP_DIAG_UNLOCK();
    fflush(stderr);
}
/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va, uint32_t saved_esp)
{
    /* A power of ten, because the rate limiter above only reaches this
     * function body at 1, 10, 100 ... and the verdict has to land on one of
     * those. A hundred thousand skips of one target is unambiguous: no title
     * makes progress through that. */
    enum { SLOTS = 16, SPIN_VERDICT = 100000 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    static int said_it;   /* the verdict below is said once, not once per target */
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    {
        /* The call site, read off the guest stack. Without it the log
         * says a wild pointer was skipped but not by whom, and the
         * caller is the only thing that leads anywhere.
         *
         * It comes from the esp the dispatch macro captured, not from
         * g_esp. The lifted call site pushes its return address onto the
         * *local* esp and only syncs g_esp at certain points, so g_esp is
         * stale here -- it read 0 exactly when a null target most needed
         * explaining, which sent three rounds of Jet Set Radio Future
         * chasing inferences instead of a call site. saved_esp is the value
         * before that push, so the return address is the dword below it.
         *
         * Only for the call form. A tail jump pushes no return address, so
         * that dword is whatever the frame happened to leave there: Tony
         * Hawk's Pro Skater 2X reported "from 0x00000008" and then "from
         * 0x00000004" for the same million skipped calls, and both are
         * plainly not code. Printing a number that cannot be an address as
         * though it were the caller is worse than printing nothing, because
         * it is the one field a reader goes to next.
         *
         * So: name the form, and where the top of the stack cannot answer,
         * scan for code-range values the way the unresolved-target logger
         * above does. Stale return addresses from earlier frames come back
         * too and the first is the reliable one, but a list of real code
         * addresses is a place to start and 0x00000008 is not. */
        uint32_t caller = 0;
        if (g_icall_dispatch_form != 2 && saved_esp >= 4 && g_xbox_mem_offset)
            caller = *(const uint32_t *)((const uint8_t *)g_xbox_mem_offset
                                         + (saved_esp - 4));
        if (caller < g_xbox_code_lo || caller >= g_xbox_code_hi)
            caller = 0;
        RECOMP_DIAG_LOCK();
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped "
                        "%llu time(s) via a %s",
                va, (unsigned long long)hits[i],
                g_icall_dispatch_form == 2 ? "jump" : "call");
        if (caller)
            fprintf(stderr, " from 0x%08X", caller);
        fprintf(stderr, " (null or wild function pointer, at call #%llu)\n",
                (unsigned long long)g_icall_count);

        /* The stack scan, once per address: the same heuristic the
         * unresolved-target logger uses, and the only thing that names a
         * caller when the return address is not where it should be. */
        if (hits[i] == 1) {
            const uint8_t *stack = (const uint8_t *)g_xbox_mem_offset
                                 + saved_esp;
            int shown = 0, k;
            fprintf(stderr, "  callers:");
            for (k = 0; g_xbox_mem_offset && saved_esp
                        && k < 160 && shown < 6; k++) {
                uint32_t v;
                memcpy(&v, stack + (size_t)k * 4, sizeof v);
                if (v >= g_xbox_code_lo && v < g_xbox_code_hi) {
                    fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
                    shown++;
                }
            }
            if (!shown)
                fprintf(stderr, " (none on the stack)");
            fprintf(stderr, "\n");
        }

        /* RECOMP_ICALL_FATAL=1: fault here instead of skipping, so the crash
         * handler prints the guest call stack that reached this call.
         *
         * The caller printed above is read from the top of the guest stack,
         * which is right only when the lifted call site pushed a return
         * address there and nothing has since moved esp. When it reads 0 --
         * exactly the case where a null target most needs explaining -- the
         * line says a wild pointer was skipped and nothing about by whom. A
         * deliberate fault costs the run and buys the backtrace. */
        {
            static int fatal = -1;
            if (fatal < 0) {
                const char *v = getenv("RECOMP_ICALL_FATAL");
                fatal = v && *v && strcmp(v, "0") != 0;
            }
            if (fatal) {
                fprintf(stderr, "[ICALL] RECOMP_ICALL_FATAL: faulting here for "
                                "a backtrace\n");
                fflush(stderr);
                *(volatile int *)0 = 1;
            }
        }
    }

    /* Past a certain count this stops being a warning and becomes a verdict.
     *
     * A skipped call sets eax to 0 and returns. Zero is a fine answer for a
     * null function pointer in most code -- it reads as NULL, false, or
     * nothing -- but it is also S_OK, so in COM-shaped code a loop that
     * repeats while the result is non-negative can never leave. Panzer
     * Dragoon Orta hung exactly there, three billion skips in forty seconds,
     * with no frame ever presented.
     *
     * The value is deliberately left alone: there is no return value that is
     * right for both conventions, and guessing failure would break the
     * callers for which zero is correct. What can be fixed is the silence.
     * Say plainly that the title is hung, once, and say what to do about it.
     */
    if (hits[i] == SPIN_VERDICT && !said_it) {
        said_it = 1;
        fprintf(stderr,
            "[ICALL] ^^ this is a hang, not slow progress. One target has been\n"
            "        skipped %d times. The skip returns eax = 0, which is also\n"
            "        S_OK, so a loop testing for a non-negative result will\n"
            "        never exit. Try `py -3 -m tools.seed_from_log <log>` to\n"
            "        recover the target as a function, and read\n"
            "        docs/technical/memory-watchpoints.md for who wrote the\n"
            "        pointer. Set RECOMP_ICALL_SPIN_FATAL=1 to stop here\n"
            "        instead of spinning.\n", SPIN_VERDICT);
        fflush(stderr);
        if (getenv("RECOMP_ICALL_SPIN_FATAL")) {
            fprintf(stderr, "[ICALL] RECOMP_ICALL_SPIN_FATAL is set; exiting.\n");
            fflush(stderr);
            exit(3);
        }
    }
    RECOMP_DIAG_UNLOCK();
    fflush(stderr);
}

