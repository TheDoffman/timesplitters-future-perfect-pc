/* Isolated execution of the translated US pickup tick, with eligibility/grant
 * callbacks stubbed. No game process, files, settings, saves or renderer used. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
static int grants, checks;

void sub_000BDD50(void) { g_eax = 0; g_esp += 4; }
void sub_000BE180(void) { g_eax = 1; g_esp += 4; }
void sub_000BE990(void) {
    g_fp_top = (g_fp_top + 7) & 7;
    g_fp_stack[g_fp_top] = 0;
    g_esp += 4;
}
void sub_000C0540(void) { abort(); } /* Teleporters excluded from this fixture. */
void sub_000C1360(void) { ++grants; g_eax = 1; g_esp += 4; }

/* GENERATED_PICKUP_ROUTINE */

static void check(int cylinder, float x, float y, float z, int expected) {
    const uint32_t pickup = 0x1000, character_prop = 0x2000;
    const uint32_t data = 0x3000, character = 0x4000, shape = 0x5000, player = 0x6000;
    const uint32_t stack = 0x10000;
    MEM32(0x74C37C) = 1; MEM32(0x7A3580) = character;
    MEM32(character) = character_prop;
    MEM32(pickup + 0x28) = 3; MEM32(pickup + 0x7C) = data;
    MEM32(data + 0x10) = cylinder ? 8 : 0;
    MEM32(data + 0xC) = 1; MEM32(data + 0x18) = shape;
    MEMF(shape + 4) = 10000; /* Original radius squared, not a replacement range. */
    MEMF(shape + 8) = 90; MEMF(shape + 12) = 90;
    MEMF(pickup + 0x64) = x; MEMF(pickup + 0x68) = y; MEMF(pickup + 0x6C) = z;
    MEMF(pickup + 0x1C4) = 10;
    MEM32(character_prop + 0x150) = player;
    MEMF(player + 0x80) = 180; MEMF(0x475CD4) = .5f;
    g_esp = stack; MEM32(stack) = 0xDEADBEEF; MEM32(stack + 4) = pickup;
    g_ebx = 0xABC; g_esi = 0xDEF; g_edi = 0x123; g_ebp = 0x456;
    g_fp_top = 3; grants = 0;
    sub_000C15B0();
    ++checks;
    if (grants != expected || g_esp != stack + 4 || g_fp_top != 3 ||
        g_ebx != 0xABC || g_esi != 0xDEF || g_edi != 0x123) {
        fprintf(stderr, "FAIL: shape=%d pos=(%g,%g,%g) grant=%d expected=%d esp=%x fp=%d\n",
                cylinder, x, y, z, grants, expected, g_esp, g_fp_top);
        exit(1);
    }
}

int main(void) {
    unsigned char *memory = calloc(1, 0x800000);
    int shape, direction;
    if (!memory) return 2;
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)memory;
    for (shape = 0; shape < 2; ++shape) {
        for (direction = 0; direction < 32; ++direction) {
            double angle = direction * 6.283185307179586 / 32;
            check(shape, (float)(99*cos(angle)), 0, (float)(99*sin(angle)), 1);
            check(shape, (float)(101*cos(angle)), 0, (float)(101*sin(angle)), 0);
            check(shape, (float)(1000*cos(angle)), 0, (float)(1000*sin(angle)), 0);
        }
        check(shape, 100, 0, 0, 0); /* Exact radius uses the original strict test. */
        check(shape, 0, 0, 100, 0);
        check(shape, 0, 500, 0, 0);
        check(shape, NAN, 0, 0, 0);
        check(shape, 0, NAN, 0, 0);
        check(shape, 0, 0, NAN, 0);
        check(shape, INFINITY, 0, 0, 0);
        check(shape, -INFINITY, 0, 0, 0);
    }
    printf("PASS: %d original pickup range/height and guest ABI checks\n", checks);
    free(memory);
    return 0;
}
