/*
 * MCPX APU MMIO Hook - VEH instruction decoder for APU register access
 *
 * Reuses the same x86-64 instruction decoder pattern as nv2a_mmio_hook.c
 * but routes reads/writes through the MCPX APU register handlers.
 */

#include "apu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Global APU state pointer -- referenced from main.c regardless of which
 * platform's MMIO hook is active, so define it before the #if guard. */
MCPXAPUState *g_apu_state = NULL;

/* The MMIO hook is a Win32-VEH x86-64 instruction decoder. On Linux the
 * equivalent goes through sigaction + ucontext_t (Stage 2 / main.c). For
 * now the whole body is Windows-only so apu_emu links on Debian. */
#if defined(_WIN32)
#include <windows.h>
/* getenv: without <stdlib.h> its pointer is truncated to int. */
#include <stdlib.h>

/* APU MMIO base in Xbox VA space */
#define APU_MMIO_BASE  0xFE800000u
#define APU_MMIO_SIZE  0x00080000u  /* 512KB */

/* (g_apu_state is defined above, outside the Win32 guard) */

/* Statistics */
static int g_apu_mmio_read_count = 0;
static int g_apu_mmio_write_count = 0;
static int g_apu_mmio_decode_fail = 0;

/* ============================================================
 * x86-64 register access helpers (same as NV2A hook)
 * ============================================================ */

static uint64_t *ctx_reg64(PCONTEXT ctx, int reg)
{
    switch (reg & 0xF) {
    case 0:  return (uint64_t*)&ctx->Rax;
    case 1:  return (uint64_t*)&ctx->Rcx;
    case 2:  return (uint64_t*)&ctx->Rdx;
    case 3:  return (uint64_t*)&ctx->Rbx;
    case 4:  return (uint64_t*)&ctx->Rsp;
    case 5:  return (uint64_t*)&ctx->Rbp;
    case 6:  return (uint64_t*)&ctx->Rsi;
    case 7:  return (uint64_t*)&ctx->Rdi;
    case 8:  return (uint64_t*)&ctx->R8;
    case 9:  return (uint64_t*)&ctx->R9;
    case 10: return (uint64_t*)&ctx->R10;
    case 11: return (uint64_t*)&ctx->R11;
    case 12: return (uint64_t*)&ctx->R12;
    case 13: return (uint64_t*)&ctx->R13;
    case 14: return (uint64_t*)&ctx->R14;
    case 15: return (uint64_t*)&ctx->R15;
    default: return NULL;
    }
}

static int decode_modrm_len(const uint8_t *ip, int has_rex_b)
{
    uint8_t modrm = *ip;
    int mod = (modrm >> 6) & 3;
    int rm = (modrm & 7) | (has_rex_b ? 8 : 0);
    int len = 1;

    if (mod == 3) return 1;
    if ((rm & 7) == 4) {
        len += 1;                               /* SIB */
        /* mod==0 with SIB.base==5 has no base register and a disp32 instead.
         * Missing it under-counted the instruction length, so RIP resumed
         * inside the operand. */
        if (mod == 0 && (ip[1] & 7) == 5) len += 4;
    }
    if (mod == 0 && (rm & 7) == 5) len += 4; /* disp32, no SIB */
    else if (mod == 1) len += 1;
    else if (mod == 2) len += 4;
    return len;
}

/* ============================================================
 * Instruction decoder for APU MMIO access
 * ============================================================ */

static bool apu_decode_and_handle(PCONTEXT ctx, uint32_t mmio_offset, int is_write)
{
    const uint8_t *ip = (const uint8_t *)ctx->Rip;
    if (!g_apu_state) return false;

    int prefix_len = 0;
    int has_66 = 0;
    int rex = 0, has_rex = 0;

    while (1) {
        uint8_t b = ip[prefix_len];
        if (b == 0x66) { has_66 = 1; prefix_len++; }
        else if (b == 0xF2 || b == 0xF3) { prefix_len++; }
        else if (b >= 0x40 && b <= 0x4F) { rex = b; has_rex = 1; prefix_len++; }
        else break;
    }

    int rex_w = has_rex && (rex & 0x08);
    int rex_r = has_rex && (rex & 0x04);
    int rex_b = has_rex && (rex & 0x01);

    const uint8_t *opcode = ip + prefix_len;
    int access_size = 4;
    if (has_66) access_size = 2;
    if (rex_w) access_size = 8;

    /* MOV r/m, r (write: 88/89) */
    if (opcode[0] == 0x89 || opcode[0] == 0x88) {
        if (opcode[0] == 0x88) access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t val = *ctx_reg64(ctx, reg);
        mcpx_apu_mmio_write(g_apu_state, mmio_offset, val, access_size);
        ctx->Rip += prefix_len + 1 + modrm_len;
        g_apu_mmio_write_count++;
        return true;
    }

    /* MOV r/m, imm (write: C7 /0)
     *
     * The immediate is 16-bit under a 0x66 prefix and 32-bit otherwise -- it
     * is never 64-bit, even with REX.W, which sign-extends imm32. Reading 4
     * bytes for the 16-bit form took two bytes of the next instruction as
     * part of the value and then resumed RIP two bytes past it. */
    if (opcode[0] == 0xC7) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int imm_len = has_66 ? 2 : 4;
        uint32_t imm = has_66
            ? *(const uint16_t *)(opcode + 1 + modrm_len)
            : *(const uint32_t *)(opcode + 1 + modrm_len);
        mcpx_apu_mmio_write(g_apu_state, mmio_offset, imm, access_size);
        ctx->Rip += prefix_len + 1 + modrm_len + imm_len;
        g_apu_mmio_write_count++;
        return true;
    }

    /* MOV r/m8, imm8 (write: C6 /0) */
    if (opcode[0] == 0xC6) {
        access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        uint8_t imm = *(opcode + 1 + modrm_len);
        mcpx_apu_mmio_write(g_apu_state, mmio_offset, imm, 1);
        ctx->Rip += prefix_len + 1 + modrm_len + 1;
        g_apu_mmio_write_count++;
        return true;
    }

    /* MOV r, r/m (read: 8A/8B) */
    if (opcode[0] == 0x8B || opcode[0] == 0x8A) {
        if (opcode[0] == 0x8A) access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, access_size);
        uint64_t *dst = ctx_reg64(ctx, reg);
        if (access_size == 1) *dst = (*dst & ~0xFFULL) | (val & 0xFF);
        else if (access_size == 2) *dst = (*dst & ~0xFFFFULL) | (val & 0xFFFF);
        else if (access_size == 4) *dst = val & 0xFFFFFFFF;
        else *dst = val;
        ctx->Rip += prefix_len + 1 + modrm_len;
        g_apu_mmio_read_count++;
        return true;
    }

    /* MOVZX r32, r/m8 (0F B6) */
    if (opcode[0] == 0x0F && opcode[1] == 0xB6) {
        access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 2, rex_b);
        int reg = ((opcode[2] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, 1);
        *ctx_reg64(ctx, reg) = val & 0xFF;
        ctx->Rip += prefix_len + 2 + modrm_len;
        g_apu_mmio_read_count++;
        return true;
    }

    /* MOVZX r32, r/m16 (0F B7) */
    if (opcode[0] == 0x0F && opcode[1] == 0xB7) {
        access_size = 2;
        int modrm_len = decode_modrm_len(opcode + 2, rex_b);
        int reg = ((opcode[2] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, 2);
        *ctx_reg64(ctx, reg) = val & 0xFFFF;
        ctx->Rip += prefix_len + 2 + modrm_len;
        g_apu_mmio_read_count++;
        return true;
    }

    /* TEST r/m, r (84/85) - read */
    if (opcode[0] == 0x85 || opcode[0] == 0x84) {
        if (opcode[0] == 0x84) access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t mem_val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, access_size);
        uint64_t reg_val = *ctx_reg64(ctx, reg);
        uint64_t result = mem_val & reg_val;
        ctx->EFlags &= ~(0x0001 | 0x0040 | 0x0080 | 0x0800);
        if (result == 0) ctx->EFlags |= 0x0040;
        if (result & (1ULL << (access_size * 8 - 1))) ctx->EFlags |= 0x0080;
        ctx->Rip += prefix_len + 1 + modrm_len;
        g_apu_mmio_read_count++;
        return true;
    }

    /* CMP r/m, r (38/39) */
    if (opcode[0] == 0x39 || opcode[0] == 0x38) {
        if (opcode[0] == 0x38) access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t mem_val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, access_size);
        uint64_t reg_val = *ctx_reg64(ctx, reg);
        if (access_size <= 4) {
            mem_val &= (1ULL << (access_size * 8)) - 1;
            reg_val &= (1ULL << (access_size * 8)) - 1;
        }
        uint64_t result = mem_val - reg_val;
        ctx->EFlags &= ~(0x0001 | 0x0040 | 0x0080 | 0x0800);
        if (result == 0) ctx->EFlags |= 0x0040;
        if (mem_val < reg_val) ctx->EFlags |= 0x0001;
        if (result & (1ULL << (access_size * 8 - 1))) ctx->EFlags |= 0x0080;
        ctx->Rip += prefix_len + 1 + modrm_len;
        g_apu_mmio_read_count++;
        return true;
    }

    /* OR r/m, r (08/09) */
    if (opcode[0] == 0x09 || opcode[0] == 0x08) {
        if (opcode[0] == 0x08) access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t mem_val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, access_size);
        uint64_t reg_val = *ctx_reg64(ctx, reg);
        mcpx_apu_mmio_write(g_apu_state, mmio_offset, mem_val | reg_val, access_size);
        ctx->Rip += prefix_len + 1 + modrm_len;
        g_apu_mmio_write_count++;
        return true;
    }

    /* AND r/m, r (20/21) */
    if (opcode[0] == 0x21 || opcode[0] == 0x20) {
        if (opcode[0] == 0x20) access_size = 1;
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t mem_val = mcpx_apu_mmio_read(g_apu_state, mmio_offset, access_size);
        uint64_t reg_val = *ctx_reg64(ctx, reg);
        mcpx_apu_mmio_write(g_apu_state, mmio_offset, mem_val & reg_val, access_size);
        ctx->Rip += prefix_len + 1 + modrm_len;
        g_apu_mmio_write_count++;
        return true;
    }

    /* Unrecognized */
    g_apu_mmio_decode_fail++;
    if (g_apu_mmio_decode_fail <= 20) {
        fprintf(stderr, "[APU] MMIO decode fail at RIP=%p offset=0x%X: %02X %02X %02X %02X %02X %02X\n",
                (void*)ctx->Rip, mmio_offset, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5]);
        fflush(stderr);
    }
    return false;
}

/* ============================================================
 * AC'97 page: writes trap, reads do not
 * ============================================================ */

/* The AC'97 bus-master control registers, as offsets from the MCPX base.
 *
 * These are NABM DMA-engine register blocks at 0xFEC00100 and 0xFEC00160:
 * the guest writes the buffer-descriptor base at engine+0x00 and reads status
 * at +0x06 and the position counter at +0x08, so +0x0B is CR, the control
 * register, and bit 1 is RR -- Reset Registers.
 *
 * RR is architecturally self-clearing: software sets it, the hardware clears
 * it when the reset finishes. So masking it out on the way in is the correct
 * model of the part, not a workaround. DirectSound relies on that exactly,
 * reading the byte back four instructions later and spinning on the register
 * copy, so it never sees a later change:
 *
 *     mov cl, byte [dsp];  and cl, 2
 *   loop:
 *     test cl, cl;  jne loop
 *
 * On hardware the write latches a command and the flag drops with it, so the
 * read already sees zero. A thread clearing the byte from outside cannot win
 * that race, and measurably does not. Catching the write is what makes it
 * deterministic: the page is mapped PAGE_READONLY, so a write faults here and
 * a read stays plain memory at full speed.
 *
 * Dropping the bit is the whole model. There is no DSP to run the command,
 * and the title reads this byte only to find out whether it may proceed.
 */
#define MCPX_AC97_BM0_CR     0x0040011Bu   /* 0xFEC0011B, engine 0 */
#define MCPX_AC97_BM1_CR     0x0040017Bu   /* 0xFEC0017B, engine 1 */
#define MCPX_AC97_CR_RR      0x02u         /* Reset Registers, self-clearing */

static uint64_t ac97_apply_mask(uint32_t mcpx_offset, uint64_t value)
{
    if (mcpx_offset == MCPX_AC97_BM0_CR || mcpx_offset == MCPX_AC97_BM1_CR)
        return value & ~(uint64_t)MCPX_AC97_CR_RR;
    return value;
}

/* Put the value in the page, which is read-only to everyone including us.
 *
 * Three things here are deliberate, and the first two were wrong before.
 *
 * The restore is to PAGE_READONLY by name, not to whatever VirtualProtect
 * handed back. Two threads in here at once and the second captures
 * PAGE_READWRITE as the "old" protection, so its restore leaves the page
 * writable and the trap is silently dead for the rest of the run.
 *
 * The lock serialises the window. Guest threads are real OS threads
 * (PsCreateSystemThreadEx creates them), so two can reach this together, and
 * during any writable window a second thread's write to the control register
 * lands unmasked -- which is exactly the hang this exists to prevent.
 *
 * And the protection is applied to the whole containing page, because that is
 * the granularity the API works at regardless of the size asked for. A store
 * that would straddle the end of the page is refused rather than dragging the
 * neighbouring page's protection with it.
 */
static CRITICAL_SECTION s_ac97_cs;
static LONG s_ac97_cs_ready;

static void ac97_lock_init(void)
{
    if (InterlockedCompareExchange(&s_ac97_cs_ready, 1, 0) == 0) {
        InitializeCriticalSection(&s_ac97_cs);
        InterlockedExchange(&s_ac97_cs_ready, 2);
    }
    while (InterlockedCompareExchange(&s_ac97_cs_ready, 2, 2) != 2)
        Sleep(0);
}

static int ac97_store(void *dst, uint64_t value, int size)
{
    void *page = (void *)((uintptr_t)dst & ~(uintptr_t)0xFFF);
    size_t off  = (uintptr_t)dst & 0xFFF;
    DWORD old;
    int ok = 1;

    if (off + (size_t)size > 0x1000)
        return 0;                          /* straddles the page: not ours */

    ac97_lock_init();
    EnterCriticalSection(&s_ac97_cs);

    if (VirtualProtect(page, 0x1000, PAGE_READWRITE, &old)) {
        switch (size) {
        case 1: *(volatile uint8_t  *)dst = (uint8_t)value;  break;
        case 2: *(volatile uint16_t *)dst = (uint16_t)value; break;
        case 8: *(volatile uint64_t *)dst = value;           break;
        default: *(volatile uint32_t *)dst = (uint32_t)value; break;
        }
        VirtualProtect(page, 0x1000, PAGE_READONLY, &old);
    } else {
        ok = 0;
    }

    LeaveCriticalSection(&s_ac97_cs);
    return ok;
}

/* ============================================================
 * NV2A interrupt status: write-1-to-clear
 * ============================================================ */

/* Defined below, next to the AC'97 handler it is shared with. */
static int mmio_decode_write_value(PCONTEXT ctx, uint64_t *out, int *out_size);
static int ac97_store(void *dst, uint64_t value, int size);

/* The registers a guest ISR acknowledges through, and the summary bit.
 *
 * PMC_INTR_0 bit 24 is not storage: it reports that the CRTC has something
 * pending, and it goes away when PCRTC_INTR_0 does. A driver acknowledges by
 * writing the bits it handled to PCRTC_INTR_0 -- write-1-to-clear -- and then
 * waits for the summary to drop.
 *
 * Burnout 2's D3D8 does exactly that, and against plain memory it cannot ever
 * finish:
 *
 *     0x0021D0D0:  mov  [edi + 0x600100], ecx      ; ecx = 1, the vblank bit
 *     0x0021D0D6:  test [edi + 0x100], 0x1000000
 *     0x0021D0DC:  jne  0x21d0d0                   ; spin
 *
 * The store put 1 into PCRTC_INTR_0 rather than clearing bit 0, PMC_INTR_0
 * kept its summary bit, and the loop never exited. That loop runs on the DPC,
 * which this runtime dispatches from the timer thread -- the same thread that
 * raises the next vblank. So one frame was delivered, the thread died inside
 * the acknowledgement, and nothing ever ticked again. The title's registered
 * vertical-blank callback never ran, and its asset loader waits on a counter
 * that callback feeds.
 *
 * Implementing the real semantics is what fixes it, and it is less code than
 * the workarounds: clear the written bits, then recompute the summary.
 */
#define NV2A_PMC_INTR_0_OFF      0x000100u
#define NV2A_PMC_INTR_PCRTC_BIT  0x01000000u
#define NV2A_PCRTC_INTR_0_OFF    0x600100u

int nv2a_intr_handle_write(PCONTEXT ctx, uintptr_t host_addr,
                           uint32_t nv2a_offset, uintptr_t aperture)
{
    volatile uint32_t *pmc =
        (volatile uint32_t *)(aperture + NV2A_PMC_INTR_0_OFF);
    volatile uint32_t *pcrtc =
        (volatile uint32_t *)(aperture + NV2A_PCRTC_INTR_0_OFF);
    uint64_t written;
    int size;

    if (!mmio_decode_write_value(ctx, &written, &size))
        return 0;

    if (nv2a_offset == NV2A_PCRTC_INTR_0_OFF) {
        uint32_t remaining = *pcrtc & ~(uint32_t)written;
        if (!ac97_store((void *)host_addr, remaining, 4))
            return 0;
        /* The summary follows the unit, and nothing else owns that bit. */
        if (remaining)
            *pmc |= NV2A_PMC_INTR_PCRTC_BIT;
        else
            *pmc &= ~NV2A_PMC_INTR_PCRTC_BIT;
        return 1;
    }

    if (nv2a_offset == NV2A_PMC_INTR_0_OFF) {
        /* PMC_INTR_0 is read-only status on hardware. A driver that writes it
         * is acknowledging, so drop the written bits rather than storing
         * them. */
        uint32_t remaining = *pmc & ~(uint32_t)written;
        return ac97_store((void *)host_addr, remaining, 4);
    }

    /* Any other register on the page: a plain store, so the page behaves as
     * memory for everything this does not model. */
    return ac97_store((void *)host_addr, written, size);
}

/* Decode the value and width a faulting store was going to write.
 *
 * Shared by the AC'97 and NV2A handlers, which differ only in what they do
 * with the value. Returns 0 for a form the decoder does not know, and the
 * caller must then let the fault through -- silently skipping an instruction
 * whose width is a guess is how a wrong RIP happens.
 */
static int mmio_decode_write_value(PCONTEXT ctx, uint64_t *out, int *out_size)
{
    const uint8_t *ip = (const uint8_t *)ctx->Rip;
    int prefix_len = 0, has_66 = 0, rex = 0, has_rex = 0;
    int rex_w, rex_r, rex_b, size;
    const uint8_t *opcode;

    while (1) {
        uint8_t b = ip[prefix_len];
        if (b == 0x66) { has_66 = 1; prefix_len++; }
        else if (b == 0xF2 || b == 0xF3) { prefix_len++; }
        else if (b >= 0x40 && b <= 0x4F) { rex = b; has_rex = 1; prefix_len++; }
        else break;
    }
    rex_w = has_rex && (rex & 0x08);
    rex_r = has_rex && (rex & 0x04);
    rex_b = has_rex && (rex & 0x01);

    opcode = ip + prefix_len;
    size = 4;
    if (has_66) size = 2;
    if (rex_w)  size = 8;

    if (opcode[0] == 0x89 || opcode[0] == 0x88) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        if (opcode[0] == 0x88) size = 1;
        *out = *ctx_reg64(ctx, reg);
        *out_size = size;
        ctx->Rip += prefix_len + 1 + modrm_len;
        return 1;
    }
    if (opcode[0] == 0xC7) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int imm_len = has_66 ? 2 : 4;
        *out = has_66 ? *(const uint16_t *)(opcode + 1 + modrm_len)
                      : *(const uint32_t *)(opcode + 1 + modrm_len);
        *out_size = size;
        ctx->Rip += prefix_len + 1 + modrm_len + imm_len;
        return 1;
    }
    if (opcode[0] == 0xC6) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        *out = *(opcode + 1 + modrm_len);
        *out_size = 1;
        ctx->Rip += prefix_len + 1 + modrm_len + 1;
        return 1;
    }
    return 0;
}

int mcpx_ac97_handle_write(PCONTEXT ctx, uintptr_t host_addr, uint32_t mcpx_offset)
{
    const uint8_t *ip = (const uint8_t *)ctx->Rip;
    int prefix_len = 0, has_66 = 0, rex = 0, has_rex = 0;
    int rex_w, rex_r, rex_b, access_size;
    const uint8_t *opcode;

    while (1) {
        uint8_t b = ip[prefix_len];
        if (b == 0x66) { has_66 = 1; prefix_len++; }
        else if (b == 0xF2 || b == 0xF3) { prefix_len++; }
        else if (b >= 0x40 && b <= 0x4F) { rex = b; has_rex = 1; prefix_len++; }
        else break;
    }
    rex_w = has_rex && (rex & 0x08);
    rex_r = has_rex && (rex & 0x04);
    rex_b = has_rex && (rex & 0x01);

    opcode = ip + prefix_len;
    access_size = 4;
    if (has_66) access_size = 2;
    if (rex_w)  access_size = 8;

    /* MOV r/m, r */
    if (opcode[0] == 0x89 || opcode[0] == 0x88) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int reg = ((opcode[1] >> 3) & 7) | (rex_r ? 8 : 0);
        uint64_t val = *ctx_reg64(ctx, reg);
        if (opcode[0] == 0x88) access_size = 1;
        if (!ac97_store((void *)host_addr,
                        ac97_apply_mask(mcpx_offset, val), access_size))
            return 0;
        ctx->Rip += prefix_len + 1 + modrm_len;
        return 1;
    }

    /* MOV r/m, imm -- 16-bit immediate under a 0x66 prefix, else 32-bit */
    if (opcode[0] == 0xC7) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int imm_len = has_66 ? 2 : 4;
        uint32_t imm = has_66
            ? *(const uint16_t *)(opcode + 1 + modrm_len)
            : *(const uint32_t *)(opcode + 1 + modrm_len);
        if (!ac97_store((void *)host_addr,
                        ac97_apply_mask(mcpx_offset, imm), access_size))
            return 0;
        ctx->Rip += prefix_len + 1 + modrm_len + imm_len;
        return 1;
    }

    /* Group 1 on a byte: op r/m8, imm8 (80 /r).
     *
     * The guest clears the register with `and byte [cr], 0`, which lands here
     * rather than in the MOV forms. It only reached the MOV path at all
     * because MEM8 is volatile, so MSVC could not fuse the lifted read and
     * write into one RMW -- a compiler that did would have crashed on an
     * unhandled form. Reading is allowed on this page, so a read-modify-write
     * is straightforward. */
    if (opcode[0] == 0x80) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        int op = (opcode[1] >> 3) & 7;
        uint8_t imm = *(opcode + 1 + modrm_len);
        uint8_t cur = *(const volatile uint8_t *)host_addr;
        uint8_t val;

        switch (op) {
        case 0: val = (uint8_t)(cur + imm); break;   /* ADD */
        case 1: val = (uint8_t)(cur | imm); break;   /* OR  */
        case 4: val = (uint8_t)(cur & imm); break;   /* AND */
        case 5: val = (uint8_t)(cur - imm); break;   /* SUB */
        case 6: val = (uint8_t)(cur ^ imm); break;   /* XOR */
        case 7: return 0;                            /* CMP writes nothing */
        default: return 0;                           /* ADC/SBB need the carry */
        }
        if (!ac97_store((void *)host_addr,
                        ac97_apply_mask(mcpx_offset, val), 1))
            return 0;
        ctx->Rip += prefix_len + 1 + modrm_len + 1;
        return 1;
    }

    /* MOV r/m8, imm8 -- how the control register is normally set */
    if (opcode[0] == 0xC6) {
        int modrm_len = decode_modrm_len(opcode + 1, rex_b);
        uint8_t imm = *(opcode + 1 + modrm_len);
        if (!ac97_store((void *)host_addr,
                        ac97_apply_mask(mcpx_offset, imm), 1))
            return 0;
        ctx->Rip += prefix_len + 1 + modrm_len + 1;
        return 1;
    }

    {
        static unsigned said;
        if (said++ < 20) {
            fprintf(stderr, "[AC97] write decode fail at RIP=%p offset=0x%X: "
                    "%02X %02X %02X %02X %02X %02X\n",
                    (void *)ctx->Rip, mcpx_offset,
                    ip[0], ip[1], ip[2], ip[3], ip[4], ip[5]);
            fflush(stderr);
        }
    }
    return 0;
}

/* ============================================================
 * Public API (called from VEH in main.c)
 * ============================================================ */

int apu_hook_handle_mmio(PCONTEXT ctx, uintptr_t fault_addr,
                         uint32_t fault_xbox_va, int is_write)
{
    uint32_t mmio_offset = fault_xbox_va - APU_MMIO_BASE;
    int ok = apu_decode_and_handle(ctx, mmio_offset, is_write) ? 1 : 0;

    /* What the title actually asks the APU for. The DSPs are stubbed here, so
     * a title that waits on one waits forever, and the only way to work out
     * what it is waiting for is to see the register traffic that precedes the
     * wait. */
    /* Read once: this runs for every trapped APU access, and an uncached
     * getenv here was 14% of Outrun 2's main thread in a race. */
    static int trace = -1;
    if (trace < 0)
        trace = getenv("RECOMP_APU_TRACE") ? 1 : 0;
    if (trace) {
        static unsigned n;
        if (n++ < 400) {
            /* The value as well as the offset: finding which register carries
             * the command-block address means recognising the address when it
             * goes past, and an offset alone never shows it. */
            uint64_t v = g_apu_state
                       ? mcpx_apu_mmio_read(g_apu_state, mmio_offset, 4) : 0;
            fprintf(stderr, "  [APUMMIO] %s 0x%05X = %08X%s\n",
                    is_write ? "write" : "read ", mmio_offset,
                    (uint32_t)v, ok ? "" : "  (decode failed)");
        }
    }
    return ok;
}

#endif /* _WIN32 */
