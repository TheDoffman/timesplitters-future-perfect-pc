/* ARM64 counterpart of the runtime's x86 VEH register-access handlers.
 * Guest MMIO stays protected. Shared writable aliases avoid changing page
 * protection while another guest thread is running. */
#include "arm64_access.h"
#include "platform/win32_compat.h"
#include "apu/apu.h"
#include <sys/ucontext.h>
#include <signal.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;

int tsfp_native_mmio(siginfo_t *info, void *context)
{
    uintptr_t host = (uintptr_t)info->si_addr;
    if (host < (uintptr_t)g_xbox_mem_offset || host - (uintptr_t)g_xbox_mem_offset > UINT32_MAX) return 0;
    uint32_t va = (uint32_t)(host - (uintptr_t)g_xbox_mem_offset);
    int apu = va >= 0xfe800000u && va < 0xfe830000u;
    int crtc = va >= 0xfd600000u && va < 0xfd604000u;
    int ac97 = va >= 0xfec00000u && va < 0xfec04000u;
    if (!apu && !crtc && !ac97) return 0;
    ucontext_t *uc = context;
    struct __darwin_arm_thread_state64 *ss = &uc->uc_mcontext->__ss;
    uint64_t regs[32];
    for (unsigned i = 0; i < 29; i++) regs[i] = ss->__x[i];
    regs[29] = ss->__fp; regs[30] = ss->__lr; regs[31] = ss->__sp;
    NativeAccess access;
    uint32_t ins;
    memcpy(&ins, (void *)(uintptr_t)ss->__pc, sizeof ins);
    if (!native_decode_access(ins, regs, &access) || access.address != host) return 0;
    uint64_t value = access.rt == 31 ? 0 : regs[access.rt];
    if (access.bytes < 8) value &= (1ull << (access.bytes * 8)) - 1;
    void *alias = w32_guest_mmio_alias((void *)host, access.bytes);
    if (!alias) return 0;
    if (apu) {
        if (!g_apu_state || access.bytes > 4 || va + access.bytes > 0xfe830000u) return 0;
        if (access.load) value = mcpx_apu_mmio_read(g_apu_state, va - 0xfe800000u, access.bytes);
        else mcpx_apu_mmio_write(g_apu_state, va - 0xfe800000u, value, access.bytes);
    } else if (access.load) {
        value = 0; memcpy(&value, alias, access.bytes);
    } else {
        if (crtc && va == 0xfd600100u) {
            if (access.bytes != 4) return 0;
            uint32_t remaining = __atomic_fetch_and((uint32_t *)alias, ~(uint32_t)value, __ATOMIC_SEQ_CST) & ~(uint32_t)value;
            uint32_t *pmc = w32_guest_mmio_alias((void *)((uintptr_t)g_xbox_mem_offset + 0xfd000100u), 4);
            if (!pmc) return 0;
            if (remaining) __atomic_fetch_or(pmc, 0x01000000u, __ATOMIC_SEQ_CST);
            else __atomic_fetch_and(pmc, ~0x01000000u, __ATOMIC_SEQ_CST);
        } else {
            if (ac97 && (va == 0xfec0011bu || va == 0xfec0017bu)) value &= ~2ull;
            memcpy(alias, &value, access.bytes);
        }
    }
    if (access.load && access.rt != 31) {
        if (access.sign_extend) {
            unsigned shift = 64 - access.bytes * 8;
            value = (uint64_t)((int64_t)(value << shift) >> shift);
        }
        if (!access.result64) value = (uint32_t)value;
        regs[access.rt] = value;
    }
    if (access.update_base) regs[access.rn] = access.writeback;
    for (unsigned i = 0; i < 29; i++) ss->__x[i] = regs[i];
    ss->__fp = regs[29]; ss->__lr = regs[30]; ss->__sp = regs[31];
    ss->__pc += 4;
    return 1;
}
