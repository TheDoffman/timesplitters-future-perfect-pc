/* ARM64 counterpart of the runtime's x86 VEH register-access handlers.
 * Guest MMIO stays protected. Shared writable aliases avoid changing page
 * protection while another guest thread is running. */
#include "arm64_access.h"
#include "mmio_device.h"
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
    if (!tsfp_mmio_device(host,access.bytes,access.load,&value)) return 0;
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
