/* Execute the original US controller-sensitivity menu callback in disposable
 * guest RAM. UI and sound callbacks are stubs; no settings or saves are used. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1;

void sub_002E9BC0(void) { g_esp += 4; }
void sub_00074910(void) { g_esp += 4; }
void sub_00074950(void) { abort(); } /* Non-slider UI events excluded. */
void sub_0003D7B0(void) { abort(); }
void sub_00045390(void) { abort(); } /* Sound disabled in isolated guest RAM. */
void sub_00045820(void) { abort(); }

/* GENERATED_SENSITIVITY_ROUTINE */

static int checks;
static float adjust(unsigned player, float value, unsigned buttons, float expected)
{
    const uint32_t menu = 0x1000, stack = 0x10000;
    const uint32_t field = 0x7BA3DC + player * 0x1E0;
    float actual;
    MEM32(0x7BA9E0) = player;
    MEMF(field) = value;
    MEM32(menu + 0x18C) = 9; /* Original sensitivity slider event. */
    MEM32(menu + 0xC4) = buttons;
    g_esp = stack; MEM32(stack) = 0xDEADBEEF; MEM32(stack + 4) = menu;
    g_esi = 0xABC; g_ebx = 0xDEF; g_edi = 0x123;
    sub_002EAB30();
    actual = MEMF(field);
    ++checks;
    if (fabsf(actual - expected) > .00001f || g_esp != stack + 4 ||
        g_esi != 0xABC || g_ebx != 0xDEF || g_edi != 0x123) {
        fprintf(stderr, "FAIL: sensitivity player %u value %.6f buttons %x: %.6f expected %.6f; esp %x\n",
                player, value, buttons, actual, expected, g_esp);
        exit(1);
    }
    return actual;
}

int main(void)
{
    unsigned player, step;
    unsigned char *memory = calloc(1, 0x800000);
    if (!memory) return 2;
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_SLIDER_CONSTANTS */
    for (player = 0; player < 4; ++player) {
        float value;
        adjust(player, .5f, 0x80000, .55f);
        adjust(player, .5f, 0x40000, .45f);
        value = 0;
        for (step = 1; step <= 20; ++step)
            value = adjust(player, value, 0x80000, step * .05f);
        adjust(player, value, 0x80000, 1);
        value = 1;
        for (step = 1; step <= 20; ++step)
            value = adjust(player, value, 0x40000, (20 - step) * .05f);
        adjust(player, value, 0x40000, 0);
        adjust(player, .5f, 0, .5f);
        adjust(player, .02f, 0x40000, 0);
        adjust(player, .98f, 0x80000, 1);
    }
    printf("PASS: %d original controller sensitivity steps, bounds and guest ABI checks\n", checks);
    free(memory);
    return 0;
}
