/* Linux aarch64 counterpart of the runtime's x86 VEH register-access handlers.
 * Guest MMIO stays protected. Shared writable aliases avoid changing page
 * protection while another guest thread is running. */
#define _GNU_SOURCE 1
#include "arm64_access.h"
#include "mmio_device.h"
#include <signal.h>
#include <sys/ucontext.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;

int tsfp_native_mmio(siginfo_t *info, void *context)
{
    /* Reject everything outside the device windows before reading the
     * instruction: a wild PC belongs to the crash report, not this handler. */
    uintptr_t host = (uintptr_t)info->si_addr;
    if (host < (uintptr_t)g_xbox_mem_offset || host - (uintptr_t)g_xbox_mem_offset > UINT32_MAX) return 0;
    uint32_t va = (uint32_t)(host - (uintptr_t)g_xbox_mem_offset);
    int apu = va >= 0xfe800000u && va < 0xfe830000u;
    int crtc = va >= 0xfd600000u && va < 0xfd604000u;
    int ac97 = va >= 0xfec00000u && va < 0xfec04000u;
    if (!apu && !crtc && !ac97) return 0;
    ucontext_t *uc = context;
    mcontext_t *mc = &uc->uc_mcontext;
    uint64_t regs[32];
    for (unsigned i = 0; i < 31; i++) regs[i] = mc->regs[i];
    regs[31] = mc->sp;
    uint32_t ins;
    memcpy(&ins, (void *)(uintptr_t)mc->pc, sizeof ins);
    NativeAccess access;
    if (!native_decode_access(ins, regs, &access) || access.address != host) return 0;
    uint64_t value = access.rt == 31 ? 0 : regs[access.rt];
    if (access.bytes < 8) value &= (1ull << (access.bytes * 8)) - 1;
    if (!tsfp_mmio_device(host, access.bytes, access.load, &value)) return 0;
    if (access.load && access.rt != 31) {
        if (access.sign_extend) {
            unsigned shift = 64 - access.bytes * 8;
            value = (uint64_t)((int64_t)(value << shift) >> shift);
        }
        if (!access.result64) value = (uint32_t)value;
        regs[access.rt] = value;
    }
    if (access.update_base) regs[access.rn] = access.writeback;
    for (unsigned i = 0; i < 31; i++) mc->regs[i] = regs[i];
    mc->sp = regs[31];
    mc->pc += 4;
    return 1;
}
