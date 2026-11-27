/* Execute the production decoder, Linux register adapter and device policy.
 * Only the host context layout and writable-alias provider are fixtures. */
#ifdef _MSC_VER
#include <intrin.h>
#define __ATOMIC_SEQ_CST 0
#define __atomic_fetch_and(p,v,order) _InterlockedAnd((volatile long *)(p),(long)(v))
#define __atomic_fetch_or(p,v,order) _InterlockedOr((volatile long *)(p),(long)(v))
#endif
#include "mmio_linux_arm64.c"
#include <stdio.h>
#include <stdlib.h>

ptrdiff_t g_xbox_mem_offset = 0x100000000ull;
MCPXAPUState *g_apu_state;
static unsigned char device[0x4000];
static uint32_t interrupt;
void *tsfp_guest_mmio_alias(void *host, size_t bytes)
{
    uintptr_t va = (uintptr_t)host - (uintptr_t)g_xbox_mem_offset;
    if (va == 0xfd000100u && bytes == 4) return &interrupt;
    if (va >= 0xfd600000u && va + bytes <= 0xfd604000u) return device + (va - 0xfd600000u);
    return NULL;
}
uint64_t mcpx_apu_mmio_read(MCPXAPUState *d, uint64_t a, unsigned s)
{ (void)d; (void)a; (void)s; abort(); }
void mcpx_apu_mmio_write(MCPXAPUState *d, uint64_t a, uint64_t v, unsigned s)
{ (void)d; (void)a; (void)v; (void)s; abort(); }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int access_instruction(uint32_t *ins, ucontext_t *uc, uintptr_t address)
{
    siginfo_t info = {0}; info.si_addr = (void *)address;
    uc->uc_mcontext.pc = (uintptr_t)ins;
    int result = tsfp_native_mmio(&info, uc);
    if (result && uc->uc_mcontext.pc != (uintptr_t)ins + 4) abort();
    if (!result && uc->uc_mcontext.pc != (uintptr_t)ins) abort();
    return result;
}
int main(void)
{
    ucontext_t uc = {0};
    uintptr_t base = (uintptr_t)g_xbox_mem_offset + 0xfd600000u;
    uc.uc_mcontext.regs[1] = base;
    uc.uc_mcontext.regs[0] = UINT64_C(0xffffffff12345678);
    uint32_t ins = 0xb9000c20u; /* str w0,[x1,#12] */
    CHECK(access_instruction(&ins, &uc, base + 12));
    uint32_t value; memcpy(&value, device + 12, 4); CHECK(value == 0x12345678);
    ins = 0xb9400c20u; /* ldr w0,[x1,#12], clears upper register bits */
    CHECK(access_instruction(&ins, &uc, base + 12)); CHECK(uc.uc_mcontext.regs[0] == value);
    device[0] = 0x80;
    ins = 0x39800020u; /* ldrsb x0,[x1] */
    CHECK(access_instruction(&ins, &uc, base)); CHECK(uc.uc_mcontext.regs[0] == UINT64_C(0xffffffffffffff80));
    ins = 0x39c00020u; /* ldrsb w0,[x1] */
    CHECK(access_instruction(&ins, &uc, base)); CHECK(uc.uc_mcontext.regs[0] == 0xffffff80u);
    uc.uc_mcontext.sp = base;
    ins = 0xb80047ffu; /* str wzr,[sp],#4 */
    CHECK(access_instruction(&ins, &uc, base)); CHECK(uc.uc_mcontext.sp == base + 4);
    memcpy(&value, device, 4); CHECK(value == 0);
    uc.uc_mcontext.regs[31] = 0x11223344;
    ins = 0xb940003fu; /* ldr wzr,[x1] discards result */
    CHECK(access_instruction(&ins, &uc, base)); CHECK(uc.uc_mcontext.regs[31] == 0x11223344);
    ins = 0xb81fcc20u; /* str w0,[x1,#-4]! */
    uc.uc_mcontext.regs[1] = base + 16;
    CHECK(access_instruction(&ins, &uc, base + 12)); CHECK(uc.uc_mcontext.regs[1] == base + 12);
    ins = 0xd503201fu; /* nop must fail without advancing PC */
    CHECK(!access_instruction(&ins, &uc, base));
    ins = 0xb9400c20u;
    CHECK(!access_instruction(&ins, &uc, base + 13)); /* address mismatch */
    uc.uc_mcontext.regs[1] = base;
    ins = 0xb9010020u; /* str w0,[x1,#0x100]: acknowledge CRTC */
    value = 3; memcpy(device + 0x100, &value, 4); interrupt = 0x01000000u;
    uc.uc_mcontext.regs[0] = 1;
    CHECK(access_instruction(&ins, &uc, base + 0x100));
    memcpy(&value, device + 0x100, 4); CHECK(value == 2 && interrupt == 0x01000000u);
    uc.uc_mcontext.regs[0] = 2;
    CHECK(access_instruction(&ins, &uc, base + 0x100));
    memcpy(&value, device + 0x100, 4); CHECK(value == 0 && interrupt == 0);
    /* Outside device windows must reject before dereferencing a wild PC. */
    CHECK(!access_instruction(NULL, &uc, 0x1234));
    puts("PASS: Linux ARM64 context, widths, sign extension, SP/writeback, rejection and CRTC");
    return 0;
}
