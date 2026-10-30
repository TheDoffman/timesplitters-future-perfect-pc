/*
 * Temporary bring-up probe for Mortal Kombat: Deadly Alliance.
 *
 * The title's main thread never leaves sub_00023420, which spins until the
 * object it was handed reaches state 4:
 *
 *     while (*(int *)obj != 4) { spin(); Sleep(); pump(); }
 *
 * Measured: the object is NULL, so the state read comes back 0 forever. The
 * request queue at 0x002C5228 is a healthy empty ring (element size 0x28,
 * capacity 4) that nothing ever enqueues to. So the failure is upstream of
 * the wait, and what this prints is the guest call chain that reached it.
 * Delete once the wait is understood.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "recomp/gen/recomp_types.h"

#define MK_QUEUE_VA 0x002C5228u

/* The XBE's executable span, from its section table: .text through XPP. */
#define MK_CODE_LO  0x00011000u
#define MK_CODE_HI  0x00230F9Cu

/* Lifted calls push the guest return address, so code-range values on the
 * stack above esp name the callers -- the same scan the watchpoint uses. */
static void mk_print_callers(const char *label)
{
    int i, shown;

    fprintf(stderr, "%s", label);
    for (i = 0, shown = 0; i < 160 && shown < 8; i++) {
        uint32_t v = MEM32(g_esp + (uint32_t)i * 4);
        if (v > MK_CODE_LO && v < MK_CODE_HI) {
            fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
            shown++;
        }
    }
    fprintf(stderr, "\n");
}

/* The title's own logger, sub_00157650, is vsprintf into this buffer followed
 * by an emit that goes nowhere here. Reading the buffer after the format turns
 * every diagnostic the title writes about itself into a line on stderr, which
 * is how "Out of RAM" was found at all. */
#define MK_MSG_VA 0x002F4890u

void recomp_mk_msg(void)
{
    char buf[512];
    unsigned i;

    for (i = 0; i + 1 < sizeof buf; i++) {
        unsigned char c = MEM8(MK_MSG_VA + i);
        if (!c)
            break;
        buf[i] = (char)c;
    }
    buf[i] = '\0';
    while (i && (buf[i - 1] == '\n' || buf[i - 1] == '\r'))
        buf[--i] = '\0';
    fprintf(stderr, "[MK-MSG] %s", buf);
    mk_print_callers("  from:");
    fflush(stderr);
}

void recomp_mk_probe(uint32_t obj)
{
    static unsigned long n;
    unsigned long m;
    int i, shown;

    m = ++n;
    if (m != 1 && m != 64 && m != 4096 && m != 262144)
        return;

    fprintf(stderr, "[MK] wait #%lu obj=0x%08X state=%u pending=%u queue:",
            m, obj, obj ? MEM32(obj) : 0, obj ? MEM32(obj + 0x10) : 0);
    for (i = 0; i < 8; i++)
        fprintf(stderr, " %08X", MEM32(MK_QUEUE_VA + (uint32_t)i * 4));
    fprintf(stderr, "\n");

    /* Lifted calls push the guest return address, so code-range values on the
     * stack above esp name the callers -- the same scan the watchpoint uses. */
    fprintf(stderr, "[MK]   callers:");
    for (i = 0, shown = 0; i < 160 && shown < 10; i++) {
        uint32_t v = MEM32(g_esp + (uint32_t)i * 4);
        if (v > MK_CODE_LO && v < MK_CODE_HI) {
            fprintf(stderr, "%s 0x%08X", shown ? " <-" : "", v);
            shown++;
        }
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

/* sub_00154E00 asks "is this heap on the registered list", and every failing
 * allocation in the title answers no. Print both sides of that question: the
 * heap the caller handed in, and the list head at 0x002F4874. */
void recomp_mk_heapchk(uint32_t heap, uint32_t head)
{
    static unsigned long n;
    static uint32_t last_heap, last_head;

    if (++n > 12 && heap == last_heap && head == last_head)
        return;
    if (n > 24)
        return;
    last_heap = heap;
    last_head = head;
    fprintf(stderr, "[MK-HEAP] #%lu asked for heap=0x%08X, list head=0x%08X\n",
            n, heap, head);
    fflush(stderr);
}

/* The SystemHeap is obtained by a plain guest malloc. Its "can I afford this"
 * probe (malloc+free) succeeds and the real allocation that follows returns
 * NULL, which is what leaves every heap unregistered. Print both. */
void recomp_mk_alloc(const char *what, uint32_t size, uint32_t result)
{
    if (size < 0x100000u)
        return;
    fprintf(stderr, "[MK-ALLOC] %s %u bytes (%.2f MB) -> 0x%08X%s\n",
            what, size, (double)size / (1024.0 * 1024.0), result,
            result ? "" : "   <-- FAILED");
    fflush(stderr);
}

/*
 * Does the guest heap give a freed block back?
 *
 * The title's own sequence is malloc(N) -> free -> malloc(N-16), and the
 * second one returns NULL. That is consistent with two very different
 * causes: a free that does not return the block, or a heap that only ever
 * had one block that size. Calling the guest's own malloc and free directly
 * separates them -- if the second round already fails, free is the problem.
 */
void recomp_mk_heaptest(uint32_t size)
{
    void sub_001C6F58(void);   /* malloc */
    void sub_001C6EAD(void);   /* free   */
    static int done;
    uint32_t p[3];
    int i;

    if (done)
        return;
    done = 1;

    for (i = 0; i < 3; i++) {
        PUSH32(g_esp, size);
        PUSH32(g_esp, 0);
        sub_001C6F58();
        g_esp += 4;
        p[i] = g_eax;

        if (p[i]) {
            PUSH32(g_esp, p[i]);
            PUSH32(g_esp, 0);
            sub_001C6EAD();
            g_esp += 4;
        }
    }

    fprintf(stderr, "[MK-HEAPTEST] malloc(%u)/free x3 -> 0x%08X 0x%08X 0x%08X%s\n",
            size, p[0], p[1], p[2],
            (p[0] && !p[1]) ? "   <-- free does not return the block" : "");
    fflush(stderr);
}

/* RtlFreeHeap, reached through the XAPI wrapper sub_0015B586. It answers a
 * boolean, and a free that answers FALSE is a block the heap never took back. */
static uint32_t g_free_ptr;

void recomp_mk_freein(uint32_t heap, uint32_t flags, uint32_t ptr)
{
    g_free_ptr = ptr;
    (void)heap; (void)flags;
}

void recomp_mk_freeout(uint32_t ok)
{
    static unsigned long n, failures;

    n++;
    if (!(ok & 0xFF))
        failures++;
    if (n <= 6 || (!(ok & 0xFF) && failures <= 6))
        fprintf(stderr, "[MK-FREE] #%lu free(0x%08X) -> %s\n",
                n, g_free_ptr, (ok & 0xFF) ? "ok" : "FAILED");
    if (n == 2000)
        fprintf(stderr, "[MK-FREE] %lu frees so far, %lu of them failed\n",
                n, failures);
    fflush(stderr);
}

/* The loading-screen object whose pump method the wait loop calls. Slot +0x18
 * resolves to 0x000E0535, which is a loop label inside a function, not an
 * entry -- so either the vtable pointer is wrong or something overwrote the
 * slot. Printing the whole vtable separates those: a table of plausible entry
 * points with one bad slot is a write, a table of noise is a bad pointer. */
void recomp_mk_vtable(uint32_t obj, uint32_t vt)
{
    static uint32_t seen_vt[8], seen_slot[8];
    static int count;
    uint32_t slot = MEM32(vt + 0x18);
    int i;

    for (i = 0; i < count; i++)
        if (seen_vt[i] == vt && seen_slot[i] == slot)
            return;
    if (count == 8)
        return;
    seen_vt[count] = vt; seen_slot[count] = slot; count++;
    fprintf(stderr, "[MK-VT] obj=0x%08X vtable=0x%08X slots:", obj, vt);
    for (i = 0; i < 10; i++)
        fprintf(stderr, " +%02X=%08X", i * 4, MEM32(vt + (uint32_t)i * 4));
    fprintf(stderr, "\n");
    fflush(stderr);
}

/*
 * RECOMP_MK_PLAIN_WAIT=1 -- take the loader's non-coroutine wait path.
 *
 * sub_001205B0 waits for a load two ways. With the loading-screen object
 * present it calls that object's pump, which is a hand-rolled coroutine:
 * it saves ebp/ebx/esi/edi and esp into the object, reloads esp, ss and ebp
 * from globals and jumps to a saved continuation address. That is a context
 * switch, and this runtime cannot follow it -- lifted functions are host C
 * functions, and the continuation is a label in the middle of one, which has
 * no dispatch entry. The jump is refused, the pump returns without doing
 * anything, and the wait spins forever.
 *
 * Without that object the title uses an ordinary spin/Sleep/pump loop that
 * needs no stack switching. This forces that path, to find out whether the
 * coroutine is the only thing in the way.
 */
int recomp_mk_plain_wait(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_MK_PLAIN_WAIT");
        on = v && *v && *v != '0';
        if (on) {
            fprintf(stderr, "[MK] RECOMP_MK_PLAIN_WAIT: using the loader's "
                            "non-coroutine wait path\n");
            fflush(stderr);
        }
    }
    return on;
}
