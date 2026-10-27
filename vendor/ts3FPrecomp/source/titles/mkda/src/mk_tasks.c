/*
 * Mortal Kombat: Deadly Alliance -- the engine's task scheduler, on host fibers.
 *
 * The title runs most of its game logic as cooperative tasks ("processes" in
 * Midway's engine). sub_000E0350 walks the task list once a frame and, for each
 * task whose delay has run out, calls the task's resume method (vtable +0x14).
 * The task runs until it sleeps (vtable +0x18), replaces its own function
 * (+0x24), dies (sub_000E0550, called directly) or returns from its function
 * with a float delay in st0. Three classes, one method layout:
 *
 *   vtable    class  resume   sleep    to big   from big  change   where it runs
 *   0x2702A0  C      E05D0    E0680*   E08D0    E08D0     E08E0    main stack, never sleeps
 *   0x2702C8  A      E0690    E0800    E08D0    E08D0     E08E0    main stack, copied out on sleep
 *   0x2702F0  B      E0920    E0A50    E0AC0    E0AE0     E0B00    its own stack inside the task
 *
 *   * E0680 falls straight into class A's resume; class C cannot sleep.
 *
 * Every one of those transfers is a hand-written stack switch ending in
 * `jmp [saved address]`, and the address is in the middle of a function -- the
 * scheduler's own call site at 0x000E0535, or the line after a sleep inside a
 * task. A lifted function is a host C function, so there is no way to enter one
 * part-way through, and the jumps were refused a billion times a run
 * (docs/technical/mkda-coroutine.md).
 *
 * This file gives every running task a host fiber. A sleep parks the fiber and
 * a resume switches back to it, so the lifted C frames of the task -- host
 * locals, the guest ebp each of them holds -- survive the way the guest stack
 * does on hardware. "Die" and "change function" unwind the fiber to its root
 * with longjmp, which is what the guest's `mov esp, <root>; ret` means.
 *
 * The guest side is kept exactly: class A's stack is still copied out to the
 * buffer in the task object on a sleep and copied back on a resume, because
 * other class A tasks reuse the same stretch of the main stack and a task may
 * hold pointers to its own locals. The scheduler globals are still written, so
 * anything that reads them sees what it would on hardware.
 *
 * Task layout, as far as the scheduler uses it:
 *   +0x00 vtable   +0x04 linked (0 = do not run again)   +0x0C delay (float)
 *   +0x10..+0x1C saved ebp, ebx, esi, edi    +0x20 flags
 *   +0x2C post-run hook   +0x30 task function   +0x34 continuation
 *   +0x40 top of the task's stack buffer   +0x44 its saved stack pointer
 *   (+0x40 > +0x44 means "suspended mid-function": the test every resume makes)
 */

#include <windows.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "recomp/gen/recomp_types.h"

#define MK_CUR_TASK      0x00321208u   /* the task being run */
#define MK_RUNNING       0x003211F8u   /* cleared when a task finishes */
#define MK_SLEEP_DELAY   0x00321200u   /* the delay a sleep stores */
#define MK_TASK_RET      0x002D4D30u   /* scratch: a task's return address */
#define MK_SCHED_RET     0x002D4D34u   /* where the scheduler resumes */
#define MK_SCHED_ESP     0x002D4D44u   /* the scheduler's stack pointer */
#define MK_BIG_SAVE      0x002D4D48u   /* class B: its own esp while on the main stack */

#define T_LINK   0x04u
#define T_DELAY  0x0Cu
#define T_EBX    0x14u
#define T_ESI    0x18u
#define T_EDI    0x1Cu
#define T_HOOK   0x2Cu
#define T_FN     0x30u
#define T_CONT   0x34u
#define T_TOP    0x40u
#define T_SP     0x44u

enum mk_kind { MK_A, MK_B, MK_C };

/* The return addresses the original pushes, per class: the root call of the
 * task function, the post-run hook and the destroy call. Only stack scans and
 * crash reports read them -- and class A's stack copy, which carries the root
 * return address with it -- but there is no reason to push anything else. */
static const struct {
    uint32_t root_ret, hook_ret, destroy_ret;
    const char *name;
} mk_class[] = {
    [MK_A] = { 0x000E0761u, 0x000E07A7u, 0x000E07F2u, "A" },
    [MK_B] = { 0x000E0972u, 0x000E09B8u, 0x000E0A26u, "B" },
    [MK_C] = { 0x000E05EAu, 0x000E061Fu, 0x000E0669u, "C" },
};

enum { MK_FREE, MK_RUNNING_STATE, MK_PARKED };
/* longjmp codes to a fiber's root. */
enum { MK_JMP_CHANGE = 1, MK_JMP_DIE, MK_JMP_ABORT };

typedef struct {
    LPVOID fiber;
    LPVOID back;          /* the fiber that resumed this one, to return to */
    uint32_t task;        /* guest task object, 0 when free */
    int kind;
    int state;
    int abort;            /* set before waking a parked fiber that must unwind */
    float jmp_delay;      /* the delay a change-function hands to the root */
    uint32_t park_esp;    /* guest esp at the sleep, after popping the return */
    uintptr_t stack_lo, stack_hi;   /* its native stack, once it has run */
    jmp_buf root;
} mk_fiber;

/* Comfortably more than the title has been seen to run at once; the log says
 * so if it ever is not. */
#define MK_MAX_FIBERS 512
static mk_fiber mk_fibers[MK_MAX_FIBERS];
static int mk_nfibers;
static int mk_parked_now, mk_parked_peak;

/* The fiber the scheduler thread became, and its native stack. */
static LPVOID mk_main_fiber;
static uintptr_t mk_main_lo, mk_main_hi;

/* A guest longjmp handed over from another fiber (mk_foreign_longjmp): the
 * fiber that owns the buffer picks it up when it next runs. */
static struct {
    jmp_buf *target;
    int value;
    mk_fiber *from;       /* the task fiber it came from, now dead */
} mk_pending;

void mk_task_report(void);

static int mk_log_on = -1;
static int mk_log(void)
{
    if (mk_log_on < 0) {
        const char *v = getenv("RECOMP_MK_TASK_LOG");
        mk_log_on = v && *v && strcmp(v, "0") != 0;
    }
    return mk_log_on;
}

/* Call a guest function the way lifted code does: push the return address,
 * dispatch, and let the callee pop it. */
static void mk_call(uint32_t va, uint32_t ret_va)
{
    recomp_func_t fn;

    PUSH32(g_esp, ret_va);
    fn = recomp_lookup_manual(va);
    if (!fn)
        fn = recomp_lookup(va);
    if (fn) {
        fn();
    } else {
        fprintf(stderr, "[MKTASK] task calls 0x%08X, which is not a lifted "
                        "function; skipped\n", va);
        g_esp += 4;
        g_eax = 0;
    }
}

static float mk_pop_st0(void)
{
    double v = g_fp_stack[g_fp_top & 7];
    g_fp_top = (g_fp_top + 1) & 7;
    return (float)v;
}

static mk_fiber *mk_self(void)
{
    LPVOID cur;
    int i;

    if (!IsThreadAFiber())
        return NULL;
    cur = GetCurrentFiber();
    for (i = 0; i < mk_nfibers; i++)
        if (mk_fibers[i].fiber == cur && mk_fibers[i].state == MK_RUNNING_STATE)
            return &mk_fibers[i];
    return NULL;
}

static mk_fiber *mk_find_parked(uint32_t task)
{
    int i;
    for (i = 0; i < mk_nfibers; i++)
        if (mk_fibers[i].state == MK_PARKED && mk_fibers[i].task == task)
            return &mk_fibers[i];
    return NULL;
}

/* After any switch lands: if another fiber handed this one a guest longjmp,
 * this is the stack it belongs to, so do it here. The fiber it came from can
 * never run again -- its frames sit above a jump that left them -- so it is
 * deleted and its slot gets a new fiber when next used. */
static void mk_take_pending(void)
{
    jmp_buf *t = mk_pending.target;
    mk_fiber *from = mk_pending.from;

    if (!t)
        return;
    mk_pending.target = NULL;
    mk_pending.from = NULL;
    if (from) {
        if (mk_log())
            fprintf(stderr, "[MKTASK] task 0x%08X (class %s): its fiber jumped "
                            "out to another stack and is retired\n",
                    from->task, mk_class[from->kind].name);
        DeleteFiber(from->fiber);
        from->fiber = NULL;
        from->task = 0;
        from->abort = 0;
        from->state = MK_FREE;
    }
    longjmp(*t, mk_pending.value);
}

/* Switch from a task fiber back to whoever resumed it. */
static void mk_leave(mk_fiber *f)
{
    SwitchToFiber(f->back);
}

/* Unwind fiber f to its root. Logged first, so a failed unwind names itself. */
static void mk_unwind(mk_fiber *f, int why)
{
    if (mk_log()) {
        volatile char here;
        fprintf(stderr, "[MKTASK] task 0x%08X (class %s, fiber %d): %s, "
                        "unwinding %lld bytes of host stack\n",
                f->task, mk_class[f->kind].name, (int)(f - mk_fibers),
                why == MK_JMP_CHANGE ? "change of function"
                    : why == MK_JMP_DIE ? "die" : "abandon",
                (long long)((char *)&f->root - (char *)&here));
        fflush(stderr);
    }
    longjmp(f->root, why);
}

/* Sleep: park the fiber. Returns when the task is resumed. */
static void mk_park(mk_fiber *f)
{
    uint32_t seh = g_seh_ebp, fp = g_ebp;

    f->state = MK_PARKED;
    mk_parked_now++;
    if (mk_parked_now > mk_parked_peak)
        mk_parked_peak = mk_parked_now;
    mk_leave(f);
    /* resumed */
    mk_parked_now--;
    f->state = MK_RUNNING_STATE;
    mk_take_pending();
    g_seh_ebp = seh;
    g_ebp = fp;
    if (f->abort)
        mk_unwind(f, MK_JMP_ABORT);
}

/* The run loop the resume methods share: call the task function until it asks
 * for a non-zero delay, run the post hook, and destroy the task if the delay is
 * negative. `entry` says how the loop was entered: 0 fresh, else a change of
 * function, which arrives with its delay as though the old function returned
 * it. */
static void mk_run(mk_fiber *f, int entry)
{
    const uint32_t task = f->task;
    const int k = f->kind;
    uint32_t stack;
    float d;

    if (entry == 0) {
        /* Class B starts on its own stack; A and C on the scheduler's. */
        stack = (k == MK_B) ? MEM32(task + T_SP) : MEM32(MK_SCHED_ESP);
    } else {
        /* A change of function unwinds to the root: the original sets esp to
         * one below the root's frame and returns into the loop. */
        stack = (k == MK_B) ? MEM32(task + T_TOP) : MEM32(MK_SCHED_ESP);
        d = f->jmp_delay;
        goto have_delay;
    }

    for (;;) {
        g_esp = stack;
        mk_call(MEM32(task + T_FN), mk_class[k].root_ret);
        d = mk_pop_st0();
    have_delay:
        g_esp = stack;
        MEMF(task + T_DELAY) = d;
        if (k != MK_C)
            MEM32(task + T_SP) = MEM32(task + T_TOP);
        /* `fcomp 0; test ah, 0x44; jp`: go round again only on an ordered 0 */
        if (!(MEMF(task + T_DELAY) == 0.0f))
            break;
    }

    if (MEM32(task + T_HOOK)) {
        mk_call(MEM32(task + T_HOOK), mk_class[k].hook_ret);
        if (MEM32(task + T_LINK) == 0)
            return;
    }
    /* `test ah, 5; jp`: destroy only on an ordered "less than 0" */
    if (MEMF(task + T_DELAY) < 0.0f) {
        g_esp = MEM32(MK_SCHED_ESP);
        MEM32(MK_RUNNING) = 0;
        if (MEM32(task + T_LINK)) {
            PUSH32(g_esp, task);
            mk_call(MEM32(MEM32(task) + 0x10), mk_class[k].destroy_ret);
            g_esp += 4;
        }
    }
}

static void WINAPI mk_fiber_main(void *p)
{
    mk_fiber *f = (mk_fiber *)p;
    ULONG_PTR lo, hi;

    GetCurrentThreadStackLimits(&lo, &hi);
    f->stack_lo = lo;
    f->stack_hi = hi;

    for (;;) {
        int r = setjmp(f->root);
        if (r == 0)
            mk_run(f, 0);
        else if (r == MK_JMP_CHANGE)
            mk_run(f, 1);
        /* MK_JMP_DIE and MK_JMP_ABORT: the task is over, nothing to run */
        if (mk_log())
            fprintf(stderr, "[MKTASK] task 0x%08X (class %s) %s\n", f->task,
                    mk_class[f->kind].name,
                    r == MK_JMP_DIE ? "died" : r == MK_JMP_ABORT
                        ? "abandoned while asleep" : "finished");
        f->task = 0;
        f->abort = 0;
        f->state = MK_FREE;
        mk_leave(f);
        /* reused for another task: round again */
    }
}

static mk_fiber *mk_alloc(void)
{
    int i;

    for (i = 0; i < mk_nfibers; i++)
        if (mk_fibers[i].state == MK_FREE)
            break;
    if (i < mk_nfibers && mk_fibers[i].fiber)
        return &mk_fibers[i];
    if (i < mk_nfibers)
        goto create;   /* a retired slot: same index, new fiber */
    if (mk_nfibers == MK_MAX_FIBERS) {
        fprintf(stderr, "[MKTASK] out of fibers: %d tasks are asleep at once\n",
                MK_MAX_FIBERS);
        fflush(stderr);
        abort();
    }
    i = mk_nfibers++;
create:
    /* The same reserve as the main thread's stack: lifted frames are large,
     * and a task runs the same code the main thread would. */
    mk_fibers[i].fiber = CreateFiberEx(64 * 1024, 2 * 1024 * 1024, 0,
                                       mk_fiber_main, &mk_fibers[i]);
    if (!mk_fibers[i].fiber) {
        fprintf(stderr, "[MKTASK] CreateFiberEx failed (%lu)\n", GetLastError());
        fflush(stderr);
        abort();
    }
    mk_fibers[i].stack_lo = mk_fibers[i].stack_hi = 0;   /* until it runs */
    mk_fibers[i].state = MK_FREE;
    return &mk_fibers[i];
}

/* Run fiber f until it parks or finishes, then come back here. */
static void mk_enter(mk_fiber *f)
{
    /* The frame pointers lifted code publishes for frameless callees: each
     * side of a switch has its own, as each would have its own ebp. */
    uint32_t seh = g_seh_ebp, fp = g_ebp;

    f->back = GetCurrentFiber();
    if (f->state != MK_PARKED)
        f->state = MK_RUNNING_STATE;
    SwitchToFiber(f->fiber);
    mk_take_pending();   /* before restoring: a longjmp brings its own */
    g_seh_ebp = seh;
    g_ebp = fp;
}

/* Wake a parked fiber only to unwind it: its task was restarted or replaced
 * while it slept (sub_000E0060 does that from outside the task). */
static void mk_abandon(mk_fiber *f)
{
    f->abort = 1;
    mk_enter(f);
}

/* The runtime's hook for a guest longjmp to a buffer armed on another native
 * stack. MKDA's main loop arms one (sub_000CAB60, 0x000CACAE) and tasks jump
 * back to it: on hardware every task stack is the main stack or memory beside
 * it, so that is an ordinary longjmp. Here the buffer is on the main fiber's
 * stack and the jump comes from a task fiber, which Windows refuses to unwind
 * (STATUS_BAD_STACK). Hand it to the fiber that owns the stack. */
static void mk_foreign_longjmp(jmp_buf *target, int value, uintptr_t armed)
{
    mk_fiber *self = mk_self();
    LPVOID owner = NULL;
    int i;

    if (armed >= mk_main_lo && armed < mk_main_hi)
        owner = mk_main_fiber;
    for (i = 0; !owner && i < mk_nfibers; i++)
        if (mk_fibers[i].fiber && armed >= mk_fibers[i].stack_lo
                && armed < mk_fibers[i].stack_hi)
            owner = mk_fibers[i].fiber;
    if (!owner || !IsThreadAFiber() || owner == GetCurrentFiber()) {
        fprintf(stderr, "[MKTASK] cannot place that longjmp: no fiber owns "
                        "its stack\n");
        fflush(stderr);
        return;
    }
    fprintf(stderr, "[MKTASK] guest longjmp from task 0x%08X to the %s stack\n",
            self ? self->task : 0, owner == mk_main_fiber ? "main" : "a task's");
    fflush(stderr);
    mk_pending.target = target;
    mk_pending.value = value;
    mk_pending.from = self;
    if (self)
        self->state = MK_FREE;   /* not findable as running or asleep */
    SwitchToFiber(owner);
    /* never resumed: mk_take_pending deletes this fiber */
}

/* vtable +0x14 for all three classes. */
static void mk_resume(int kind)
{
    uint32_t ret, task, sched_esp;
    mk_fiber *f;
    int suspended;
    static int first = 1;

    if (first) {
        first = 0;
        if (!IsThreadAFiber() && !ConvertThreadToFiber(NULL)) {
            fprintf(stderr, "[MKTASK] ConvertThreadToFiber failed (%lu)\n",
                    GetLastError());
            fflush(stderr);
            abort();
        }
        {
            ULONG_PTR lo, hi;
            GetCurrentThreadStackLimits(&lo, &hi);
            mk_main_lo = lo;
            mk_main_hi = hi;
        }
        mk_main_fiber = GetCurrentFiber();
        recomp_set_foreign_longjmp(mk_foreign_longjmp);
        fprintf(stderr, "[MKTASK] task scheduler on host fibers "
                        "(mk_tasks.c; RECOMP_MK_TASK_LOG=1 for each task)\n");
        atexit(mk_task_report);
    }

    task = MEM32(MK_CUR_TASK);
    POP32(g_esp, ret);
    sched_esp = g_esp;
    MEM32(MK_SCHED_RET) = ret;
    MEM32(MK_SCHED_ESP) = sched_esp;

    /* Class C never checks for a saved stack: it cannot have one. */
    suspended = kind != MK_C && MEM32(task + T_TOP) > MEM32(task + T_SP);
    f = mk_find_parked(task);

    if (suspended && f && f->kind == kind) {
        if (kind == MK_A) {
            /* E0690: copy the saved stack back below the scheduler's frame */
            uint32_t lo = MEM32(task + T_SP), hi = MEM32(task + T_TOP);
            uint32_t size = hi - lo, dst = sched_esp - size;
            memmove((void *)XBOX_PTR(dst), (const void *)XBOX_PTR(lo), size);
            if (dst != f->park_esp) {
                static int said;
                if (!said++)
                    fprintf(stderr, "[MKTASK] class A task 0x%08X resumed with "
                                    "its stack at 0x%08X, put to sleep at 0x%08X: "
                                    "the scheduler's esp moved\n",
                            task, dst, f->park_esp);
            }
            g_esp = dst;
        } else {
            g_esp = MEM32(task + T_SP);   /* E0920 */
        }
        g_ebx = MEM32(task + T_EBX);
        g_esi = MEM32(task + T_ESI);
        g_edi = MEM32(task + T_EDI);
        mk_enter(f);
    } else {
        if (f)
            mk_abandon(f);   /* asleep, but the task says it starts afresh */
        if (suspended) {
            static int said;
            if (!said++)
                fprintf(stderr, "[MKTASK] task 0x%08X (class %s) has a saved "
                                "stack but no fiber asleep for it; starting it "
                                "afresh\n", task, mk_class[kind].name);
        }
        f = mk_alloc();
        f->task = task;
        f->kind = kind;
        f->abort = 0;
        mk_enter(f);
    }

    /* Back in the scheduler, exactly as `jmp [0x2D4D34]` with esp and ebp
     * restored would have left it. ebx/esi/edi are what the task left: the
     * scheduler does not use them and its caller restores its own. */
    g_esp = sched_esp;
}

/* Sleep, the class A form (E0800): save the registers and the stack. */
static void mk_sleep(int kind)
{
    uint32_t task = MEM32(MK_CUR_TASK), ret;
    mk_fiber *f = mk_self();

    MEM32(task + T_EBX) = g_ebx;
    MEM32(task + T_ESI) = g_esi;
    MEM32(task + T_EDI) = g_edi;
    MEM32(task + T_DELAY) = MEM32(MK_SLEEP_DELAY);
    POP32(g_esp, ret);
    MEM32(MK_TASK_RET) = ret;
    MEM32(task + T_CONT) = ret;

    if (kind == MK_A) {
        /* Copy [esp, scheduler esp) to the buffer that ends at +0x40. */
        uint32_t lo = g_esp, hi = MEM32(MK_SCHED_ESP);
        uint32_t size = hi > lo ? hi - lo : 0, top = MEM32(task + T_TOP);
        memmove((void *)XBOX_PTR(top - size), (const void *)XBOX_PTR(lo), size);
        MEM32(task + T_SP) = top - size;
    } else {
        MEM32(task + T_SP) = g_esp;
    }

    if (MEM32(task + T_HOOK))
        mk_call(MEM32(task + T_HOOK), kind == MK_A ? 0x000E08B2u : 0x000E0A9Bu);

    if (!f) {
        static int said;
        if (!said++)
            fprintf(stderr, "[MKTASK] task 0x%08X sleeps outside a task fiber; "
                            "returning to it at once\n", task);
        return;
    }
    f->park_esp = g_esp;
    mk_park(f);
    /* Resumed: mk_resume put esp and the callee-saved registers back. */
}

/* Change the task's function (E08E0, E0B00) and return to the root loop with
 * the given delay, abandoning everything the task had on its stack. */
static void mk_change(void)
{
    uint32_t task = MEM32(MK_CUR_TASK);
    uint32_t fn = MEM32(g_esp + 4), delay_bits = MEM32(g_esp + 8);
    mk_fiber *f = mk_self();
    float d;

    memcpy(&d, &delay_bits, sizeof d);
    MEM32(task + T_FN) = fn;
    MEM32(0x002D4D2Cu) = delay_bits;
    if (!f) {
        fprintf(stderr, "[MKTASK] change of function outside a task fiber; "
                        "ignored\n");
        g_esp += 4;
        return;
    }
    f->jmp_delay = d;
    mk_unwind(f, MK_JMP_CHANGE);
}

/* Die (E0550). Called directly, from anywhere in a task. */
static void mk_die(void)
{
    uint32_t task = MEM32(MK_CUR_TASK);
    mk_fiber *f = mk_self();

    if (MEM32(task + T_HOOK)) {
        mk_call(MEM32(task + T_HOOK), 0x000E0564u);
        if (MEM32(task + T_LINK) == 0)
            goto out;
    }
    g_esp = MEM32(MK_SCHED_ESP);
    MEM32(MK_RUNNING) = 0;
    if (MEM32(task + T_LINK)) {
        PUSH32(g_esp, task);
        mk_call(MEM32(MEM32(task) + 0x10), 0x000E05BCu);
        g_esp += 4;
    }
out:
    if (!f) {
        static int said;
        if (!said++)
            fprintf(stderr, "[MKTASK] task 0x%08X dies outside a task fiber\n",
                    task);
        return;
    }
    mk_unwind(f, MK_JMP_DIE);
}

/* Class B's move onto the main stack (E0AC0) and back (E0AE0), for work too
 * deep for the task's own buffer. Pure guest stack-pointer moves. */
static void mk_to_big_stack(void)
{
    uint32_t ret;
    POP32(g_esp, ret);
    MEM32(MK_TASK_RET) = ret;
    MEM32(MK_BIG_SAVE) = g_esp;
    g_esp = MEM32(MK_SCHED_ESP);
}

static void mk_from_big_stack(void)
{
    uint32_t ret;
    POP32(g_esp, ret);
    MEM32(MK_TASK_RET) = ret;
    g_esp = MEM32(MK_BIG_SAVE);
    MEM32(MK_BIG_SAVE) = 0;
}

/* The entry points recomp_manual.c defines under the guest names. */
void mk_task_resume_a(void) { mk_resume(MK_A); }
void mk_task_resume_b(void) { mk_resume(MK_B); }
void mk_task_resume_c(void) { mk_resume(MK_C); }
void mk_task_sleep_a(void)  { mk_sleep(MK_A); }
void mk_task_sleep_b(void)  { mk_sleep(MK_B); }
void mk_task_change(void)   { mk_change(); }
void mk_task_die(void)      { mk_die(); }
void mk_task_to_big(void)   { mk_to_big_stack(); }
void mk_task_from_big(void) { mk_from_big_stack(); }

void mk_task_sleep_c(void)
{
    static int said;
    uint32_t ret;
    if (!said++)
        fprintf(stderr, "[MKTASK] a class C task tried to sleep (E0680); "
                        "class C has nowhere to keep a stack. Ignored\n");
    POP32(g_esp, ret);
    (void)ret;
}

/* At exit: how many fibers it took. */
void mk_task_report(void)
{
    fprintf(stderr, "[MKTASK] %d fiber(s) created, at most %d asleep at once\n",
            mk_nfibers, mk_parked_peak);
}
