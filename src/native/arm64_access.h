/* Decode only scalar AArch64 load/store forms that can arise at guest MMIO.
 * Unknown instructions fail closed; never skip an instruction we cannot model. */
#ifndef TSFP_ARM64_ACCESS_H
#define TSFP_ARM64_ACCESS_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uintptr_t address, writeback;
    unsigned bytes, rt, rn, load, sign_extend, result64, update_base;
} NativeAccess;
static inline int native_decode_access(uint32_t ins, const uint64_t regs[32], NativeAccess *a)
{
    unsigned opc = (ins >> 22) & 3u, scale = ins >> 30;
    *a = (NativeAccess){0};
    a->bytes = 1u << scale; a->rt = ins & 31u; a->rn = (ins >> 5) & 31u;
    if ((ins & 0x3b000000u) == 0x39000000u && !(ins & 0x04000000u)) {
        a->address = regs[a->rn] + ((uint64_t)((ins >> 10) & 4095u) << scale);
    } else if ((ins & 0x3b200000u) == 0x38000000u && !(ins & 0x04000000u)) {
        unsigned mode = (ins >> 10) & 3u;
        if (mode == 2) return 0; /* unprivileged forms are not emitted here */
        int64_t off = (int64_t)((ins >> 12) & 511u);
        if (off & 256) off -= 512;
        a->address = regs[a->rn] + (mode == 1 ? 0 : off);
        a->update_base = mode == 1 || mode == 3;
        a->writeback = regs[a->rn] + off;
        if (a->update_base && a->rn == a->rt && a->rn != 31) return 0;
    } else if ((ins & 0x3b200c00u) == 0x38200800u && !(ins & 0x04000000u)) {
        unsigned rm = (ins >> 16) & 31u, option = (ins >> 13) & 7u;
        uint64_t off = rm == 31 ? 0 : regs[rm];
        switch (option) {
        case 2: off = (uint32_t)off; break; /* UXTW */
        case 3: break;                    /* LSL */
        case 6: off = (uint64_t)(int64_t)(int32_t)off; break; /* SXTW */
        case 7: break;                    /* SXTX */
        default: return 0;
        }
        if (ins & 0x1000u) off <<= scale;
        a->address = regs[a->rn] + off;
    } else return 0;
    if ((scale == 3 && opc >= 2) || (scale == 2 && opc == 3)) return 0;
    a->load = opc != 0; a->sign_extend = opc >= 2;
    a->result64 = scale == 3 || opc == 2;
    return 1;
}
#endif
