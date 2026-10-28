/* Project native continuation registry for translated guest CRT calls.
 * setjmp executes in the generated caller, never in this service. Slots and
 * guest register cells belong to the host thread. Stack tags are integers for
 * membership checks; they are never dereferenced after their frame returns.
 */
#if defined(_WIN32)
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

enum { TSFP_CONTINUATIONS = 32 };
static RECOMP_TLS jmp_buf continuations[TSFP_CONTINUATIONS];
static RECOMP_TLS uint32_t guest_keys[TSFP_CONTINUATIONS];
static RECOMP_TLS uintptr_t stack_tags[TSFP_CONTINUATIONS];
static RECOMP_TLS unsigned active;
/* Configure at startup, or while guest threads are quiescent. */
static recomp_foreign_longjmp_fn foreign_handler;

static unsigned continuation_for(uint32_t key)
{
    unsigned cursor=active;
    while (cursor) {
        --cursor;
        if (guest_keys[cursor]==key) return cursor;
    }
    return TSFP_CONTINUATIONS;
}

jmp_buf *recomp_setjmp_slot(uint32_t guest_buffer)
{
    volatile unsigned char stack_anchor=0;
    unsigned index=continuation_for(guest_buffer);
    if (index==TSFP_CONTINUATIONS) {
        /* Keep older live slots stable; overflow replaces only the newest. */
        index=active<TSFP_CONTINUATIONS ? active++ : TSFP_CONTINUATIONS-1u;
        guest_keys[index]=guest_buffer;
    }
    stack_tags[index]=(uintptr_t)&stack_anchor;
    return &continuations[index];
}

void recomp_set_foreign_longjmp(recomp_foreign_longjmp_fn handler)
{
    foreign_handler=handler;
}

int recomp_guest_longjmp(uint32_t guest_buffer, uint32_t result)
{
    const unsigned index=continuation_for(guest_buffer);
    if (index==TSFP_CONTINUATIONS) return 0; /* Let the guest implementation run. */

    /* The guest CRT stores these five little-endian architectural cells. The
     * project accessors retain unaligned access and guest-width arithmetic. */
    g_ebp=MEM32(guest_buffer);
    g_seh_ebp=g_ebp;
    g_ebx=MEM32(guest_buffer+4u);
    g_edi=MEM32(guest_buffer+8u);
    g_esi=MEM32(guest_buffer+12u);
    g_esp=MEM32(guest_buffer+16u)+4u;
    active=index+1u;

    /* Preserve all guest result bits; the supported hosts use 32-bit int. */
    const uint32_t bits=result ? result : 1u;
    int native_result;
    _Static_assert(sizeof(native_result)==sizeof(bits), "32-bit native jump result required");
    memcpy(&native_result,&bits,sizeof(native_result));
#if defined(_WIN32)
    ULONG_PTR bottom,top;
    GetCurrentThreadStackLimits(&bottom,&top);
    if (stack_tags[index]<bottom || stack_tags[index]>=top) {
        if (foreign_handler)
            foreign_handler(&continuations[index],native_result,stack_tags[index]);
        /* An absent or returning adapter cannot make an unwind safe. */
        fprintf(stderr,"[tsfp-jump] foreign stack for guest buffer %08X; adapter did not transfer control\n",
                (unsigned)guest_buffer);
        fflush(stderr);
        abort();
    }
#endif
    longjmp(continuations[index],native_result);
}
