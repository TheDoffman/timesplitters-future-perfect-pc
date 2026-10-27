/**
 * Guest memory watchpoints.
 *
 * Every hard bug in this project so far has come down to the same question:
 * *what wrote this?* A structure holds a value it should not, the code walks
 * into it and faults, and the fault tells you where the damage was read, not
 * where it was done. TimeSplitters 2's dark level, Marvel vs Capcom 2's
 * stalled frame loop and Outrun 2's wild pointer were all that shape.
 *
 * There is already a watch of a sort: RECOMP_WATCH_VA in recomp_trace.c polls
 * an address at every function entry. It has two limits that matter. It needs
 * a --trace-all-entries build, which costs a few percent of frame time and
 * has to be lifted specially; and it can only say "it changed somewhere
 * between entering A and entering B", which on a hot path is thousands of
 * instructions of suspects.
 *
 * This is the exact version, and it works in a plain build. It protects the
 * page, lets the write fault, and names the instruction:
 *
 *     RECOMP_WATCH_WRITE=0x001A5F0C:4 titles/outrun2/.../outrun2_recomp.exe
 *
 *     [WATCH] write to 0x001A5F0C from sub_001BB69D+0x2A
 *     [WATCH]   0x00000000 -> 0xF8604020
 *     [WATCH]   guest esp=0x00F7EF2C eax=0xF8604020 ecx=0x001A5F08
 *     [WATCH]   callers: 0x001BC6D2 <- 0x001BD546 <- 0x001BD668
 *
 * How it works: the watched page is made read-only, so a write to it raises
 * an access violation. The handler reports it, restores the page, and sets
 * the processor's trap flag so the very next instruction raises a
 * single-step exception -- at which point the write has happened, the new
 * value can be read, and the page is protected again.
 *
 * Two consequences worth knowing:
 *
 *   - Protection is per page and per process, so a watch on one word traps
 *     every write to the 4 KB around it, and during the one-instruction
 *     window when the page is restored another thread's write to it is
 *     missed. Guest code is cooperatively single-threaded here, so in
 *     practice that window is the vblank thread and the ISR.
 *
 *   - Mirror regions are separate mappings of the same memory, so a write
 *     through a different alias of the same physical page does not trap.
 *     Watch the address the writer actually uses.
 *
 * The companion is RECOMP_FIND_VALUE, which scans guest RAM for a value and
 * prints every address holding it. That is how you get an address to watch
 * when all you have is a bad pointer: find who is holding it, then watch that
 * field and re-run. The two together turn "it crashed on garbage" into a
 * named function in two runs, provided the failure is deterministic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "xbox_memory_layout.h"
#include "xbox_watchpoint.h"

#ifdef _WIN32

#include <dbghelp.h>

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;

extern uint32_t g_xbox_code_lo, g_xbox_code_hi;

#define MAX_WATCH 8
#define TRAP_FLAG 0x100u

typedef struct {
    uint32_t va;
    uint32_t len;
    uint32_t last;      /* value at the last report, for the -> arrow */
} Watch;

static Watch   g_watch[MAX_WATCH];
static int     g_n_watch;
static int     g_watch_reads;       /* trap reads as well as writes */
static long    g_watch_budget = 200;
static int     g_disarmed;
static char    g_arm_on[128];       /* RECOMP_WATCH_ARM_ON, until it fires */
static long    g_arm_on_nth = 1;    /* ...on the Nth matching open ("text#N") */

/* Single-step state is per thread: two threads can be mid-step at once, and
 * each has to put back its own page. */
static RECOMP_TLS void    *g_step_page;
static RECOMP_TLS uint32_t g_step_va;
static RECOMP_TLS uint32_t g_step_before;
static RECOMP_TLS int      g_step_pending;

static uint8_t *guest_base(void)
{
    return (uint8_t *)xbox_GetMemoryOffset();
}

/* The contiguous window (MmAllocateContiguousMemory, 0x80000000) is separate
 * storage from low RAM, mapped at the same guest-to-host offset, so the page
 * arithmetic here holds for it unchanged. It used to be refused as "outside
 * mapped guest RAM", which is where a title's pinned pools and a lot of its
 * streamed data live: TimeSplitters: Future Perfect's cutscene animation
 * records are at 0x813A6580. Kept in step with kernel.h's XBOX_CONTIG_*. */
#define WATCH_CONTIG_BASE 0x80000000u
#define WATCH_CONTIG_SIZE (64u * 1024u * 1024u)

static int guest_in_ram(uint32_t va, uint32_t len)
{
    if ((uint64_t)va + len <= (uint64_t)xbox_GetMappedSize())
        return 1;
    return va >= WATCH_CONTIG_BASE
        && (uint64_t)va + len <= (uint64_t)WATCH_CONTIG_BASE + WATCH_CONTIG_SIZE;
}

static uint32_t guest_read32(uint32_t va)
{
    if (!guest_in_ram(va, 4))
        return 0;
    return *(const volatile uint32_t *)(guest_base() + va);
}

/* The page a guest VA lives on, as a host pointer. */
static void *host_page_of(uint32_t va)
{
    uintptr_t p = (uintptr_t)guest_base() + va;
    return (void *)(p & ~(uintptr_t)0xFFF);
}

static DWORD arm_protection(void)
{
    /* Read-only still allows reads, so a write-only watch leaves the title
     * running at full speed apart from the writes themselves. Watching reads
     * needs the page taken away entirely, which is far more expensive. */
    return g_watch_reads ? PAGE_NOACCESS : PAGE_READONLY;
}

static void protect_one(uint32_t va, uint32_t len, DWORD prot)
{
    uintptr_t first = ((uintptr_t)guest_base() + va) & ~(uintptr_t)0xFFF;
    uintptr_t last  = ((uintptr_t)guest_base() + va + len - 1) & ~(uintptr_t)0xFFF;
    DWORD old = 0;
    VirtualProtect((LPVOID)first, (SIZE_T)(last - first) + 0x1000, prot, &old);
}

static void arm_all(void)
{
    int i;
    for (i = 0; i < g_n_watch; i++)
        protect_one(g_watch[i].va, g_watch[i].len, arm_protection());
}

static void disarm_all(const char *why)
{
    int i;
    for (i = 0; i < g_n_watch; i++)
        protect_one(g_watch[i].va, g_watch[i].len, PAGE_READWRITE);
    g_disarmed = 1;
    fprintf(stderr, "[WATCH] disarmed: %s\n", why);
    fflush(stderr);
}

/* Which watch, if any, covers this guest address. -1 for none. */
static int watch_covering(uint32_t va)
{
    int i;
    for (i = 0; i < g_n_watch; i++)
        if (va >= g_watch[i].va && va < g_watch[i].va + g_watch[i].len)
            return i;
    return -1;
}

/* Whether the faulting address is on a page we protected at all. A write to
 * the other 4088 bytes of a watched page faults too, and has to be let
 * through rather than reported -- but it still needs the same step-over
 * dance, or it would fault forever. */
static int on_watched_page(uint32_t va)
{
    int i;
    void *p = host_page_of(va);
    for (i = 0; i < g_n_watch; i++) {
        uintptr_t first = (uintptr_t)host_page_of(g_watch[i].va);
        uintptr_t last  = (uintptr_t)host_page_of(g_watch[i].va + g_watch[i].len - 1);
        if ((uintptr_t)p >= first && (uintptr_t)p <= last)
            return 1;
    }
    return 0;
}

static void print_symbol(uintptr_t rip)
{
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
    DWORD64 disp = 0;

    memset(buf, 0, sizeof(buf));
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)rip, &disp, sym))
        fprintf(stderr, "%s+0x%llX", sym->Name, (unsigned long long)disp);
    else
        fprintf(stderr, "RIP 0x%llX", (unsigned long long)rip);
}

/* The guest return-address chain, as the crash handler recovers it: lifted
 * calls push the guest return address, so scanning up from esp for values
 * inside the code sections names the callers. */
static void print_callers(void)
{
    const uint32_t *sp;
    int i, shown = 0;

    if (!g_esp || !guest_in_ram(g_esp, 256 * 4))
        return;
    sp = (const uint32_t *)(guest_base() + g_esp);
    fprintf(stderr, "[WATCH]   callers:");
    for (i = 0; i < 128 && shown < 5; i++) {
        uint32_t v = sp[i];
        if (v > g_xbox_code_lo && v < g_xbox_code_hi) {
            fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
            shown++;
        }
    }
    if (!shown)
        fprintf(stderr, " (none on the stack)");
    fprintf(stderr, "\n");
}

/* RECOMP_WATCH_STACK=<n>: the first n guest stack words at the write.
 *
 * The writer's registers are printed, but its caller's are not, and they
 * are often the question: Max Payne's index copy overran its buffer, and
 * the object that handed out that buffer was only in the copy's saved edi,
 * which a routine pushes on entry and nothing else records. */
static void print_stack(void)
{
    static int n = -1;
    const uint32_t *sp;
    int i;

    if (n < 0) {
        const char *v = getenv("RECOMP_WATCH_STACK");
        n = v ? atoi(v) : 0;
        if (n > 64)
            n = 64;
    }
    if (n <= 0 || !g_esp || !guest_in_ram(g_esp, (uint32_t)n * 4))
        return;
    sp = (const uint32_t *)(guest_base() + g_esp);
    for (i = 0; i < n; i++)
        fprintf(stderr, "%s%08X", (i % 8) ? " " : (i ? "\n[WATCH]   stack: "
                                                      : "[WATCH]   stack: "),
                sp[i]);
    fprintf(stderr, "\n");
}

void xbox_watch_init(void)
{
    const char *spec = getenv("RECOMP_WATCH_WRITE");
    const char *p;
    const char *budget = getenv("RECOMP_WATCH_BUDGET");

    if (!spec || !*spec)
        return;
    if (!xbox_GetMemoryOffset()) {
        fprintf(stderr, "[WATCH] guest memory is not mapped yet; not armed\n");
        return;
    }

    g_watch_reads = getenv("RECOMP_WATCH_READS") != NULL;
    if (budget && *budget)
        g_watch_budget = strtol(budget, NULL, 0);

    /* "0xADDR[:len][,0xADDR[:len]]" -- length defaults to a dword, because
     * the thing being watched is almost always a pointer or a counter. */
    for (p = spec; *p && g_n_watch < MAX_WATCH; ) {
        char *end = NULL;
        uint32_t va = (uint32_t)strtoul(p, &end, 0);
        uint32_t len = 4;
        if (end == p)
            break;
        p = end;
        if (*p == ':') {
            len = (uint32_t)strtoul(p + 1, &end, 0);
            p = end;
            if (len == 0) len = 4;
        }
        if (!guest_in_ram(va, len)) {
            fprintf(stderr, "[WATCH] 0x%08X is outside mapped guest RAM; skipped\n", va);
        } else {
            g_watch[g_n_watch].va = va;
            g_watch[g_n_watch].len = len;
            g_watch[g_n_watch].last = guest_read32(va);
            fprintf(stderr, "[WATCH] watching 0x%08X (%u bytes), currently 0x%08X\n",
                    va, len, g_watch[g_n_watch].last);
            g_n_watch++;
        }
        while (*p == ',' || *p == ' ')
            p++;
    }

    if (!g_n_watch)
        return;

    /* Armed late, on a file open, when the page is busy long before the
     * write that matters. Every write to a watched page traps, and on a hot
     * page that slows the title enough to change what it does: Future
     * Perfect's cutscene record at 0x813A6580 shares its page with start-up
     * work, and with the watch armed from boot the scripted presses landed on
     * different screens and the run never reached the cutscene at all. */
    {
        const char *on = getenv("RECOMP_WATCH_ARM_ON");
        if (on && *on) {
            /* "text#N": the Nth open of a matching file. A pack a title
             * opens at boot and again at the moment of interest -- Future
             * Perfect's skelts3.pak, once at start-up and once right after a
             * cutscene's data is read -- can only be named that way. */
            char *hash;
            snprintf(g_arm_on, sizeof g_arm_on, "%s", on);
            hash = strrchr(g_arm_on, '#');
            if (hash) {
                *hash = 0;
                g_arm_on_nth = strtol(hash + 1, NULL, 10);
                if (g_arm_on_nth < 1)
                    g_arm_on_nth = 1;
            }
            fprintf(stderr, "[WATCH] %d watchpoint(s) held until open #%ld "
                    "of a file matching \"%s\"\n", g_n_watch, g_arm_on_nth,
                    g_arm_on);
            fflush(stderr);
            return;
        }
    }

    arm_all();
    fprintf(stderr, "[WATCH] %d watchpoint(s) armed on %s, budget %ld reports\n",
            g_n_watch, g_watch_reads ? "reads and writes" : "writes",
            g_watch_budget);
    fflush(stderr);
}

void xbox_watch_note_path(const char *xbox_path)
{
    if (!g_arm_on[0] || !xbox_path || !strstr(xbox_path, g_arm_on))
        return;
    if (--g_arm_on_nth > 0)
        return;
    g_arm_on[0] = 0;                    /* once */
    arm_all();
    fprintf(stderr, "[WATCH] armed on opening %s: %d watchpoint(s) on %s, "
            "budget %ld reports\n", xbox_path, g_n_watch,
            g_watch_reads ? "reads and writes" : "writes", g_watch_budget);
    fflush(stderr);
}

int xbox_watch_handle_av(PEXCEPTION_POINTERS ep, uintptr_t fault_addr,
                         int is_write)
{
    uint32_t va;
    int idx;

    if (!g_n_watch || g_disarmed || !xbox_GetMemoryOffset())
        return 0;
    if (fault_addr < (uintptr_t)guest_base())
        return 0;
    va = (uint32_t)(fault_addr - (uintptr_t)guest_base());
    if (!on_watched_page(va))
        return 0;

    /* Already stepping on this thread means the step itself faulted on
     * something else. Do not recurse: put the page back and decline. */
    if (g_step_pending) {
        protect_one(g_step_va, 4, arm_protection());
        g_step_pending = 0;
        return 0;
    }

    idx = watch_covering(va);
    if (idx >= 0 && (is_write || g_watch_reads)) {
        if (g_watch_budget-- <= 0) {
            disarm_all("report budget exhausted (RECOMP_WATCH_BUDGET)");
            return 1;
        }
        /* The host thread too: two writers of one structure on different
         * threads is a race the uniprocessor console never had. */
        fprintf(stderr, "[WATCH] %s to 0x%08X (thread %lu) from ",
                is_write ? "write" : "read", va,
                (unsigned long)GetCurrentThreadId());
        print_symbol((uintptr_t)ep->ContextRecord->Rip);
        fprintf(stderr, "\n");
        {
            /* The host bytes of the storing instruction. Lifted code is
             * compiled C, so one guest store can become several host
             * instructions and the symbol offset alone does not say which
             * line did it; the encoding names the operand size and the
             * base register, which does. */
            const uint8_t *ip = (const uint8_t *)ep->ContextRecord->Rip;
            int b;
            fprintf(stderr, "[WATCH]   host insn:");
            for (b = 0; b < 12; b++)
                fprintf(stderr, " %02X", ip[b]);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "[WATCH]   guest esp=0x%08X eax=0x%08X ecx=0x%08X "
                        "edx=0x%08X esi=0x%08X edi=0x%08X\n",
                g_esp, g_eax, g_ecx, g_edx, g_esi, g_edi);
        print_callers();
        print_stack();
        g_step_before = guest_read32(g_watch[idx].va);
        g_step_va = g_watch[idx].va;
    } else {
        /* Collateral: some other address on the same page. Step over it
         * silently. */
        g_step_va = g_watch[0].va;
        g_step_before = 0;
        idx = -1;
    }

    /* Let the access happen, then trap immediately after it so the page can
     * be protected again -- and so the value it wrote can be read. */
    protect_one(g_step_va, 4, PAGE_READWRITE);
    ep->ContextRecord->EFlags |= TRAP_FLAG;
    g_step_pending = idx >= 0 ? 1 : 2;   /* 2 = silent */
    fflush(stderr);
    return 1;
}

int xbox_watch_handle_step(PEXCEPTION_POINTERS ep)
{
    int quiet;

    if (!g_step_pending)
        return 0;

    quiet = g_step_pending == 2;
    g_step_pending = 0;
    ep->ContextRecord->EFlags &= ~TRAP_FLAG;

    if (!quiet) {
        uint32_t now = guest_read32(g_step_va);
        fprintf(stderr, "[WATCH]   0x%08X -> 0x%08X%s\n",
                g_step_before, now,
                now == g_step_before ? "  (unchanged)" : "");
        fflush(stderr);
    }

    if (!g_disarmed)
        protect_one(g_step_va, 4, arm_protection());
    (void)g_step_page;
    return 1;
}

#endif /* _WIN32 */

void xbox_watch_scan_value(uint32_t value)
{
    const uint32_t *mem = (const uint32_t *)xbox_GetMemoryOffset();
    size_t words = xbox_GetMappedSize() / 4;
    size_t i;
    int hits = 0;

    if (!mem || !words)
        return;

    fprintf(stderr, "[FIND] scanning %llu MB of guest RAM for 0x%08X\n",
            (unsigned long long)(words * 4 / (1024 * 1024)), value);
    for (i = 0; i < words && hits < 64; i++) {
        if (mem[i] == value) {
            fprintf(stderr, "[FIND]   0x%08X holds it\n", (uint32_t)(i * 4));
            hits++;
        }
    }
    if (!hits)
        fprintf(stderr, "[FIND]   nowhere in RAM -- it was computed into a "
                        "register, not loaded from memory\n");
    else if (hits >= 64)
        fprintf(stderr, "[FIND]   ... stopped at 64\n");
    fprintf(stderr, "[FIND] watch one of these with "
                    "RECOMP_WATCH_WRITE=<addr> to see what writes it\n");
    fflush(stderr);
}

void xbox_watch_scan_on_crash(void)
{
    const char *v = getenv("RECOMP_FIND_VALUE");
    if (v && *v)
        xbox_watch_scan_value((uint32_t)strtoul(v, NULL, 0));
}
