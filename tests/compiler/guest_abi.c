/* Original guest-cell contracts. The oracle is byte storage and explicit
 * architectural state transitions, independent of the implementation helpers. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "under_test.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_esp;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp, g_fs_base;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_control_word, g_fp_cc;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3;
RECOMP_TLS RecompMmx g_mm4, g_mm5, g_mm6, g_mm7;

#define CHECK(x) do { if (!(x)) { printf("line %d\n", __LINE__); return 1; } } while (0)
static unsigned char ram[4096];
static unsigned address_calls, cell_calls, value_calls;
static uint32_t cell;
static uint32_t address(void) { ++address_calls; return 33; }
static uint32_t *destination(void) { ++cell_calls; return &cell; }
static uint32_t incoming(void) { ++value_calls; return 0x12345678u; }
extern void cross_unit_callback(void);

static int primitives(void)
{
    unsigned offset, seed, lane;
    uint32_t stack, result;
    CHECK(sizeof(RecompXmm) == 16 && sizeof(RecompMmx) == 8);
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)ram;
    CHECK(XBOX_PTR(UINT64_C(0x300000011)) == (uintptr_t)(ram + 17));
    g_xbox_mem_offset = -16;
    CHECK(XBOX_PTR(UINT64_C(0x200000020)) == 16);
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)ram;
    for (offset = 16; offset < 32; ++offset) {
        for (seed = 0; seed < 256; ++seed) {
            uint64_t value = UINT64_C(0x8090a0b0c0d0e000) | seed;
            memset(ram, 0x5a, sizeof(ram));
            MEM64(offset) = value;
            CHECK(MEM64(offset) == value);
            CHECK(ram[offset-1] == 0x5a && ram[offset+8] == 0x5a);
            for (lane = 0; lane < 8; ++lane)
                CHECK(ram[offset+lane] == (unsigned char)(value >> (8*lane)));
            CHECK((uint64_t)SMEM64(offset) == value);
            MEM32(offset) = (uint32_t)value;
            CHECK(MEM32(offset) == (uint32_t)value);
            CHECK((uint32_t)SMEM32(offset) == (uint32_t)value);
            MEM16(offset) = (uint16_t)value;
            CHECK(MEM16(offset) == (uint16_t)value);
            CHECK((uint16_t)SMEM16(offset) == (uint16_t)value);
            MEM8(offset) = (uint8_t)value;
            CHECK(MEM8(offset) == (uint8_t)value);
            CHECK((uint8_t)SMEM8(offset) == (uint8_t)value);
            MEMF(offset) = -0.0f;
            CHECK(MEM32(offset) == 0x80000000u);
            MEMD(offset) = -0.0;
            CHECK(MEM64(offset) == UINT64_C(0x8000000000000000));
            MEM32(offset) = 0x3f800000u;
            CHECK(MEMF(offset) == 1.0f);
            MEM64(offset) = UINT64_C(0x3ff0000000000000);
            CHECK(MEMD(offset) == 1.0);
        }
    }
    MEM32(address()) = incoming();
    CHECK(address_calls == 1 && value_calls == 1 && MEM32(33) == 0x12345678u);
    CHECK(MEM32(address()) == 0x12345678u && address_calls == 2);
    CHECK((uintptr_t)&MEM32(address()) == (uintptr_t)(ram+33) && address_calls == 3);
    for (seed = 0; seed < 65536; ++seed) {
        cell = 0x89abcdefu;
        result = SET_LO16(cell, seed);
        CHECK(cell == 0x89ab0000u + seed && result == cell);
        CHECK(LO16(cell) == seed && LO8(cell) == (seed % 256));
        CHECK(HI8(cell) == seed / 256);
        cell = 0x89abcdefu; SET_LO8(cell, seed);
        CHECK(cell == 0x89abcd00u + seed % 256);
        cell = 0x89abcdefu; SET_HI8(cell, seed);
        CHECK(cell == 0x89ab00efu + (seed % 256) * 256);
    }
    cell = 0xabcdef01u; value_calls = cell_calls = 0;
    SET_HI8(*destination(), incoming());
    CHECK(cell_calls == 1 && value_calls == 1 && cell == 0xabcd7801u);
    stack = 512; MEM32(516) = 0x87654321u;
    PUSH32(stack, MEM32(stack+4));
    CHECK(stack == 508 && MEM32(508) == 0x87654321u);
    PUSH32(stack, stack);
    CHECK(stack == 504 && MEM32(504) == 508);
    POP32(stack, result);
    CHECK(result == 508 && stack == 508);
    POP32(stack, MEM32(stack));
    CHECK(stack == 512 && MEM32(512) == 0x87654321u);
    MEM32(stack) = 900;
    POP32(stack, stack);
    CHECK(stack == 900);
    cell = 600; cell_calls = value_calls = 0;
    PUSH32(*destination(), incoming());
    CHECK(cell_calls == 1 && value_calls == 1 && cell == 596);
    POP32(*destination(), result);
    CHECK(cell_calls == 2 && cell == 600 && result == 0x12345678u);
    return 0;
}

static int thread_body(void)
{
    CHECK(g_eax == 0 && g_esp == 0 && g_fs_base == 0 && g_fp_top == 0);
    CHECK(g_xmm0.q[0] == 0 && g_mm7.q == 0 && g_fp_stack[0] == 0);
    g_eax = 12; g_esp = 34; g_fs_base = 56; g_fp_top = 7;
    g_xmm0.q[0] = 78; g_mm7.q = 90; g_fp_stack[0] = 123;
    cross_unit_callback();
    CHECK(g_eax == 13 && g_xmm0.u[0] == 79 && g_mm7.q == 91);
    return 0;
}
#if defined(_WIN32)
static DWORD WINAPI worker(void *unused) { (void)unused; return (DWORD)thread_body(); }
#else
static void *worker(void *unused) { (void)unused; return (void *)(uintptr_t)thread_body(); }
#endif

int main(void)
{
    CHECK(primitives() == 0);
    g_eax = 111; g_esp = 222; g_fs_base = 333; g_fp_top = 4;
    g_xmm0.q[0] = 444; g_mm7.q = 555; g_fp_stack[0] = 666;
#if defined(_WIN32)
    {
        DWORD result = 1;
        HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        CHECK(thread != NULL);
        CHECK(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0);
        CHECK(GetExitCodeThread(thread, &result) && result == 0);
        CHECK(CloseHandle(thread));
    }
#else
    {
        pthread_t thread; void *result;
        CHECK(pthread_create(&thread, NULL, worker, NULL) == 0);
        CHECK(pthread_join(thread, &result) == 0 && result == NULL);
    }
#endif
    CHECK(g_eax == 111 && g_esp == 222 && g_fs_base == 333 && g_fp_top == 4);
    CHECK(g_xmm0.q[0] == 444 && g_mm7.q == 555 && g_fp_stack[0] == 666);
    cross_unit_callback();
    CHECK(g_eax == 112 && g_xmm0.u[0] == 445 && g_mm7.q == 556);
    puts("guest cells, memory, stack and threads passed");
    return 0;
}
