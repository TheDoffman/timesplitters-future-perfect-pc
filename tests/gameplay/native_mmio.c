/* Real SIGBUS recovery through protected guest pages, plus decoder boundaries. */
#include "arm64_access.h"
#include "platform/win32_compat.h"
#include "apu/apu.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

ptrdiff_t g_xbox_mem_offset;
MCPXAPUState *g_apu_state;
uint64_t mcpx_apu_mmio_read(MCPXAPUState *d, uint64_t a, unsigned s)
{ (void)d; (void)a; (void)s; abort(); }
void mcpx_apu_mmio_write(MCPXAPUState *d, uint64_t a, uint64_t v, unsigned s)
{ (void)d; (void)a; (void)v; (void)s; abort(); }
extern int tsfp_native_mmio(siginfo_t *, void *);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
static volatile sig_atomic_t faults;
static void handle(int sig, siginfo_t *info, void *ctx)
{
    if (!tsfp_native_mmio(info, ctx)) _Exit(128 + sig);
    faults++;
}
int main(void)
{
    NativeAccess a;
    uint64_t r[32] = {0}; r[10] = 0x1000; r[11] = 0x140;
    CHECK(native_decode_access(0xb82b6948u, r, &a)); /* str w8,[x10,x11] */
    CHECK(a.address == 0x1140 && a.bytes == 4 && !a.load && a.rt == 8);
    r[1] = 0x2000;
    CHECK(native_decode_access(0xb9400c20u, r, &a)); /* ldr w0,[x1,#12] */
    CHECK(a.address == 0x200c && a.bytes == 4 && a.load && !a.result64);
    CHECK(native_decode_access(0xb81fcc20u, r, &a)); /* str w0,[x1,#-4]! */
    CHECK(a.address == 0x1ffc && a.writeback == 0x1ffc && a.update_base);
    CHECK(!native_decode_access(0xd503201fu, r, &a)); /* nop */
    CHECK(!native_decode_access(0x3d800020u, r, &a)); /* str q0,[x1]: SIMD unsupported */

    void *base = w32_guest_arena_reserve(0x100000000ull);
    CHECK(base != NULL);
    g_xbox_mem_offset = (ptrdiff_t)base;
    uint8_t *nv = w32_guest_mmio_map((uint8_t *)base + 0xfd000000u, 0x1000000);
    uint8_t *mc = w32_guest_mmio_map((uint8_t *)base + 0xfe800000u, 0x800000);
    CHECK(nv && mc);
    uint32_t *status = w32_guest_mmio_alias(nv + 0x600100, 4);
    CHECK(status != NULL);
    *status = 1;
    *(uint32_t *)(nv + 0x100) = 0x01000000u;
    DWORD old;
    CHECK(VirtualProtect(nv + 0x600000, 0x4000, PAGE_READONLY, &old));
    CHECK(VirtualProtect(mc + 0x400000, 0x4000, PAGE_READONLY, &old));
    struct sigaction sa = {0}; sa.sa_flags = SA_SIGINFO; sa.sa_sigaction = handle;
    sigemptyset(&sa.sa_mask); CHECK(sigaction(SIGBUS, &sa, NULL) == 0); CHECK(sigaction(SIGSEGV, &sa, NULL) == 0);
    *(volatile uint32_t *)(nv + 0x600140) = 0x12345678u;
    CHECK(*(volatile uint32_t *)(nv + 0x600140) == 0x12345678u);
    *(volatile uint32_t *)(nv + 0x600100) = 1;
    CHECK(*status == 0 && *(uint32_t *)(nv + 0x100) == 0);
    *(volatile uint8_t *)(mc + 0x40011b) = 3;
    CHECK(*(volatile uint8_t *)(mc + 0x40011b) == 1); /* reset self-clears */
    CHECK(faults == 3);
    CHECK(w32_guest_arena_release());
    base = w32_guest_arena_reserve(0x100000000ull);
    CHECK(base != NULL);
    CHECK(w32_guest_mmio_map((uint8_t *)base + 0xfd000000u, 0x1000000) != NULL);
    CHECK(w32_guest_arena_release());
    puts("PASS: ARM64 decoder, protected register writes, NV2A interrupt acknowledgement and AC97 reset");
    return 0;
}
